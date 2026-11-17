using System.Diagnostics;
using System.IO;
using System.Net.Http;
using System.Text.Json;
using System.Text.RegularExpressions;
using System.Windows;
using OpenSmash.Launcher;
using static NativeSmash.Words;
using Brush = System.Windows.Media.Brush;
using Orientation = System.Windows.Controls.Orientation;
using HorizontalAlignment = System.Windows.HorizontalAlignment;
using VerticalAlignment = System.Windows.VerticalAlignment;
using FontStyle = System.Windows.FontStyle;
using Image = System.Windows.Controls.Image;
using MessageBox = System.Windows.MessageBox;

namespace NativeSmash
{
    // Game: updates (the installer's repository and branch, tools\install_windowsmash.ps1 $RepoOwner/$RepoName/
    // $UpdateBranch, compared with this copy's commit as OpenSmash's launcher does; applied with the installer's
    // -UpdateOnly and -BuildAfterUpdate, then this launcher rebuilt when its sources changed), and the tools.
    public partial class MainWindow
    {
        string updLocal = "", updHead = "";
        bool updBusy;

        void InitGame()
        {
            GameTitle.Text = T("gameTitle"); UpdTitle.Text = T("updTitle"); UpdCheckBtn.Content = T("updCheck"); UpdNowBtn.Content = T("updNow");
            ToolsTitle.Text = T("toolsTitle"); RepairBtn.Content = T("repair"); UnlockBtn.Content = T("unlock"); FolderBtn.Content = T("folder");
            LogBtn.Content = T("log"); ReportsBtn.Content = T("reports"); OpenSmashBtn.Content = T("opensmash"); ToolsNote.Text = T("toolsNote");
            UpdStatus.Text = ""; UpdDetail.Text = "";
            UpdCheckBtn.Click += (_, _) => _ = CheckUpdates(atStart: false);
            UpdNowBtn.Click += (_, _) => ApplyUpdate();
            RepairBtn.Click += (_, _) => Repair();
            UnlockBtn.Click += (_, _) => Unlock();
            FolderBtn.Click += (_, _) => Launch.Open(repo);
            LogBtn.Click += (_, _) =>
            {
                string log = Path.Combine(buildDir, "play.log");
                if (File.Exists(log)) Process.Start(new ProcessStartInfo("notepad.exe", "\"" + log + "\"") { UseShellExecute = true }); else Launch.Open(buildDir);
            };
            ReportsBtn.Click += (_, _) => { string d = Path.Combine(repo, "reports"); Directory.CreateDirectory(d); Launch.Open(d); };
            OpenSmashBtn.Click += (_, _) =>
            {
                try
                {
                    PresenceStop();
                    var f = new LauncherForm(workDir, repo, buildDir);
                    UiLocale.Attach(f);
                    f.FormClosed += (_, _) => { PresenceStart(); RefreshHome(); };
                    f.Show();
                }
                catch (Exception ex) { Toast(ex.Message, warn: true); PresenceStart(); }
            };
        }

        (string Owner, string Name, string Branch) UpdateSource()
        {
            string owner = "gomeztorressandygabriel-cloud", name = "NativeSmash", branch = "nativesmash";
            try
            {
                string text = File.ReadAllText(Path.Combine(repo, "tools", "install_windowsmash.ps1"));
                var o = Regex.Match(text, @"(?m)^\$RepoOwner\s*=\s*'([^']+)'"); if (o.Success) owner = o.Groups[1].Value;
                var n = Regex.Match(text, @"(?m)^\$RepoName\s*=\s*'([^']+)'"); if (n.Success) name = n.Groups[1].Value;
                var b = Regex.Match(text, @"(?m)^\$UpdateBranch\s*=\s*'([^']+)'"); if (b.Success) branch = b.Groups[1].Value;
            }
            catch { }
            return (owner, name, branch);
        }

        async Task<string> GitHub(string url)
        {
            using var req = new HttpRequestMessage(HttpMethod.Get, url);
            req.Headers.Accept.ParseAdd("application/vnd.github+json");
            using var resp = await http.SendAsync(req);
            string body = await resp.Content.ReadAsStringAsync();
            if (!resp.IsSuccessStatusCode) throw new InvalidOperationException("GitHub " + (int)resp.StatusCode);
            return body;
        }

        string LocalCommit()
        {
            try
            {
                if (Directory.Exists(Path.Combine(repo, ".git")) || File.Exists(Path.Combine(repo, ".git")))
                {
                    var r = Launch.Run("git", new[] { "rev-parse", "HEAD" }, repo);
                    var m = Regex.Match(r.Output.Trim(), @"^[0-9a-fA-F]{40}$");
                    if (r.Code == 0 && m.Success) return m.Value.ToLowerInvariant();
                }
            }
            catch { }
            try
            {
                string f = Path.Combine(repo, "tools", "source_version.txt");
                if (File.Exists(f))
                {
                    var m = Regex.Match(File.ReadAllText(f), @"(?<![0-9a-fA-F])([0-9a-fA-F]{40})(?![0-9a-fA-F])");
                    if (m.Success) return m.Groups[1].Value.ToLowerInvariant();
                }
            }
            catch { }
            return "";
        }

        async Task CheckUpdates(bool atStart)
        {
            if (updBusy) return;
            updBusy = true;
            UpdCheckBtn.IsEnabled = false; UpdCheckBtn.Content = T("updChecking");
            UpdNowBtn.Visibility = Visibility.Collapsed; UpdDetail.Text = "";
            try
            {
                var (owner, name, branch) = UpdateSource();
                string api = $"https://api.github.com/repos/{owner}/{name}";
                updLocal = await Task.Run(LocalCommit);
                ThisCopy.Text = updLocal != "" ? F("thisCopy", Launch.Short(updLocal)) : "";
                // (the branch, as the installer's Get-UpdateRef: the repository's "latest" release is an old 13.0.3 one)
                string reference = Uri.EscapeDataString(branch);
                using var head = JsonDocument.Parse(await GitHub($"{api}/commits/{reference}"));
                updHead = head.RootElement.GetProperty("sha").GetString() ?? "";
                if (updLocal == "") { UpdStatus.Text = T("updUnknown"); return; }
                if (updLocal == updHead) { UpdStatus.Text = T("updUpToDate"); UpdBanner.Visibility = Visibility.Collapsed; return; }
                string cmpBody;
                try { cmpBody = await GitHub($"{api}/compare/{updLocal}...{updHead}"); }
                catch (InvalidOperationException) { UpdStatus.Text = T("updOwn"); return; }   // (this copy's commit is not on GitHub)
                using var cmp = JsonDocument.Parse(cmpBody);
                string status = cmp.RootElement.GetProperty("status").GetString() ?? "";
                int ahead = cmp.RootElement.TryGetProperty("ahead_by", out var ab) && ab.TryGetInt32(out int n) ? n : 0;
                if (status == "ahead")
                {
                    UpdStatus.Text = F("updAvailable", ahead);
                    var titles = new List<string>();
                    if (cmp.RootElement.TryGetProperty("commits", out var commits))
                        foreach (var c in commits.EnumerateArray().Reverse())
                        {
                            string line = (c.GetProperty("commit").GetProperty("message").GetString() ?? "").Split('\n')[0].Trim();
                            if (line.Length > 0 && !line.StartsWith("Merge ", StringComparison.OrdinalIgnoreCase)) titles.Add("•  " + (line.Length > 110 ? line[..110] + "…" : line));
                            if (titles.Count >= 4) break;
                        }
                    UpdDetail.Text = string.Join("\n", titles);
                    UpdNowBtn.Visibility = Visibility.Visible;
                    UpdBannerText.Text = F("updBanner", ahead);
                    UpdBanner.Visibility = Visibility.Visible;
                }
                else if (status is "identical") UpdStatus.Text = T("updUpToDate");
                else if (status is "behind") UpdStatus.Text = T("updNewer");
                else UpdStatus.Text = T("updOwn");
            }
            catch (Exception ex) when (ex is HttpRequestException or TaskCanceledException or InvalidOperationException or JsonException or KeyNotFoundException)
            {
                if (!atStart) UpdStatus.Text = F("updFail", ex.Message);
            }
            finally
            {
                updBusy = false;
                UpdCheckBtn.IsEnabled = true; UpdCheckBtn.Content = T("updCheck");
            }
        }

        static string LauncherFingerprint(string repo)
        {
            using var hash = System.Security.Cryptography.IncrementalHash.CreateHash(System.Security.Cryptography.HashAlgorithmName.SHA256);
            foreach (string dir in new[] { "NativeSmash.Launcher", "OpenSmash.Launcher", "locales" })
            {
                string root = Path.Combine(repo, dir);
                if (!Directory.Exists(root)) continue;
                foreach (string f in Directory.EnumerateFiles(root, "*", SearchOption.AllDirectories)
                    .Where(p => !p.Contains("\\bin\\") && !p.Contains("\\obj\\")).OrderBy(p => p, StringComparer.OrdinalIgnoreCase))
                {
                    hash.AppendData(System.Text.Encoding.UTF8.GetBytes(Path.GetRelativePath(repo, f) + "\0"));
                    hash.AppendData(File.ReadAllBytes(f));
                }
            }
            return Convert.ToHexString(hash.GetHashAndReset());
        }

        async void ApplyUpdate()
        {
            if (updBusy) return;
            if (GameRunning()) { Toast(T("gameRunning"), warn: true); return; }
            if (MessageBox.Show(this, T("updConfirm"), "NativeSmash", MessageBoxButton.YesNo, MessageBoxImage.Question) != MessageBoxResult.Yes) return;
            updBusy = true;
            UpdNowBtn.IsEnabled = UpdCheckBtn.IsEnabled = PlayBtn.IsEnabled = false;
            UpdRun.Visibility = Visibility.Visible; UpdBar.IsIndeterminate = true;
            UpdStatus.Text = T("updRunning");
            string why = "";
            bool launcherChanged = false;
            try
            {
                string before = await Task.Run(LocalCommit);
                string fpBefore = await Task.Run(() => LauncherFingerprint(repo));
                void Line(string l) => Dispatcher.BeginInvoke(() => UpdLine.Text = l.Trim());
                var r = await Task.Run(() => Launch.RunPowerShell(repo, "install_windowsmash.ps1", new[] { "-UpdateOnly" }, Line));
                if (r.Code != 0) throw new InvalidOperationException(r.Output.Contains("changes of its own") ? T("updOwn") : Tail(r.Output));
                string after = await Task.Run(LocalCommit);
                if (after != before)
                {
                    void Build(string l) => Dispatcher.BeginInvoke(() =>
                    {
                        UpdLine.Text = l.Trim();
                        var m = Regex.Match(l, @"(?:^LOG percent (\d{1,3})$)|(?:\]\s+(\d{1,3})%)");
                        if (m.Success && int.TryParse(m.Groups[1].Success ? m.Groups[1].Value : m.Groups[2].Value, out int pct))
                        { UpdBar.IsIndeterminate = false; UpdBar.Value = pct * 10; UpdStatus.Text = F("updBuilding", pct); }
                    });
                    var b = await Task.Run(() => Launch.RunPowerShell(repo, "install_windowsmash.ps1", new[] { "-BuildAfterUpdate" }, Build));
                    if (b.Code != 0) throw new InvalidOperationException(Tail(b.Output));
                    launcherChanged = await Task.Run(() => LauncherFingerprint(repo)) != fpBefore;
                }
            }
            catch (Exception ex) { why = ex.Message; }
            UpdRun.Visibility = Visibility.Collapsed;
            UpdNowBtn.IsEnabled = UpdCheckBtn.IsEnabled = true; PlayBtn.IsEnabled = WindowSmash.IsBuilt(repo);
            updBusy = false;
            if (why != "") { UpdStatus.Text = F("updError", why); return; }
            UpdBanner.Visibility = Visibility.Collapsed;
            if (launcherChanged && RebuildLauncher())
            {
                MessageBox.Show(this, T("updLauncher"), "NativeSmash", MessageBoxButton.OK, MessageBoxImage.Information);
                Close();
                return;
            }
            UpdStatus.Text = T("updDone");
            await CheckUpdates(atStart: false);
            RefreshHome(); RefreshShaders();
        }

        // tools\build_nativesmash_launcher.ps1 -WaitForProcessId: rebuilds NativeSmash.exe once this one has closed and
        // opens it again. Only with the .NET SDK (a release's NativeSmash.exe updates with the release instead).
        bool RebuildLauncher()
        {
            string script = Path.Combine(repo, "tools", "build_nativesmash_launcher.ps1");
            if (!File.Exists(script)) return false;
            try
            {
                var sdk = Launch.Run("dotnet", new[] { "--list-sdks" }, repo);
                if (sdk.Code != 0 || !Regex.IsMatch(sdk.Output, @"(?m)^\s*[89]\.|^\s*1\d\.")) return false;
                var psi = new ProcessStartInfo("powershell.exe") { UseShellExecute = false, CreateNoWindow = true, WorkingDirectory = repo };
                foreach (string a in new[] { "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", script, "-WaitForProcessId", Environment.ProcessId.ToString(), "-Start" })
                    psi.ArgumentList.Add(a);
                Process.Start(psi);
                return true;
            }
            catch { return false; }
        }

        void Repair()
        {
            if (GameRunning()) { Toast(T("gameRunning"), warn: true); return; }
            PresenceStop();
            Hide();
            try { App.RunInstaller(repo, workDir); }
            catch (Exception ex) { Toast(ex.Message, warn: true); }
            Show(); Activate();
            PresenceStart();
            RefreshHome(); RefreshShaders();
        }

        async void Unlock()
        {
            if (GameRunning()) { Toast(T("gameRunning"), warn: true); return; }
            if (MessageBox.Show(this, T("unlockAsk"), "NativeSmash", MessageBoxButton.YesNo, MessageBoxImage.Question) != MessageBoxResult.Yes) return;
            UnlockBtn.IsEnabled = false;
            var r = await Task.Run(() => Launch.RunPowerShell(repo, "Unlock-All-Fighters.ps1", Array.Empty<string>()));
            UnlockBtn.IsEnabled = true;
            if (r.Code == 0) Toast(T("unlockDone"));
            else Toast(F("unlockFail", Tail(r.Output).Trim().Split('\n').LastOrDefault() ?? ""), warn: true);
        }
    }
}
