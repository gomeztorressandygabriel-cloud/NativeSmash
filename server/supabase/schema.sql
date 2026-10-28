-- NativeSmash's players server on Supabase: accounts, who is online, invitations to play and quick match.
--
-- Paste the whole file into the Supabase project's SQL Editor and run it (it can be run again: it keeps what is there).
-- The launcher and the web page talk to it with the project's URL and its publishable ("anon") key only; every table is
-- closed (row level security) and is reached through the functions below, which check who is asking (auth.uid()).
--
-- How a game is arranged:
--   * Each launcher calls heartbeat() every 20 s while it is open: it is "online" for 60 s after the last one.
--   * Invite: A calls send_invite(B) and gets a room code; B sees it in my_invites() and calls answer_invite(id, true);
--     A sees "accepted" in invite_status(id). A's launcher makes the room with that code (host), B's joins it.
--   * Quick match: quick_match() either pairs the caller with the longest-waiting player of the same netcode (both get
--     the same code: the one who was waiting hosts) or puts the caller in the queue; quick_status() says when paired.

-- ---- accounts ---------------------------------------------------------------------------------------------------------
create table if not exists public.profiles (
  id uuid primary key references auth.users (id) on delete cascade,
  username text not null unique check (char_length(username) between 3 and 20 and username ~ '^[A-Za-z0-9_.-]+$'),
  created_at timestamptz not null default now(),
  -- (the ranking, later: filled from the uploaded match records)
  rating integer not null default 1000,
  wins integer not null default 0,
  losses integer not null default 0
);
alter table public.profiles enable row level security;
drop policy if exists "profiles are public" on public.profiles;
create policy "profiles are public" on public.profiles for select using (true);   -- (the ranking page reads them)

-- A new account's profile, with the username given at sign-up (options.data.username).
create or replace function public.on_new_user() returns trigger
language plpgsql security definer set search_path = public as $$
declare
  wanted text := coalesce(nullif(trim(new.raw_user_meta_data ->> 'username'), ''), 'player');
  name text := wanted;
  n integer := 1;
begin
  while exists (select 1 from public.profiles where lower(username) = lower(name)) loop
    n := n + 1;
    name := left(wanted, 16) || n::text;
  end loop;
  insert into public.profiles (id, username) values (new.id, name);
  return new;
end $$;
drop trigger if exists on_auth_user_created on auth.users;
create trigger on_auth_user_created after insert on auth.users for each row execute function public.on_new_user();

-- Is this username free? (the sign-up form asks before sending)
create or replace function public.username_free(name text) returns boolean
language sql stable security definer set search_path = public as $$
  select not exists (select 1 from public.profiles where lower(username) = lower(trim(name)));
$$;

-- ---- who is online ----------------------------------------------------------------------------------------------------
create table if not exists public.presence (
  user_id uuid primary key references public.profiles (id) on delete cascade,
  state text not null default 'launcher' check (state in ('launcher', 'searching', 'playing')),
  version text,
  last_seen timestamptz not null default now()
);
alter table public.presence enable row level security;   -- (no policies: reached through the functions only)

create or replace function public.heartbeat(state text default 'launcher', version text default null) returns void
language plpgsql security definer set search_path = public as $$
begin
  if auth.uid() is null then raise exception 'not signed in'; end if;
  if state not in ('launcher', 'searching', 'playing') then state := 'launcher'; end if;
  insert into public.presence (user_id, state, version, last_seen) values (auth.uid(), state, version, now())
  on conflict (user_id) do update set state = excluded.state, version = excluded.version, last_seen = now();
end $$;

-- How many are online (the web page shows it; anyone may ask).
create or replace function public.online_count() returns integer
language sql stable security definer set search_path = public as $$
  select count(*)::integer from public.presence where last_seen > now() - interval '60 seconds';
$$;

-- The players online but the caller, for the launcher's list.
create or replace function public.online_players() returns table (user_id uuid, username text, state text, rating integer)
language sql stable security definer set search_path = public as $$
  select p.user_id, f.username, p.state, f.rating
  from public.presence p join public.profiles f on f.id = p.user_id
  where p.last_seen > now() - interval '60 seconds' and p.user_id <> auth.uid()
  order by (p.state = 'playing'), f.username
  limit 200;
$$;

-- ---- invitations ------------------------------------------------------------------------------------------------------
create table if not exists public.invites (
  id bigint generated always as identity primary key,
  from_user uuid not null references public.profiles (id) on delete cascade,
  to_user uuid not null references public.profiles (id) on delete cascade,
  room_code text not null,
  netcode text not null default 'rollback' check (netcode in ('rollback', 'normal')),
  status text not null default 'pending' check (status in ('pending', 'accepted', 'declined', 'cancelled')),
  created_at timestamptz not null default now(),
  answered_at timestamptz
);
create index if not exists invites_to on public.invites (to_user, status);
alter table public.invites enable row level security;

create or replace function public.new_room_code() returns text
language sql volatile as $$
  select string_agg(substr('ABCDEFGHJKMNPQRSTUVWXYZ23456789', 1 + floor(random() * 31)::integer, 1), '')
  from generate_series(1, 6);
$$;

-- A invites B: the invitation lasts 60 s.
create or replace function public.send_invite(to_user uuid, netcode text default 'rollback')
returns table (id bigint, room_code text)
language plpgsql security definer set search_path = public as $$
#variable_conflict use_column
declare inv public.invites;
begin
  if auth.uid() is null then raise exception 'not signed in'; end if;
  if to_user = auth.uid() then raise exception 'not yourself'; end if;
  if not exists (select 1 from public.presence where user_id = send_invite.to_user and last_seen > now() - interval '60 seconds') then
    raise exception 'that player is not online';
  end if;
  if (select count(*) from public.invites where from_user = auth.uid() and created_at > now() - interval '1 minute') >= 10 then
    raise exception 'too many invitations: wait a moment';
  end if;
  update public.invites set status = 'cancelled' where from_user = auth.uid() and status = 'pending';
  insert into public.invites (from_user, to_user, room_code, netcode)
  values (auth.uid(), send_invite.to_user, public.new_room_code(), case when send_invite.netcode = 'normal' then 'normal' else 'rollback' end)
  returning * into inv;
  return query select inv.id, inv.room_code;
end $$;

-- The invitations waiting for the caller (the last 60 s).
create or replace function public.my_invites() returns table (id bigint, from_user uuid, username text, netcode text, created_at timestamptz)
language sql stable security definer set search_path = public as $$
  select i.id, i.from_user, f.username, i.netcode, i.created_at
  from public.invites i join public.profiles f on f.id = i.from_user
  where i.to_user = auth.uid() and i.status = 'pending' and i.created_at > now() - interval '60 seconds'
  order by i.created_at desc;
$$;

-- B answers: accepted, it gets the room code to join.
create or replace function public.answer_invite(invite_id bigint, accept boolean)
returns table (room_code text, netcode text)
language plpgsql security definer set search_path = public as $$
#variable_conflict use_column
declare inv public.invites;
begin
  update public.invites set status = case when accept then 'accepted' else 'declined' end, answered_at = now()
  where invites.id = invite_id and to_user = auth.uid() and status = 'pending' and created_at > now() - interval '60 seconds'
  returning * into inv;
  if inv.id is null then raise exception 'that invitation is no longer there'; end if;
  if accept then return query select inv.room_code, inv.netcode; end if;
end $$;

-- A asks how its invitation went.
create or replace function public.invite_status(invite_id bigint) returns text
language sql stable security definer set search_path = public as $$
  select case when status = 'pending' and created_at <= now() - interval '60 seconds' then 'expired' else status end
  from public.invites where id = invite_id and from_user = auth.uid();
$$;

create or replace function public.cancel_invite(invite_id bigint) returns void
language sql security definer set search_path = public as $$
  update public.invites set status = 'cancelled' where id = invite_id and from_user = auth.uid() and status = 'pending';
$$;

-- ---- quick match ------------------------------------------------------------------------------------------------------
create table if not exists public.queue (
  user_id uuid primary key references public.profiles (id) on delete cascade,
  netcode text not null default 'rollback',
  created_at timestamptz not null default now(),
  -- filled when paired: the room, and whether this player hosts it
  room_code text,
  opponent uuid references public.profiles (id) on delete set null,
  hosts boolean
);
alter table public.queue enable row level security;

-- Pair with the longest-waiting player of the same netcode, or wait in the queue. Paired: the room code and the role.
create or replace function public.quick_match(netcode text default 'rollback')
returns table (status text, room_code text, hosts boolean, opponent text)
language plpgsql security definer set search_path = public as $$
#variable_conflict use_column
declare
  me uuid := auth.uid();
  other public.queue;
  code text;
  nc text := case when quick_match.netcode = 'normal' then 'normal' else 'rollback' end;
begin
  if me is null then raise exception 'not signed in'; end if;
  delete from public.queue q where q.created_at < now() - interval '5 minutes' and q.room_code is null;
  delete from public.queue q where q.user_id = me;
  select * into other from public.queue q
  where q.netcode = nc and q.room_code is null and q.user_id <> me
    and exists (select 1 from public.presence p where p.user_id = q.user_id and p.last_seen > now() - interval '60 seconds')
  order by q.created_at limit 1 for update skip locked;
  if other.user_id is null then
    insert into public.queue (user_id, netcode) values (me, nc);
    return query select 'waiting'::text, null::text, null::boolean, null::text;
    return;
  end if;
  code := public.new_room_code();
  update public.queue set room_code = code, opponent = me, hosts = true where user_id = other.user_id;
  insert into public.queue (user_id, netcode, room_code, opponent, hosts) values (me, nc, code, other.user_id, false);
  return query select 'paired'::text, code, false, (select username from public.profiles where id = other.user_id);
end $$;

-- While waiting: paired yet?
create or replace function public.quick_status()
returns table (status text, room_code text, hosts boolean, opponent text)
language sql stable security definer set search_path = public as $$
  select case when q.room_code is null then 'waiting' else 'paired' end, q.room_code, q.hosts, f.username
  from public.queue q left join public.profiles f on f.id = q.opponent
  where q.user_id = auth.uid();
$$;

create or replace function public.quick_cancel() returns void
language sql security definer set search_path = public as $$
  delete from public.queue where user_id = auth.uid();
$$;

create or replace function public.go_offline() returns void
language sql security definer set search_path = public as $$
  delete from public.presence where user_id = auth.uid();
  delete from public.queue where user_id = auth.uid();
$$;

-- ---- who may call what ------------------------------------------------------------------------------------------------
revoke all on function public.heartbeat(text, text), public.go_offline(), public.online_players(), public.send_invite(uuid, text),
  public.my_invites(), public.answer_invite(bigint, boolean), public.invite_status(bigint), public.cancel_invite(bigint),
  public.quick_match(text), public.quick_status(), public.quick_cancel() from public, anon;
grant execute on function public.heartbeat(text, text), public.go_offline(), public.online_players(), public.send_invite(uuid, text),
  public.my_invites(), public.answer_invite(bigint, boolean), public.invite_status(bigint), public.cancel_invite(bigint),
  public.quick_match(text), public.quick_status(), public.quick_cancel() to authenticated;
grant execute on function public.online_count(), public.username_free(text) to anon, authenticated;
revoke all on function public.on_new_user(), public.new_room_code() from public, anon, authenticated;
