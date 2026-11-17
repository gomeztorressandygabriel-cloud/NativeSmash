using System.IO;
using System.Text;
using System.Text.RegularExpressions;
using OpenSmash.Launcher;

namespace NativeSmash
{
    // The files the game reads (config\ once the game made it, else the folder itself: WindowSmash.ConfigFile), read
    // and written as OpenSmash's launcher does (LauncherForm.ReadSettings/WriteSettingsFiles): a missing value is the
    // game's default (SettingDefaults), and only what the player changes here is written, so the game's own settings
    // menu (Esc) and this window never undo each other.
    public static class Settings
    {
        public static string File(string repo, string name) => WindowSmash.ConfigFile(repo, name);

        public static string? Ini(string path, string section, string key)
        {
            if (!System.IO.File.Exists(path)) return null;
            bool inSection = section == "";
            foreach (string raw in System.IO.File.ReadAllLines(path))
            {
                string line = raw.Trim();
                if (line.StartsWith("["))
                {
                    string? h = PadProfile.Header(line);
                    inSection = h != null && h.Equals(section, StringComparison.OrdinalIgnoreCase);
                    continue;
                }
                if (!inSection || line.Length == 0 || line[0] == ';' || line[0] == '#') continue;
                int eq = line.IndexOf('=');
                if (eq <= 0 || !line[..eq].Trim().Equals(key, StringComparison.OrdinalIgnoreCase)) continue;
                string v = line[(eq + 1)..];
                int c = v.IndexOfAny(new[] { ';', '#' });
                if (c >= 0 && (c == 0 || char.IsWhiteSpace(v[c - 1]))) v = v[..c];
                return v.Trim();
            }
            return null;
        }

        public static void SetIni(string path, string section, string key, string value)
        {
            var lines = System.IO.File.Exists(path) ? System.IO.File.ReadAllLines(path).ToList() : new List<string>();
            bool inSection = section == "";
            int sectionAt = section == "" ? 0 : -1, keyAt = -1;
            for (int i = 0; i < lines.Count; i++)
            {
                string line = lines[i].Trim();
                if (line.StartsWith("["))
                {
                    string? h = PadProfile.Header(line);
                    inSection = section != "" && h != null && h.Equals(section, StringComparison.OrdinalIgnoreCase);
                    if (inSection) sectionAt = i;
                    else if (section == "" ) inSection = false;
                    continue;
                }
                if (!inSection || line.Length == 0 || line[0] == ';' || line[0] == '#') continue;
                int eq = line.IndexOf('=');
                if (eq > 0 && line[..eq].Trim().Equals(key, StringComparison.OrdinalIgnoreCase)) { keyAt = i; break; }
            }
            string row = key + " = " + value;
            if (keyAt >= 0) lines[keyAt] = row;
            else if (section == "")
            {
                int at = lines.FindIndex(l => l.TrimStart().StartsWith("["));
                lines.Insert(at < 0 ? lines.Count : at, row);
            }
            else if (sectionAt >= 0) lines.Insert(sectionAt + 1, row);
            else
            {
                if (lines.Count > 0 && lines[^1].Trim() != "") lines.Add("");
                lines.Add("[" + section + "]");
                lines.Add(row);
            }
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            System.IO.File.WriteAllLines(path, lines, new UTF8Encoding(false));
        }

        public static string FirstLine(string path)
        {
            if (!System.IO.File.Exists(path)) return "";
            foreach (string raw in System.IO.File.ReadAllLines(path))
            {
                string line = raw.Trim();
                if (line.Length > 0 && line[0] != '#') return line;
            }
            return "";
        }

        public static void SetFirstLine(string path, string value)
        {
            var lines = System.IO.File.Exists(path) ? System.IO.File.ReadAllLines(path).ToList() : new List<string>();
            int at = lines.FindIndex(l => l.Trim().Length > 0 && l.Trim()[0] != '#');
            if (string.IsNullOrEmpty(value)) { if (at >= 0) lines.RemoveAt(at); }
            else if (at >= 0) lines[at] = value;
            else lines.Add(value);
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            System.IO.File.WriteAllLines(path, lines, new UTF8Encoding(false));
        }

        // display.ini's keys (no section) and controllers.ini's [general] ones, as the game has them now.
        public static string Display(string repo, string key) => Ini(File(repo, "display.ini"), "", key) ?? SettingDefaults.Of(key);
        public static void SetDisplay(string repo, string key, string value) => SetIni(File(repo, "display.ini"), "", key, value);
        public static string Pads(string repo, string key) => Ini(File(repo, "controllers.ini"), "general", key) ?? SettingDefaults.Of(key);
        public static void SetPads(string repo, string key, string value) => SetIni(File(repo, "controllers.ini"), "general", key, value);

        // online_server.txt: the lobby (auto, private, custom, lan, offline), its code and a server of one's own.
        public static (string Lobby, string Code, string Server) Lobby(string repo)
        {
            string path = File(repo, "online_server.txt");
            string line = System.IO.File.Exists(path) ? FirstLine(path) : SettingDefaults.OfFile("online_server.txt");
            var r = SettingDefaults.LobbyFromLine(line);
            string ini = File(repo, "launcher.ini");
            string code = r.Code != "" ? r.Code : Ini(ini, "opensmash", "code") ?? "";
            string server = r.Server != "" ? r.Server : Ini(ini, "opensmash", "server") ?? "";
            if (SettingDefaults.MeansPublicLobby(server)) server = "";
            if (System.IO.File.Exists(path) && SettingDefaults.IsOldLobbyLine(line))
                try { SetFirstLine(path, SettingDefaults.LineFromLobby(r.Lobby, r.Code, r.Server)); } catch { }
            return (r.Lobby, code, server);
        }

        // "" when written, else what is wrong (nothing is written then).
        public static string SetLobby(string repo, string lobby, string code, string server)
        {
            code = (code ?? "").Replace(" ", "");
            server = (server ?? "").Trim();
            if (lobby == "private" && code == "") return "code";
            if (lobby == "custom" && server != "" && !Regex.IsMatch(server, @"^\S+:\d+$")) return "server";
            SetFirstLine(File(repo, "online_server.txt"), SettingDefaults.LineFromLobby(lobby, code, server));
            string ini = File(repo, "launcher.ini");
            if (code != "") SetIni(ini, "opensmash", "code", code);
            if (server != "") SetIni(ini, "opensmash", "server", server);
            return "";
        }

        public static string Name(string repo) => FirstLine(File(repo, "online_name.txt"));
        public static void SetName(string repo, string name) => SetFirstLine(File(repo, "online_name.txt"), name.Trim());
        public static bool Direct(string repo)
        {
            string p = File(repo, "online_direct.txt");
            string v = System.IO.File.Exists(p) ? FirstLine(p) : SettingDefaults.OfFile("online_direct.txt");
            return !v.StartsWith("off", StringComparison.OrdinalIgnoreCase);
        }
        public static void SetDirect(string repo, bool on) => SetFirstLine(File(repo, "online_direct.txt"), on ? "on" : "off");

        // launcher.ini [nativesmash] show_launcher = 0: Play SSBU.bat and the shortcut start the game straight away.
        public static bool StartDirectly(string repo) => (Ini(File(repo, "launcher.ini"), "nativesmash", "show_launcher") ?? "1") == "0";
        public static void SetStartDirectly(string repo, bool on) => SetIni(File(repo, "launcher.ini"), "nativesmash", "show_launcher", on ? "0" : "1");

        // build\launch_mode.txt: "mods" (the mods turned on) or "vanilla" (the game as it is), as OpenSmash's launcher.
        public static string LaunchMode(string repo)
        {
            try
            {
                string p = Path.Combine(WindowSmash.BuildDir(repo), "launch_mode.txt");
                if (System.IO.File.Exists(p) && System.IO.File.ReadAllText(p).Trim().Equals("vanilla", StringComparison.OrdinalIgnoreCase)) return "vanilla";
            }
            catch { }
            return "mods";
        }
        public static void SetLaunchMode(string repo, string mode)
        {
            Directory.CreateDirectory(WindowSmash.BuildDir(repo));
            System.IO.File.WriteAllText(Path.Combine(WindowSmash.BuildDir(repo), "launch_mode.txt"), mode + "\n");
        }
    }
}
