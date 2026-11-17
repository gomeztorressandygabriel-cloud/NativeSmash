using System.Net.Http;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace NativeSmash
{
    // NativeSmash's own players server (Supabase: server/supabase/schema.sql and schema_v2.sql in the 0.1 tree; its URL and
    // public key in tools\players.json): the NativeSmash account made on the project's web page, being online, the players
    // online, friends and invitations. The session (the server's refresh token and the name) is kept in
    // %LOCALAPPDATA%\NativeSmash\session.json, as the 0.1 launcher kept it, so a player signed in there stays signed in here;
    // the password is never kept.
    public sealed class Players
    {
        public record Player(string Id, string Name, string State, int Rating, string Avatar = "", string Relation = "");
        public record Invite(long Id, string Name);
        public record Profile(string Name, string Avatar, int Rating, int Wins, int Losses, long Place);

        readonly HttpClient http;
        public readonly string Url = "", Key = "", SignupPage = "";
        public bool On => Url != "" && Key != "";
        string access = "", refresh = "";
        DateTime accessUntil = DateTime.MinValue;
        // (NATIVESMASH_SESSION: another session file, to test signed out without touching the player's own)
        static readonly string SessionFile = Environment.GetEnvironmentVariable("NATIVESMASH_SESSION") is { Length: > 0 } f ? f
            : Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "NativeSmash", "session.json");

        public bool SignedIn => access != "";
        public string Name { get; private set; } = "";
        public string UserId { get; private set; } = "";

        public Players(string repo, HttpClient http)
        {
            this.http = http;
            try
            {
                using var doc = JsonDocument.Parse(File.ReadAllText(Path.Combine(repo, "tools", "players.json")));
                var r = doc.RootElement;
                Url = (r.TryGetProperty("url", out var u) ? u.GetString() ?? "" : "").TrimEnd('/');
                Key = r.TryGetProperty("key", out var k) ? k.GetString() ?? "" : "";
                SignupPage = r.TryGetProperty("signup", out var s) ? s.GetString() ?? "" : "";
            }
            catch { }
        }

        // ---- talking to the server ------------------------------------------------------------------------------
        public sealed class ServerError : Exception { public ServerError(string m) : base(m) { } }

        async Task<JsonNode?> Web(HttpMethod method, string path, object? body, bool anon = false)
        {
            using var req = new HttpRequestMessage(method, Url + path);
            req.Headers.TryAddWithoutValidation("apikey", Key);
            req.Headers.TryAddWithoutValidation("Authorization", "Bearer " + (anon || access == "" ? Key : access));
            if (body != null) req.Content = new StringContent(JsonSerializer.Serialize(body), Encoding.UTF8, "application/json");
            using var resp = await http.SendAsync(req);
            string text = await resp.Content.ReadAsStringAsync();
            if (!resp.IsSuccessStatusCode)
            {
                string why = text;
                try
                {
                    var j = JsonNode.Parse(text);
                    foreach (string k in new[] { "message", "msg", "error_description", "error" })
                        if (j?[k] is JsonValue v && v.ToString() != "") { why = v.ToString(); break; }
                }
                catch { }
                throw new ServerError(why);
            }
            return text.Trim() == "" ? null : JsonNode.Parse(text);
        }

        async Task<JsonNode?> Rpc(string name, object? body = null)
        {
            if (access != "" && DateTime.UtcNow > accessUntil && refresh != "") await Refresh();
            return await Web(HttpMethod.Post, "/rest/v1/rpc/" + name, body ?? new { });
        }

        static string S(JsonNode? n, string k) => n?[k] is JsonValue v ? v.ToString() : "";
        static int I(JsonNode? n, string k) => n?[k] is JsonValue v && int.TryParse(v.ToString(), out int i) ? i : 0;
        static long L(JsonNode? n, string k) => n?[k] is JsonValue v && long.TryParse(v.ToString(), out long i) ? i : 0;
        static IEnumerable<JsonNode?> Rows(JsonNode? n) => n is JsonArray a ? a : n == null ? Enumerable.Empty<JsonNode?>() : new[] { n };

        // ---- the session ------------------------------------------------------------------------------------------
        async Task Keep(JsonNode? tok)
        {
            access = S(tok, "access_token"); refresh = S(tok, "refresh_token");
            accessUntil = DateTime.UtcNow.AddSeconds(Math.Max(60, I(tok, "expires_in") - 120));
            UserId = S(tok?["user"], "id");
            try
            {
                var prof = await Web(HttpMethod.Get, $"/rest/v1/profiles?id=eq.{UserId}&select=username", null);
                string n = S(Rows(prof).FirstOrDefault(), "username");
                if (n != "") Name = n;
            }
            catch { }
            try
            {
                Directory.CreateDirectory(Path.GetDirectoryName(SessionFile)!);
                File.WriteAllText(SessionFile, JsonSerializer.Serialize(new { refresh, name = Name }));
            }
            catch { }
        }

        async Task Refresh() => await Keep(await Web(HttpMethod.Post, "/auth/v1/token?grant_type=refresh_token", new { refresh_token = refresh }, anon: true));

        // The session from last time; false when there is none (or it ended).
        public async Task<bool> Resume()
        {
            if (!On || !File.Exists(SessionFile)) return false;
            try
            {
                var s = JsonNode.Parse(File.ReadAllText(SessionFile));
                refresh = S(s, "refresh"); Name = S(s, "name");
                if (refresh == "") return false;
                await Refresh();
                return true;
            }
            catch (ServerError) { access = ""; refresh = ""; try { File.Delete(SessionFile); } catch { } return false; }
            catch { access = ""; return false; }
        }

        public async Task SignIn(string email, string password)
            => await Keep(await Web(HttpMethod.Post, "/auth/v1/token?grant_type=password", new { email, password }, anon: true));

        public async Task SignOut()
        {
            try { if (access != "") await Rpc("go_offline"); } catch { }
            access = ""; refresh = ""; Name = ""; UserId = "";
            try { File.Delete(SessionFile); } catch { }
        }

        // ---- what the window shows ----------------------------------------------------------------------------------
        public Task Heartbeat(string state, string version) => Rpc("heartbeat", new { state, version });
        public async Task GoOffline() { try { if (access != "") await Rpc("go_offline"); } catch { } }

        public async Task<int> OnlineCount()
        {
            var n = access != "" ? await Rpc("online_count") : await Web(HttpMethod.Post, "/rest/v1/rpc/online_count", new { }, anon: true);
            return n is JsonValue v && int.TryParse(v.ToString(), out int i) ? i : 0;
        }

        public async Task<List<Player>> Online() =>
            Rows(await Rpc("online_players")).Select(r => new Player(S(r, "user_id"), S(r, "username"), S(r, "state"), I(r, "rating"))).ToList();

        public async Task<List<Player>> Friends() =>
            Rows(await Rpc("my_friends")).Select(r => new Player(S(r, "user_id"), S(r, "username"), S(r, "state"), I(r, "rating"), S(r, "avatar"), S(r, "relation"))).ToList();

        public async Task<List<Invite>> Invites() =>
            Rows(await Rpc("my_invites")).Select(r => new Invite(L(r, "id"), S(r, "username"))).ToList();

        public async Task<Profile?> MyProfile()
        {
            if (Name == "") return null;
            var r = Rows(await Rpc("player_profile", new { name = Name })).FirstOrDefault();
            return r == null ? null : new Profile(S(r, "username"), S(r, "avatar"), I(r, "rating"), I(r, "wins"), I(r, "losses"), L(r, "place"));
        }

        public async Task<(long Id, string Code)> SendInvite(string userId)
        {
            var r = Rows(await Rpc("send_invite", new { to_user = userId, netcode = "rollback" })).FirstOrDefault();
            return (L(r, "id"), S(r, "room_code"));
        }
        public async Task<string> InviteStatus(long id) => (await Rpc("invite_status", new { invite_id = id }))?.ToString().Trim('"') ?? "";
        public Task CancelInvite(long id) => Rpc("cancel_invite", new { invite_id = id });
        public async Task<string> AnswerInvite(long id, bool accept)
        {
            var r = Rows(await Rpc("answer_invite", new { invite_id = id, accept })).FirstOrDefault();
            return S(r, "room_code");
        }

        public async Task<string> FriendRequest(string name) => (await Rpc("friend_request", new { name }))?.ToString().Trim('"') ?? "";
        public Task FriendAnswer(string name, bool accept) => Rpc("friend_answer", new { name, accept });
        public Task FriendRemove(string name) => Rpc("friend_remove", new { name });
    }
}
