using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Net.Http;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using OpenSmash.Launcher;
using static NativeSmash.Words;
using Brush = System.Windows.Media.Brush;
using Orientation = System.Windows.Controls.Orientation;
using HorizontalAlignment = System.Windows.HorizontalAlignment;
using VerticalAlignment = System.Windows.VerticalAlignment;
using FontStyle = System.Windows.FontStyle;
using Image = System.Windows.Controls.Image;
using Color = System.Windows.Media.Color;

namespace NativeSmash
{
    // Online: the Discord account (OpenSmash's DiscordAccount: the same token file, config\discord_token.txt, the game
    // and OpenSmash's launcher use), the GSP per fighter and the ranking; the lobby (online_server.txt, as the game's
    // own settings menu writes it), the online name, direct connections and the Windows firewall's rule.
    public partial class MainWindow
    {
        DiscordToken? discord;
        bool discordBusy, discordApp;
        CancellationTokenSource? discordCancel;
        byte[]? avatar;

        void InitOnline()
        {
            OnlineTitle.Text = T("onlineTitle"); OnlineHelp.Text = T("onlineHelp");
            DcTitle.Text = T("dcTitle"); DcHelp.Text = T("dcHelp"); DcSignIn.Content = T("signIn"); DcCancel.Content = T("dcCancel");
            DcSignOut.Content = T("dcSignOut"); DcShown.Content = T("dcShown"); GspLabel.Text = T("gspLabel"); GspEmpty.Text = T("gspEmpty");
            LobbyTitle.Text = T("lobbyTitle"); LobbyHelp.Text = T("lobbyHelp"); NameLabel.Text = T("name"); LobbyLabel.Text = T("lobby");
            CodeLabel.Text = T("code"); CodeNew.Content = T("codeNew"); CodeCopy.Content = T("codeCopy"); CodeHelp.Text = T("codeHelp");
            ServerLabel.Text = T("server"); DirectConn.Content = T("directConn"); DirectHelp.Text = T("directHelp");
            FirewallBtn.Content = T("firewallBtn"); FirewallHelp.Text = T("firewallHelp");
            HowTitle.Text = T("howTitle"); HowText.Text = T("howText");

            discordApp = DiscordAccount.AppId(repo) != "";
            DcShown.IsChecked = DiscordAccount.ShowsName(repo);
            DcSignIn.Click += (_, _) => DiscordSignIn();
            DcCancel.Click += (_, _) => discordCancel?.Cancel();
            DcSignOut.Click += (_, _) => DiscordSignOut();
            DcShown.Click += (_, _) => DiscordToggleShown();

            NameBox.Text = Settings.Name(repo);
            NameBox.LostFocus += (_, _) => Save(() => { if (NameBox.Text.Trim() != Settings.Name(repo)) Settings.SetName(repo, NameBox.Text); });
            var (lobby, code, server) = Settings.Lobby(repo);
            lobbyNow = lobby;
            CodeBox.Text = code; ServerBox.Text = server;
            Chips(LobbyChips, new[] { "auto", "private", "custom", "lan", "offline" }.Select(v => (v, T("lobby_" + v))), lobby, v => { lobbyNow = v; LobbyRows(); CommitOnline(); });
            CodeBox.LostFocus += (_, _) => CommitOnline();
            ServerBox.LostFocus += (_, _) => CommitOnline();
            CodeNew.Click += (_, _) => { CodeBox.Text = NewCode(); CommitOnline(); CopyCode(); };
            CodeCopy.Click += (_, _) => CopyCode();
            DirectConn.IsChecked = Settings.Direct(repo);
            DirectConn.Click += (_, _) => Save(() => Settings.SetDirect(repo, DirectConn.IsChecked == true));
            FirewallBtn.Click += (_, _) => AddFirewallRule();
            LobbyRows();
            DiscordShow();
        }

        string lobbyNow = "auto";

        void LobbyRows()
        {
            CodeRow.Visibility = lobbyNow is "private" or "custom" ? Visibility.Visible : Visibility.Collapsed;
            CodeNew.Visibility = CodeCopy.Visibility = lobbyNow == "private" ? Visibility.Visible : Visibility.Collapsed;
            CodeHelp.Visibility = lobbyNow == "private" ? Visibility.Visible : Visibility.Collapsed;
            ServerRow.Visibility = lobbyNow == "custom" ? Visibility.Visible : Visibility.Collapsed;
            bool on = lobbyNow != "offline";
            NameBox.IsEnabled = DirectConn.IsEnabled = on;
        }

        // The lobby fields written (online_server.txt): false when something must be fixed first.
        bool CommitOnline()
        {
            if (loading) return true;
            string why;
            try { why = Settings.SetLobby(repo, lobbyNow, CodeBox.Text, ServerBox.Text); }
            catch (Exception ex) { Toast(ex.Message, warn: true); return false; }
            LobbyNote.Text = why switch { "code" => T("needCode"), "server" => T("badServer"), _ => "" };
            LobbyNote.Visibility = why == "" ? Visibility.Collapsed : Visibility.Visible;
            try { if (NameBox.Text.Trim() != Settings.Name(repo)) Settings.SetName(repo, NameBox.Text); } catch { }
            return why == "";
        }

        static string NewCode()
        {
            const string abc = "ABCDEFGHJKMNPQRSTUVWXYZ23456789";
            return new string(Enumerable.Range(0, 6).Select(_ => abc[Random.Shared.Next(abc.Length)]).ToArray());
        }

        void CopyCode()
        {
            string c = CodeBox.Text.Trim();
            if (c == "") return;
            try { System.Windows.Clipboard.SetText(c); Toast(T("codeCopied")); } catch { }
        }

        // ---- Windows firewall: the installer's rule for this folder's game ---------------------------------------
        const string FirewallRule = "WindowSmash (ssbu.exe)";
        bool firewallAsked;

        async void RefreshFirewall()
        {
            if (firewallAsked) return;
            firewallAsked = true;
            string exe = Path.Combine(buildDir, "ssbu.exe");
            bool ok = await Task.Run(() =>
            {
                string ps = $"$ok=$false; foreach($r in @(Get-NetFirewallRule -DisplayName '{FirewallRule}' -ErrorAction SilentlyContinue)){{ $f=$r|Get-NetFirewallApplicationFilter -ErrorAction SilentlyContinue; if($f -and $f.Program -ieq '{exe.Replace("'", "''")}'){{$ok=$true}} }}; if($ok){{'yes'}}else{{'no'}}";
                var r = Launch.Run("powershell.exe", new[] { "-NoProfile", "-Command", ps }, repo);
                return r.Output.Trim().EndsWith("yes");
            });
            if (ok) { FirewallBtn.Visibility = Visibility.Collapsed; FirewallHelp.Text = "✓  " + T("firewallOk"); FirewallHelp.Foreground = (Brush)FindResource("Good"); }
        }

        async void AddFirewallRule()
        {
            string exe = Path.Combine(buildDir, "ssbu.exe");
            string cmd = $"New-NetFirewallRule -DisplayName '{FirewallRule}' -Direction Inbound -Program '{exe.Replace("'", "''")}' -Action Allow -Profile Any | Out-Null";
            int code = await Task.Run(() =>
            {
                try
                {
                    using var p = Process.Start(new ProcessStartInfo("powershell.exe", $"-NoProfile -WindowStyle Hidden -Command \"{cmd}\"") { UseShellExecute = true, Verb = "runas", WindowStyle = ProcessWindowStyle.Hidden });
                    p?.WaitForExit();
                    return p?.ExitCode ?? -1;
                }
                catch { return -1; }
            });
            firewallAsked = false;
            if (code == 0) RefreshFirewall(); else Toast(T("firewallFail"), warn: true);
        }

        // ---- Discord ---------------------------------------------------------------------------------------------
        List<FighterGsp> gsp = new();
        int rank;
        Dictionary<int, int> fighterRanks = new();

        async Task DiscordRefresh()
        {
            if (!discordApp) { DiscordShow(); return; }
            try
            {
                discord = await DiscordAccount.Current(repo, CancellationToken.None);
                await DiscordExtras();
                if (discord != null && DcShown.IsChecked == true) await DiscordAccount.SetShown(repo, discord, true, CancellationToken.None);
            }
            catch (Exception e) when (e is HttpRequestException or TaskCanceledException or IOException or InvalidOperationException or System.Text.Json.JsonException) { }
            DiscordShow();
        }

        async Task DiscordExtras()
        {
            if (discord == null) return;
            try
            {
                string url = DiscordAccount.AvatarUrl(discord);
                if (url != "") avatar = await http.GetByteArrayAsync(url);
            }
            catch (Exception e) when (e is HttpRequestException or TaskCanceledException or ArgumentException) { }
            try { gsp = await DiscordAccount.Records(repo, CancellationToken.None); }
            catch (Exception e) when (e is HttpRequestException or TaskCanceledException or IOException or System.Text.Json.JsonException) { }
            try { (rank, fighterRanks) = await DiscordAccount.Ranks(repo, discord, CancellationToken.None); }
            catch (Exception e) when (e is HttpRequestException or TaskCanceledException or IOException or System.Text.Json.JsonException) { }
        }

        async void DiscordSignIn()
        {
            if (discordBusy) return;
            if (!discordApp) { NavOnline.IsChecked = true; DiscordError(T("dcNoApp")); return; }
            discordBusy = true; DiscordError("");
            discordCancel = new CancellationTokenSource();
            DiscordShow();
            try
            {
                discord = await DiscordAccount.SignIn(repo, discordCancel.Token);
                await DiscordExtras();
                if (DcShown.IsChecked == true) _ = DiscordAccount.SetShown(repo, discord, true, CancellationToken.None);
                Activate();
            }
            catch (OperationCanceledException) { DiscordError(T("dcCancelled")); }
            catch (Exception e) when (e is InvalidOperationException or HttpRequestException or System.Text.Json.JsonException or KeyNotFoundException or IOException)
            {
                DiscordError(e.Message);
            }
            discordBusy = false;
            DiscordShow();
        }

        void DiscordSignOut()
        {
            if (DcShown.IsChecked == true) _ = DiscordAccount.SetShown(repo, discord, false, CancellationToken.None);
            DcShown.IsChecked = false;
            try { DiscordAccount.SignOut(repo); } catch { }
            discord = null; avatar = null; rank = 0; gsp = new(); fighterRanks = new();
            DiscordShow();
            _ = CommunityRefresh();
        }

        async void DiscordToggleShown()
        {
            bool shown = DcShown.IsChecked == true;
            try
            {
                int took = await DiscordAccount.SetShown(repo, discord, shown, CancellationToken.None);
                DiscordError(took == 0 && DiscordAccount.NsaId(repo) == "" ? T("dcNeedPlay") : "");
                _ = CommunityRefresh();
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException or HttpRequestException or TaskCanceledException) { DiscordError(e.Message); }
        }

        void DiscordError(string text)
        {
            DcError.Text = text;
            DcError.Visibility = text == "" ? Visibility.Collapsed : Visibility.Visible;
        }

        // The account everywhere it shows: Home's card, the side menu, the Online page.
        void DiscordShow()
        {
            bool inn = discord != null;
            var pic = Picture(avatar);
            string rankText = rank > 0
                ? (gsp.Count > 0 ? F("rankBest", rank, gsp.Max(g => g.Gsp).ToString("N0", CultureInfo.CurrentCulture)) : F("rankIs", rank))
                : T("rankNone");

            AccEyebrow.Text = inn ? T("accEyebrowIn") : T("accEyebrow");
            AccOut.Visibility = inn ? Visibility.Collapsed : Visibility.Visible;
            AccIn.Visibility = inn ? Visibility.Visible : Visibility.Collapsed;
            AccSignIn.IsEnabled = !discordBusy;
            AccSignIn.Content = discordBusy ? T("dcBusy") : T("signIn");
            if (!discordApp) { AccSignIn.Visibility = Visibility.Collapsed; AccOutHelp.Text = T("dcNoApp"); }

            SideMe.Visibility = inn ? Visibility.Visible : Visibility.Collapsed;
            DcOut.Visibility = inn ? Visibility.Collapsed : Visibility.Visible;
            DcIn.Visibility = inn ? Visibility.Visible : Visibility.Collapsed;
            DcSignIn.IsEnabled = !discordBusy && discordApp;
            DcSignIn.Content = discordBusy ? T("dcBusy") : T("signIn");
            DcCancel.Visibility = discordBusy ? Visibility.Visible : Visibility.Collapsed;
            if (!discordApp) DiscordError(T("dcNoApp"));
            if (!inn) return;

            string name = discord!.Shown;
            AccName.Text = name; SideName.Text = name; DcName.Text = name;
            AccRank.Text = rankText; DcRank.Text = rankText; SideRank.Text = rank > 0 ? "#" + rank + " GSP" : "Discord";
            AccAvatar.Fill = pic; SideAvatar.Fill = pic; DcAvatar.Fill = pic;

            GspList.Children.Clear();
            GspEmpty.Visibility = gsp.Count == 0 ? Visibility.Visible : Visibility.Collapsed;
            foreach (var r in gsp.OrderByDescending(g => g.Gsp).Take(12))
            {
                var sp = new StackPanel();
                sp.Children.Add(new TextBlock { Text = DiscordAccount.FighterName(r.Fighter), Foreground = (Brush)FindResource("Ink"), FontSize = 14, FontWeight = FontWeights.SemiBold });
                string line = r.Gsp.ToString("N0", CultureInfo.CurrentCulture) + " GSP";
                if (fighterRanks.TryGetValue(r.Fighter, out int fr) && fr > 0) line += "   ·   #" + fr;
                sp.Children.Add(new TextBlock { Text = line, Foreground = (Brush)FindResource("Muted"), FontSize = 12, Margin = new Thickness(0, 2, 0, 0) });
                GspList.Children.Add(new Border
                {
                    CornerRadius = new CornerRadius(12), Background = (Brush)FindResource("Veil"), BorderBrush = (Brush)FindResource("Edge"),
                    BorderThickness = new Thickness(1), Padding = new Thickness(14, 10, 14, 10), Margin = new Thickness(0, 0, 10, 10), MinWidth = 150, Child = sp,
                });
            }
        }
    }
}
