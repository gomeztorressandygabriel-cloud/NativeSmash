using System.Diagnostics;
using System.IO;
using System.Text;
using System.Text.Json;
using OpenSmash.Launcher;

namespace NativeSmash
{
    // Starting things the way OpenSmash's launcher does: Python scripts (its own Python, ModManagerProcess.FindPython),
    // PowerShell scripts, and the game (WindowSmash.StartGame) after the online files (Epic's and Discord's libraries)
    // are put beside it, so a NativeSmash player meets OpenSmash's players in the same lobby and on Epic's relays.
    public static class Launch
    {
        public static string Python(string repo) => ModManagerProcess.FindPython(repo, WindowSmash.BuildDir(repo));

        public static (int Code, string Output) Run(string file, IEnumerable<string> args, string workDir, Action<string>? onLine = null,
            Action<Process>? started = null, IDictionary<string, string>? env = null)
        {
            var psi = new ProcessStartInfo
            {
                FileName = file, WorkingDirectory = workDir, UseShellExecute = false, CreateNoWindow = true,
                RedirectStandardOutput = true, RedirectStandardError = true,
                StandardOutputEncoding = Encoding.UTF8, StandardErrorEncoding = Encoding.UTF8,
            };
            psi.EnvironmentVariables["PYTHONIOENCODING"] = "utf-8";
            psi.EnvironmentVariables["PYTHONUNBUFFERED"] = "1";
            if (env != null) foreach (var kv in env) psi.EnvironmentVariables[kv.Key] = kv.Value;
            foreach (string a in args) psi.ArgumentList.Add(a);
            var sb = new StringBuilder();
            using var p = Process.Start(psi);
            if (p == null) return (-1, "could not start " + file);
            started?.Invoke(p);
            void Take(string? line)
            {
                if (line == null) return;
                lock (sb) sb.AppendLine(line);
                try { onLine?.Invoke(line); } catch { }
            }
            p.OutputDataReceived += (_, e) => Take(e.Data);
            p.ErrorDataReceived += (_, e) => Take(e.Data);
            p.BeginOutputReadLine();
            p.BeginErrorReadLine();
            p.WaitForExit();
            lock (sb) return (p.ExitCode, sb.ToString());
        }

        public static (int Code, string Output) RunPython(string repo, string script, IEnumerable<string> args, Action<string>? onLine = null, Action<Process>? started = null)
        {
            string py = Python(repo);
            if (string.IsNullOrEmpty(py)) return (-1, "Python was not found");
            return Run(py, new[] { Path.Combine(repo, "tools", script) }.Concat(args), repo, onLine, started);
        }

        public static (int Code, string Output) RunPowerShell(string repo, string script, IEnumerable<string> args, Action<string>? onLine = null)
            => Run("powershell.exe", new[] { "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", Path.Combine(repo, "tools", script) }.Concat(args), repo, onLine);

        // tools\online_files.py: Epic Online Services' and Discord's files beside the game (nothing downloaded when they
        // are current). Without them the game goes online through the public lobby alone, unseen by the players on Epic.
        public static void EnsureOnlineFiles(string repo)
        {
            try
            {
                if (File.Exists(Path.Combine(repo, "tools", "online_files.py")))
                    RunPython(repo, "online_files.py", new[] { WindowSmash.BuildDir(repo) });
            }
            catch { }
        }

        // The mods as Play leaves them (OpenSmash's PrepareModsForLaunch): "vanilla" takes them out, "mods" applies the
        // ones turned on. "" when ready, else what to say.
        public static string PrepareMods(string repo, string mode)
        {
            try
            {
                var st = ModStatus(repo);
                if (st == null) return "";
                if (mode == "vanilla") { if (st.Value.Modded) RunPython(repo, "os_mods.py", new[] { "vanilla" }); return ""; }
                if (st.Value.Enabled == 0) return "";
                if (st.Value.NeedsBuild != "") return "build";
                if (!st.Value.Modded) RunPython(repo, "os_mods.py", new[] { "apply" });
            }
            catch { }
            return "";
        }

        public record struct Mods(int Total, int Enabled, bool Modded, string NeedsBuild, List<(string Name, string Title, bool On)> List);

        // tools\os_mods.py status: the mods in mods\ (the ":..." entries are built-in switches, not mods).
        public static Mods? ModStatus(string repo)
        {
            if (!File.Exists(Path.Combine(repo, "tools", "os_mods.py"))) return null;
            var (code, output) = RunPython(repo, "os_mods.py", new[] { "status" });
            string? json = output.Split('\n').Select(l => l.Trim()).FirstOrDefault(l => l.StartsWith("{"));
            if (json == null) return null;
            try
            {
                using var doc = JsonDocument.Parse(json);
                var r = doc.RootElement;
                var list = new List<(string, string, bool)>();
                if (r.TryGetProperty("mods", out var mods) && mods.ValueKind == JsonValueKind.Array)
                    foreach (var m in mods.EnumerateArray())
                    {
                        string name = m.TryGetProperty("name", out var n) ? n.GetString() ?? "" : "";
                        if (name == "" || name.StartsWith(":")) continue;
                        string title = m.TryGetProperty("title", out var t) && t.ValueKind == JsonValueKind.String && t.GetString() != "" ? t.GetString()! : name;
                        bool on = m.TryGetProperty("enabled", out var e) && e.ValueKind == JsonValueKind.True;
                        list.Add((name, title, on));
                    }
                bool modded = r.TryGetProperty("modded", out var md) && md.ValueKind == JsonValueKind.True;
                string needs = r.TryGetProperty("needs_build", out var nb) && nb.ValueKind == JsonValueKind.String ? nb.GetString() ?? "" : "";
                return new Mods(list.Count, list.Count(x => x.Item3), modded, needs, list);
            }
            catch (JsonException) { return null; }
        }

        public static void Open(string target)
        {
            try { Process.Start(new ProcessStartInfo(target) { UseShellExecute = true }); } catch { }
        }

        // A short commit name ("65f5cf2") or "".
        public static string Short(string sha) => sha.Length >= 7 ? sha[..7] : sha;
    }
}
