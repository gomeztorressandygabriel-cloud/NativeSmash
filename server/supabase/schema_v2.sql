-- NativeSmash's players server, part 2 (run after schema.sql; it can be run again): friends, the matches the server arranges and
-- their results (the ranking's points, Elo), quick match by level, the players' profiles and history, a name change and an avatar
-- colour.
--
-- Ranked results: every invitation accepted and every quick match makes a match; after the game each player says how it went
-- (report_result: win, loss or none). When the two answers agree (one won, the other lost) the points move (Elo, K = 32) and the
-- win and the loss are counted; both "none": no match; anything else: disputed, nothing moves.

-- ---- profiles: an avatar colour -------------------------------------------------------------------------------------------
alter table public.profiles add column if not exists avatar text check (avatar is null or avatar ~ '^#[0-9A-Fa-f]{6}$');

create or replace function public.set_avatar(color text) returns void
language plpgsql security definer set search_path = public as $$
begin
  if auth.uid() is null then raise exception 'not signed in'; end if;
  if color is not null and color !~ '^#[0-9A-Fa-f]{6}$' then raise exception 'not a colour'; end if;
  update public.profiles set avatar = color where id = auth.uid();
end $$;

create or replace function public.set_username(name text) returns void
language plpgsql security definer set search_path = public as $$
begin
  if auth.uid() is null then raise exception 'not signed in'; end if;
  name := trim(name);
  if name !~ '^[A-Za-z0-9_.-]{3,20}$' then raise exception 'bad name'; end if;
  if exists (select 1 from public.profiles where lower(username) = lower(name) and id <> auth.uid()) then raise exception 'name taken'; end if;
  update public.profiles set username = name where id = auth.uid();
end $$;

-- ---- friends ------------------------------------------------------------------------------------------------------------
create table if not exists public.friends (
  a uuid not null references public.profiles (id) on delete cascade,   -- (who asked)
  b uuid not null references public.profiles (id) on delete cascade,
  status text not null default 'pending' check (status in ('pending', 'accepted')),
  created_at timestamptz not null default now(),
  primary key (a, b),
  check (a <> b)
);
alter table public.friends enable row level security;

create or replace function public.friend_request(name text) returns text
language plpgsql security definer set search_path = public as $$
declare other uuid;
begin
  if auth.uid() is null then raise exception 'not signed in'; end if;
  select id into other from public.profiles where lower(username) = lower(trim(name));
  if other is null then raise exception 'no such player'; end if;
  if other = auth.uid() then raise exception 'not yourself'; end if;
  if exists (select 1 from public.friends where a = other and b = auth.uid()) then   -- (they had asked: that is a yes)
    update public.friends set status = 'accepted' where a = other and b = auth.uid();
    return 'accepted';
  end if;
  insert into public.friends (a, b) values (auth.uid(), other) on conflict do nothing;
  return 'sent';
end $$;

create or replace function public.friend_answer(name text, accept boolean) returns void
language plpgsql security definer set search_path = public as $$
declare other uuid;
begin
  select id into other from public.profiles where lower(username) = lower(trim(name));
  if accept then update public.friends set status = 'accepted' where a = other and b = auth.uid() and status = 'pending';
  else delete from public.friends where a = other and b = auth.uid() and status = 'pending'; end if;
end $$;

create or replace function public.friend_remove(name text) returns void
language plpgsql security definer set search_path = public as $$
declare other uuid;
begin
  select id into other from public.profiles where lower(username) = lower(trim(name));
  delete from public.friends where (a = auth.uid() and b = other) or (a = other and b = auth.uid());
end $$;

-- The caller's friends and friend requests, with who is online now.
create or replace function public.my_friends()
returns table (user_id uuid, username text, avatar text, rating integer, relation text, state text)
language sql stable security definer set search_path = public as $$
  select p.id, p.username, p.avatar, p.rating,
         case when f.status = 'accepted' then 'friend' when f.a = auth.uid() then 'outgoing' else 'incoming' end,
         (select s.state from public.presence s where s.user_id = p.id and s.last_seen > now() - interval '60 seconds')
  from public.friends f
  join public.profiles p on p.id = case when f.a = auth.uid() then f.b else f.a end
  where f.a = auth.uid() or f.b = auth.uid()
  order by (f.status = 'accepted') desc, p.username;
$$;

-- ---- matches and their results ----------------------------------------------------------------------------------------------
create table if not exists public.matches (
  id bigint generated always as identity primary key,
  room_code text not null,
  host uuid not null references public.profiles (id) on delete cascade,
  guest uuid not null references public.profiles (id) on delete cascade,
  netcode text not null default 'rollback',
  source text not null check (source in ('invite', 'quick')),
  created_at timestamptz not null default now(),
  host_report text check (host_report in ('win', 'loss', 'none')),
  guest_report text check (guest_report in ('win', 'loss', 'none')),
  status text not null default 'open' check (status in ('open', 'done', 'disputed', 'void')),
  winner uuid references public.profiles (id) on delete set null,
  points integer,
  finished_at timestamptz
);
create index if not exists matches_host on public.matches (host, created_at desc);
create index if not exists matches_guest on public.matches (guest, created_at desc);
alter table public.matches enable row level security;

create or replace function public.new_match(code text, host uuid, guest uuid, netcode text, source text) returns bigint
language sql security definer set search_path = public as $$
  insert into public.matches (room_code, host, guest, netcode, source) values (code, host, guest, netcode, source) returning id;
$$;

-- The caller's last match still waiting for its result (the last 3 hours): the launcher asks how it went after the game.
create or replace function public.my_open_match()
returns table (id bigint, opponent text, hosts boolean, created_at timestamptz, reported boolean)
language sql stable security definer set search_path = public as $$
  select m.id, p.username, m.host = auth.uid(), m.created_at,
         case when m.host = auth.uid() then m.host_report is not null else m.guest_report is not null end
  from public.matches m join public.profiles p on p.id = case when m.host = auth.uid() then m.guest else m.host end
  where (m.host = auth.uid() or m.guest = auth.uid()) and m.status = 'open' and m.created_at > now() - interval '3 hours'
  order by m.created_at desc limit 1;
$$;

-- How it went, by one of the two. Both answered: the points move (or not).
create or replace function public.report_result(match_id bigint, result text) returns text
language plpgsql security definer set search_path = public as $$
declare
  m public.matches;
  rh integer; rg integer; e double precision; k integer := 32; d integer; w uuid; l uuid;
begin
  if auth.uid() is null then raise exception 'not signed in'; end if;
  if result not in ('win', 'loss', 'none') then raise exception 'bad result'; end if;
  select * into m from public.matches where matches.id = match_id for update;
  if m.id is null or m.status <> 'open' then raise exception 'that match is closed'; end if;
  if m.host = auth.uid() then update public.matches set host_report = result where matches.id = match_id;
  elsif m.guest = auth.uid() then update public.matches set guest_report = result where matches.id = match_id;
  else raise exception 'not your match'; end if;
  select * into m from public.matches where matches.id = match_id;
  if m.host_report is null or m.guest_report is null then return 'waiting'; end if;
  if m.host_report = 'none' and m.guest_report = 'none' then
    update public.matches set status = 'void', finished_at = now() where matches.id = match_id; return 'void';
  end if;
  if (m.host_report = 'win' and m.guest_report = 'loss') or (m.host_report = 'loss' and m.guest_report = 'win') then
    w := case when m.host_report = 'win' then m.host else m.guest end;
    l := case when m.host_report = 'win' then m.guest else m.host end;
    select rating into rh from public.profiles where id = w;
    select rating into rg from public.profiles where id = l;
    e := 1.0 / (1.0 + power(10.0, (rg - rh) / 400.0));   -- (the winner's expected score)
    d := greatest(1, round(k * (1 - e))::integer);
    update public.profiles set rating = rating + d, wins = wins + 1 where id = w;
    update public.profiles set rating = greatest(0, rating - d), losses = losses + 1 where id = l;
    update public.matches set status = 'done', winner = w, points = d, finished_at = now() where matches.id = match_id;
    return 'done';
  end if;
  update public.matches set status = 'disputed', finished_at = now() where matches.id = match_id;
  return 'disputed';
end $$;

-- ---- invitations and quick match make matches -------------------------------------------------------------------------------
drop function if exists public.answer_invite(bigint, boolean);
create or replace function public.answer_invite(invite_id bigint, accept boolean)
returns table (room_code text, netcode text, match_id bigint)
language plpgsql security definer set search_path = public as $$
#variable_conflict use_column
declare inv public.invites; mid bigint;
begin
  update public.invites set status = case when accept then 'accepted' else 'declined' end, answered_at = now()
  where invites.id = invite_id and to_user = auth.uid() and status = 'pending' and created_at > now() - interval '60 seconds'
  returning * into inv;
  if inv.id is null then raise exception 'that invitation is no longer there'; end if;
  if accept then
    mid := public.new_match(inv.room_code, inv.from_user, inv.to_user, inv.netcode, 'invite');
    return query select inv.room_code, inv.netcode, mid;
  end if;
end $$;

-- How many are looking for a quick match now.
create or replace function public.searching_count() returns integer
language sql stable security definer set search_path = public as $$
  select count(*)::integer from public.queue q
  where q.room_code is null and exists (select 1 from public.presence p where p.user_id = q.user_id and p.last_seen > now() - interval '60 seconds');
$$;

-- Quick match by level: the waiting player nearest in points, within 150 points and 10 more for each second either has waited.
drop function if exists public.quick_match(text);
create or replace function public.quick_match(netcode text default 'rollback')
returns table (status text, room_code text, hosts boolean, opponent text)
language plpgsql security definer set search_path = public as $$
#variable_conflict use_column
declare
  me uuid := auth.uid();
  my_rating integer;
  other public.queue;
  code text;
  nc text := case when quick_match.netcode = 'normal' then 'normal' else 'rollback' end;
begin
  if me is null then raise exception 'not signed in'; end if;
  select rating into my_rating from public.profiles where id = me;
  delete from public.queue q where q.created_at < now() - interval '5 minutes' and q.room_code is null;
  delete from public.queue q where q.user_id = me;
  select q.* into other from public.queue q join public.profiles f on f.id = q.user_id
  where q.netcode = nc and q.room_code is null and q.user_id <> me
    and exists (select 1 from public.presence p where p.user_id = q.user_id and p.last_seen > now() - interval '60 seconds')
    and abs(f.rating - my_rating) <= 150 + 10 * extract(epoch from now() - q.created_at)
  order by abs(f.rating - my_rating), q.created_at limit 1 for update of q skip locked;
  if other.user_id is null then
    insert into public.queue (user_id, netcode) values (me, nc);
    return query select 'waiting'::text, null::text, null::boolean, null::text;
    return;
  end if;
  code := public.new_room_code();
  update public.queue set room_code = code, opponent = me, hosts = true where user_id = other.user_id;
  insert into public.queue (user_id, netcode, room_code, opponent, hosts) values (me, nc, code, other.user_id, false);
  perform public.new_match(code, other.user_id, me, nc, 'quick');
  return query select 'paired'::text, code, false, (select username from public.profiles where id = other.user_id);
end $$;

-- A waiting player looks again (its own wait widens the range): paired yet, or paired now with someone who came since.
create or replace function public.quick_retry(netcode text default 'rollback')
returns table (status text, room_code text, hosts boolean, opponent text)
language plpgsql security definer set search_path = public as $$
#variable_conflict use_column
declare
  me uuid := auth.uid(); mine public.queue; my_rating integer; other public.queue; code text;
begin
  select * into mine from public.queue q where q.user_id = me;
  if mine.user_id is null then return; end if;
  if mine.room_code is not null then
    return query select 'paired'::text, mine.room_code, mine.hosts, (select username from public.profiles where id = mine.opponent);
    return;
  end if;
  select rating into my_rating from public.profiles where id = me;
  select q.* into other from public.queue q join public.profiles f on f.id = q.user_id
  where q.netcode = mine.netcode and q.room_code is null and q.user_id <> me
    and exists (select 1 from public.presence p where p.user_id = q.user_id and p.last_seen > now() - interval '60 seconds')
    and abs(f.rating - my_rating) <= 150 + 10 * extract(epoch from now() - least(q.created_at, mine.created_at))
  order by abs(f.rating - my_rating), q.created_at limit 1 for update of q skip locked;
  if other.user_id is null then
    return query select 'waiting'::text, null::text, null::boolean, null::text;
    return;
  end if;
  code := public.new_room_code();
  update public.queue set room_code = code, opponent = me, hosts = true where user_id = other.user_id;
  update public.queue set room_code = code, opponent = other.user_id, hosts = false where user_id = me;
  perform public.new_match(code, other.user_id, me, mine.netcode, 'quick');
  return query select 'paired'::text, code, false, (select username from public.profiles where id = other.user_id);
end $$;

-- ---- profiles and history (the web page; anyone may look) -----------------------------------------------------------------
create or replace function public.player_profile(name text)
returns table (username text, avatar text, rating integer, wins integer, losses integer, created_at timestamptz, place bigint, online boolean)
language sql stable security definer set search_path = public as $$
  select p.username, p.avatar, p.rating, p.wins, p.losses, p.created_at,
         (select count(*) + 1 from public.profiles o where o.rating > p.rating),
         exists (select 1 from public.presence s where s.user_id = p.id and s.last_seen > now() - interval '60 seconds')
  from public.profiles p where lower(p.username) = lower(trim(name));
$$;

create or replace function public.player_matches(name text)
returns table (opponent text, won boolean, points integer, netcode text, finished_at timestamptz)
language sql stable security definer set search_path = public as $$
  select o.username, m.winner = p.id, m.points, m.netcode, m.finished_at
  from public.profiles p
  join public.matches m on (m.host = p.id or m.guest = p.id) and m.status = 'done'
  join public.profiles o on o.id = case when m.host = p.id then m.guest else m.host end
  where lower(p.username) = lower(trim(name))
  order by m.finished_at desc limit 25;
$$;

-- ---- who may call what ------------------------------------------------------------------------------------------------
revoke all on function public.set_avatar(text), public.set_username(text), public.friend_request(text), public.friend_answer(text, boolean),
  public.friend_remove(text), public.my_friends(), public.my_open_match(), public.report_result(bigint, text),
  public.answer_invite(bigint, boolean), public.searching_count(), public.quick_match(text), public.quick_retry(text) from public, anon;
grant execute on function public.set_avatar(text), public.set_username(text), public.friend_request(text), public.friend_answer(text, boolean),
  public.friend_remove(text), public.my_friends(), public.my_open_match(), public.report_result(bigint, text),
  public.answer_invite(bigint, boolean), public.searching_count(), public.quick_match(text), public.quick_retry(text) to authenticated;
grant execute on function public.player_profile(text), public.player_matches(text) to anon, authenticated;
revoke all on function public.new_match(text, uuid, uuid, text, text) from public, anon, authenticated;
