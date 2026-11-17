using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Threading;
using static NativeSmash.Words;
using Brush = System.Windows.Media.Brush;
using Button = System.Windows.Controls.Button;
using Color = System.Windows.Media.Color;
using ColorConverter = System.Windows.Media.ColorConverter;
using HorizontalAlignment = System.Windows.HorizontalAlignment;
using KeyEventArgs = System.Windows.Input.KeyEventArgs;
using Orientation = System.Windows.Controls.Orientation;
using VerticalAlignment = System.Windows.VerticalAlignment;

namespace NativeSmash
{
    // The NativeSmash account (Players.cs: the project's own players server, accounts made on its web page): signing in,
    // being seen online (also while playing), friends, the players online and invitations. An accepted invitation puts both
    // players in the same private lobby (a code the server makes, online_server.txt) and starts the game; the lobby the
    // player had comes back when the game closes. The lobby, Epic's relays and the game are the same as everyone's, so this
    // adds to OpenSmash's online play instead of splitting it.
    public partial class MainWindow
    {
        Players players = null!;
        readonly DispatcherTimer accountTimer = new() { Interval = TimeSpan.FromSeconds(2) };
        DateTime nextBeat, nextList;
        (long Id, string Code, string Name)? sentInvite;
        Players.Invite? shownInvite;
        HashSet<long> seenInvites = new();
        string? lobbyBefore;   // online_server.txt's line before an invitation's match (null: none to put back)
        Players.Profile? profile;
        List<Players.Player> friends = new(), online = new();
        bool accountBusy;

        void InitAccount()
        {
            players = new Players(repo, http);
            AccEmailHint.Text = T("email"); AccPassHint.Text = T("password");
            AccSignIn.Content = T("nsSignIn"); AccSignUp.Content = T("nsSignUp"); AccForgot.Content = T("nsForgot");
            NsTitle.Text = T("nsTitle"); NsHelp.Text = T("nsHelp"); NsGoHome.Content = T("nsGoHome"); NsSignUp.Content = T("nsSignUp");
            NsProfile.Content = T("nsProfile"); NsSignOut.Content = T("nsSignOut"); NsCancelInvite.Content = T("cancelInvite");
            FriendsLabel.Text = T("friends"); FriendsEmpty.Text = T("friendsEmpty"); FriendAdd.Content = T("friendAdd");
            PlayersLabel.Text = T("playersLabel"); PlayersEmpty.Text = T("playersEmpty");
            InviteYes.Content = T("accept"); InviteNo.Content = T("decline");
            NsCount.Text = "…";

            AccEmail.TextChanged += (_, _) => AccEmailHint.Visibility = AccEmail.Text == "" ? Visibility.Visible : Visibility.Collapsed;
            AccPass.PasswordChanged += (_, _) => AccPassHint.Visibility = AccPass.Password == "" ? Visibility.Visible : Visibility.Collapsed;
            AccPass.KeyDown += (_, e) => { if (e.Key == Key.Enter) { e.Handled = true; AccountSignIn(); } };
            AccSignIn.Click += (_, _) => AccountSignIn();
            AccSignUp.Click += (_, _) => Launch.Open(players.SignupPage != "" ? players.SignupPage : "https://gomeztorressandygabriel-cloud.github.io/NativeSmash/cuenta.html");
            NsSignUp.Click += (_, _) => Launch.Open(players.SignupPage);
            AccForgot.Click += (_, _) => Launch.Open(players.SignupPage);
            NsGoHome.Click += (_, _) => { NavHome.IsChecked = true; AccEmail.Focus(); };
            NsSignOut.Click += async (_, _) => { await players.SignOut(); profile = null; friends.Clear(); online.Clear(); AccountShow(); };
            NsProfile.Click += (_, _) => Launch.Open(SiteBase() + "perfil.html?u=" + Uri.EscapeDataString(players.Name));
            NsCancelInvite.Click += async (_, _) =>
            {
                if (sentInvite is { } s) { try { await players.CancelInvite(s.Id); } catch { } Status(F("inviteCancelled", s.Name)); }
                sentInvite = null; NsCancelInvite.Visibility = Visibility.Collapsed;
            };
            FriendAdd.Click += (_, _) => AddFriend();
            FriendBox.KeyDown += (_, e) => { if (e.Key == Key.Enter) { e.Handled = true; AddFriend(); } };
            InviteYes.Click += (_, _) => AnswerShownInvite(true);
            InviteNo.Click += (_, _) => AnswerShownInvite(false);
            accountTimer.Tick += async (_, _) => await AccountTick();

            if (!players.On) { NsOut.Visibility = Visibility.Collapsed; AccSignIn.IsEnabled = false; }
            AccountShow();
            Loaded += async (_, _) =>
            {
                if (!players.On) return;
                await players.Resume();
                AccountShow();
                nextBeat = nextList = DateTime.MinValue;
                await AccountTick();
                accountTimer.Start();
            };
        }

        string SiteBase()
        {
            string p = players.SignupPage;
            int slash = p.LastIndexOf('/');
            return slash > 0 ? p[..(slash + 1)] : p;
        }

        void AccountClosing()
        {
            accountTimer.Stop();
            if (players?.SignedIn == true) try { players.GoOffline().Wait(2000); } catch { }
            RestoreLobby();
        }

        // ---- signing in ---------------------------------------------------------------------------------------------
        async void AccountSignIn()
        {
            if (accountBusy) return;
            string email = AccEmail.Text.Trim(), pass = AccPass.Password;
            if (email == "" || pass == "") { AccountError(T("needBoth")); return; }
            accountBusy = true; AccSignIn.IsEnabled = false; AccSignIn.Content = T("signingIn"); AccountError("");
            try
            {
                await players.SignIn(email, pass);
                AccPass.Clear();
                // (the game's online name follows the account's, unless the player chose one of their own)
                string mine = Settings.Name(repo);
                if (players.Name != "" && (mine == "" || System.Text.RegularExpressions.Regex.IsMatch(mine, @"^Player\d+$")))
                {
                    try { Settings.SetName(repo, players.Name); NameBox.Text = players.Name; } catch { }
                }
                nextBeat = nextList = DateTime.MinValue;
                await AccountTick();
                accountTimer.Start();
            }
            catch (Players.ServerError e) { AccountError(ServerWords(e.Message)); }
            catch (Exception e) when (e is HttpRequestException or TaskCanceledException) { AccountError(T("noServer")); }
            accountBusy = false; AccSignIn.IsEnabled = true; AccSignIn.Content = T("nsSignIn");
            AccountShow();
        }

        static string ServerWords(string m)
        {
            string l = m.ToLowerInvariant();
            if (l.Contains("invalid login")) return T("badLogin");
            if (l.Contains("not confirmed")) return T("notConfirmed");
            if (l.Contains("not online")) return T("notOnline");
            if (l.Contains("no longer there")) return T("inviteGone");
            if (l.Contains("too many")) return T("tooMany");
            if (l.Contains("no such") || l.Contains("not found")) return T("noSuchPlayer");
            return F("netError", m);
        }

        void AccountError(string text) { AccError.Text = text; AccError.Visibility = text == "" ? Visibility.Collapsed : Visibility.Visible; }

        void Status(string text) { NsStatus.Text = text; NsStatus.Visibility = text == "" ? Visibility.Collapsed : Visibility.Visible; if (text != "") Toast(text); }

        // ---- the loop: being online, the lists, invitations -----------------------------------------------------------
        int tickBusy;

        async Task AccountTick()
        {
            if (Interlocked.Exchange(ref tickBusy, 1) == 1) return;
            try
            {
                var now = DateTime.UtcNow;
                if (!players.SignedIn)
                {
                    if (now >= nextList) { try { NsCount.Text = F("nsOnline", await players.OnlineCount()); } catch { } nextList = now.AddSeconds(15); }
                    return;
                }
                if (now >= nextBeat)
                {
                    try { await players.Heartbeat(GameRunning() ? "playing" : "launcher", "launcher-0.3"); } catch { }
                    nextBeat = now.AddSeconds(20);
                }
                if (sentInvite is { } s)
                {
                    string st = "";
                    try { st = await players.InviteStatus(s.Id); } catch { }
                    if (st == "accepted") { sentInvite = null; NsCancelInvite.Visibility = Visibility.Collapsed; StartMatch(s.Code, s.Name); }
                    else if (st is "declined" or "expired" or "cancelled") { sentInvite = null; NsCancelInvite.Visibility = Visibility.Collapsed; Status(F("inviteOver", s.Name)); }
                }
                if (now >= nextList)
                {
                    nextList = now.AddSeconds(4);
                    try
                    {
                        var invites = await players.Invites();
                        online = await players.Online();
                        friends = await players.Friends();
                        NsCount.Text = F("nsOnline", await players.OnlineCount());
                        if (profile == null || now.Second < 5) profile = await players.MyProfile();
                        ShowInvite(invites);
                    }
                    catch (Players.ServerError e) when (e.Message.Contains("JWT", StringComparison.OrdinalIgnoreCase)) { await players.Resume(); }
                    catch { }
                    AccountShow();
                }
            }
            finally { Interlocked.Exchange(ref tickBusy, 0); }
        }

        // ---- invitations ----------------------------------------------------------------------------------------------
        void ShowInvite(List<Players.Invite> invites)
        {
            var inv = invites.FirstOrDefault();
            shownInvite = inv;
            InviteBanner.Visibility = inv == null || playing ? Visibility.Collapsed : Visibility.Visible;
            if (inv == null) return;
            InviteText.Text = F("invitedYou", inv.Name);
            if (seenInvites.Add(inv.Id) && IsVisible)
            {
                try { System.Media.SystemSounds.Exclamation.Play(); } catch { }
                NavHome.IsChecked = true;
                if (WindowState == WindowState.Minimized) WindowState = WindowState.Normal;
                Activate();
            }
        }

        async void AnswerShownInvite(bool accept)
        {
            if (shownInvite is not { } inv) return;
            InviteBanner.Visibility = Visibility.Collapsed;
            shownInvite = null;
            try
            {
                string code = await players.AnswerInvite(inv.Id, accept);
                if (accept && code != "") StartMatch(code, inv.Name);
            }
            catch (Players.ServerError e) { Toast(ServerWords(e.Message), warn: true); }
            catch (Exception e) when (e is HttpRequestException or TaskCanceledException) { Toast(T("noServer"), warn: true); }
        }

        async void InvitePlayer(Players.Player p)
        {
            if (sentInvite != null) { Toast(T("inviteWait"), warn: true); return; }
            try
            {
                var (id, code) = await players.SendInvite(p.Id);
                sentInvite = (id, code, p.Name);
                Status(F("inviteSent", p.Name));
                NsCancelInvite.Visibility = Visibility.Visible;
            }
            catch (Players.ServerError e) { Toast(ServerWords(e.Message), warn: true); }
            catch (Exception e) when (e is HttpRequestException or TaskCanceledException) { Toast(T("noServer"), warn: true); }
        }

        // Both players in the same private lobby, "NS" and the server's code, and the game started; the lobby they had
        // comes back when the game closes (RestoreLobby, from Play).
        void StartMatch(string code, string with)
        {
            if (GameRunning()) { Toast(T("gameRunning"), warn: true); return; }
            try
            {
                string path = Settings.File(repo, "online_server.txt");
                lobbyBefore ??= File.Exists(path) ? Settings.FirstLine(path) : "";
                Settings.SetLobby(repo, "private", "NS" + code, "");
            }
            catch (Exception e) { Toast(e.Message, warn: true); return; }
            Status(F("matchWith", with, "NS" + code));
            NavHome.IsChecked = true;
            Play();
        }

        internal void RestoreLobby()
        {
            if (lobbyBefore == null) return;
            try
            {
                string path = Settings.File(repo, "online_server.txt");
                if (lobbyBefore == "") { if (File.Exists(path)) File.Delete(path); }
                else Settings.SetFirstLine(path, lobbyBefore);
            }
            catch { }
            lobbyBefore = null;
            ReloadLobby();
        }

        void ReloadLobby()
        {
            var (lobby, code, server) = Settings.Lobby(repo);
            bool was = loading; loading = true;
            lobbyNow = lobby; Pick(LobbyChips, lobby); CodeBox.Text = code; ServerBox.Text = server; LobbyRows();
            loading = was;
        }

        // ---- friends ----------------------------------------------------------------------------------------------------
        async void AddFriend()
        {
            string name = FriendBox.Text.Trim();
            if (name == "") return;
            try
            {
                string r = await players.FriendRequest(name);
                Status(r == "accepted" ? F("friendNow", name) : F("friendSent", name));
                FriendBox.Clear();
                nextList = DateTime.MinValue;
            }
            catch (Players.ServerError e) { Toast(ServerWords(e.Message), warn: true); }
            catch (Exception e) when (e is HttpRequestException or TaskCanceledException) { Toast(T("noServer"), warn: true); }
        }

        async void FriendDo(Func<Task> act)
        {
            try { await act(); nextList = DateTime.MinValue; await AccountTick(); }
            catch (Players.ServerError e) { Toast(ServerWords(e.Message), warn: true); }
            catch (Exception e) when (e is HttpRequestException or TaskCanceledException) { Toast(T("noServer"), warn: true); }
        }

        // ---- what shows -------------------------------------------------------------------------------------------------
        static Brush AvatarBrush(string hex, Brush fallback)
        {
            try { if (hex.StartsWith("#")) return new SolidColorBrush((Color)ColorConverter.ConvertFromString(hex)); } catch { }
            return fallback;
        }

        static string Initial(string name) => name == "" ? "?" : name[..1].ToUpperInvariant();

        string StateWords(Players.Player p) => p.Relation switch
        {
            "incoming" => T("friendIncoming"),
            "outgoing" => T("friendOutgoing"),
            _ => p.State switch { "playing" => T("stPlaying"), "searching" => T("stSearching"), "launcher" => T("stLauncher"), _ => T("friendOffline") },
        };

        void AccountShow()
        {
            bool inn = players.SignedIn;
            AccEyebrow.Text = inn ? T("accEyebrowIn") : T("accEyebrow");
            AccOut.Visibility = inn ? Visibility.Collapsed : Visibility.Visible;
            AccIn.Visibility = inn ? Visibility.Visible : Visibility.Collapsed;
            NsOut.Visibility = inn || !players.On ? Visibility.Collapsed : Visibility.Visible;
            NsIn.Visibility = inn ? Visibility.Visible : Visibility.Collapsed;
            SideMe.Visibility = inn ? Visibility.Visible : Visibility.Collapsed;
            HomeFriends.Visibility = inn ? Visibility.Visible : Visibility.Collapsed;
            if (!inn) { InviteBanner.Visibility = Visibility.Collapsed; return; }

            var accent = (Brush)FindResource("Accent");
            var pic = AvatarBrush(profile?.Avatar ?? "", accent);
            string name = players.Name;
            string rank = profile is { } pr
                ? (pr.Wins + pr.Losses > 0 ? F("nsRankWl", pr.Rating, pr.Place, pr.Wins, pr.Losses) : F("nsRank", pr.Rating, pr.Place))
                : "";
            AccName.Text = name; NsName.Text = name; SideName.Text = name;
            AccRank.Text = rank; NsRank.Text = rank; SideRank.Text = profile != null ? F("nsPts", profile.Rating) : "NativeSmash";
            AccInitial.Text = NsInitial.Text = SideInitial.Text = Initial(name);
            AccAvatar.Background = NsAvatar.Background = SideAvatar.Background = pic;

            // home: friends online, with Invite
            HomeFriends.Children.Clear();
            var on = friends.Where(f => f.Relation == "friend" && f.State is "launcher" or "playing" or "searching").Take(4).ToList();
            if (on.Count > 0)
                HomeFriends.Children.Add(new TextBlock { Text = T("friendsOnline"), Style = (Style)FindResource("Eyebrow"), Margin = new Thickness(0, 4, 0, 6) });
            foreach (var f in on) HomeFriends.Children.Add(PersonRow(f, small: true));

            // online page: friends and players
            FriendsList.Children.Clear();
            foreach (var f in friends) FriendsList.Children.Add(PersonRow(f, small: false));
            FriendsEmpty.Visibility = friends.Count == 0 ? Visibility.Visible : Visibility.Collapsed;
            PlayersList.Children.Clear();
            var friendIds = friends.Select(f => f.Id).ToHashSet();
            foreach (var p in online.Where(p => !friendIds.Contains(p.Id)).Take(30)) PlayersList.Children.Add(PersonRow(p, small: false));
            PlayersEmpty.Visibility = PlayersList.Children.Count == 0 ? Visibility.Visible : Visibility.Collapsed;
            PlayersEmpty.Text = T("playersEmpty");
        }

        FrameworkElement PersonRow(Players.Player p, bool small)
        {
            var g = new Grid { Margin = new Thickness(0, small ? 3 : 5, 0, small ? 3 : 5) };
            g.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
            g.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
            g.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
            double size = small ? 28 : 36;
            bool isOn = p.State is "launcher" or "playing" or "searching";
            var av = new Grid { Width = size, Height = size };
            av.Children.Add(new Border
            {
                CornerRadius = new CornerRadius(size / 2), Background = AvatarBrush(p.Avatar, (Brush)FindResource("Accent")),
                Child = new TextBlock { Text = Initial(p.Name), Foreground = System.Windows.Media.Brushes.White, FontSize = small ? 12 : 15, FontWeight = FontWeights.SemiBold, HorizontalAlignment = HorizontalAlignment.Center, VerticalAlignment = VerticalAlignment.Center },
            });
            av.Children.Add(new System.Windows.Shapes.Ellipse
            {
                Width = small ? 9 : 11, Height = small ? 9 : 11, HorizontalAlignment = HorizontalAlignment.Right, VerticalAlignment = VerticalAlignment.Bottom,
                Fill = isOn ? (Brush)FindResource("Good") : new SolidColorBrush(Color.FromRgb(0x5A, 0x60, 0x78)), Stroke = new SolidColorBrush(Color.FromRgb(0x10, 0x13, 0x28)), StrokeThickness = 2,
            });
            var text = new StackPanel { Margin = new Thickness(small ? 10 : 12, 0, 8, 0), VerticalAlignment = VerticalAlignment.Center };
            text.Children.Add(new TextBlock { Text = p.Name, Foreground = (Brush)FindResource("Ink"), FontSize = small ? 13 : 14, FontWeight = FontWeights.SemiBold, TextTrimming = TextTrimming.CharacterEllipsis });
            string sub = StateWords(p) + (p.Rating > 0 && !small ? "  ·  " + F("nsPts", p.Rating) : "");
            text.Children.Add(new TextBlock { Text = sub, Foreground = (Brush)FindResource("Muted"), FontSize = small ? 11 : 12 });
            Grid.SetColumn(text, 1);
            var buttons = new StackPanel { Orientation = Orientation.Horizontal, VerticalAlignment = VerticalAlignment.Center };
            Button B(string label, Action act, bool accent = false)
            {
                var b = new Button { Content = label, Style = (Style)FindResource(accent ? "AccentBtn" : "Small"), Margin = new Thickness(6, 0, 0, 0) };
                if (accent) { b.Padding = new Thickness(0); b.FontSize = 13; }
                b.Click += (_, _) => act();
                return b;
            }
            if (p.Relation == "incoming")
            {
                buttons.Children.Add(B(T("accept"), () => FriendDo(() => players.FriendAnswer(p.Name, true))));
                buttons.Children.Add(B(T("decline"), () => FriendDo(() => players.FriendAnswer(p.Name, false))));
            }
            else
            {
                if (isOn && p.State != "playing") buttons.Children.Add(B(T("invite"), () => InvitePlayer(p)));
                if (!small && p.Relation is "friend" or "outgoing") buttons.Children.Add(B(T("friendRemove"), () => FriendDo(() => players.FriendRemove(p.Name))));
                if (!small && p.Relation == "") buttons.Children.Add(B(T("friendAdd"), () => FriendDo(async () => { await players.FriendRequest(p.Name); Status(F("friendSent", p.Name)); })));
            }
            Grid.SetColumn(buttons, 2);
            g.Children.Add(av); g.Children.Add(text); g.Children.Add(buttons);
            return g;
        }
    }
}
