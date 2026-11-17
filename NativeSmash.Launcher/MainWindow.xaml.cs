using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Net.Http;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Media.Imaging;
using System.Windows.Threading;
using OpenSmash.Launcher;
using static NativeSmash.Words;
using Brush = System.Windows.Media.Brush;
using Orientation = System.Windows.Controls.Orientation;
using HorizontalAlignment = System.Windows.HorizontalAlignment;
using VerticalAlignment = System.Windows.VerticalAlignment;
using FontStyle = System.Windows.FontStyle;
using Image = System.Windows.Controls.Image;
using Button = System.Windows.Controls.Button;
using CheckBox = System.Windows.Controls.CheckBox;
using Color = System.Windows.Media.Color;
using KeyEventArgs = System.Windows.Input.KeyEventArgs;
using MessageBox = System.Windows.MessageBox;
using RadioButton = System.Windows.Controls.RadioButton;
using TextBox = System.Windows.Controls.TextBox;

namespace NativeSmash
{
    // The window: a side menu (Home, Online, Controllers, Video and sound, Mods, Shaders, Game) and its pages.
    // Home: Play, the game's state, and the right column (the Discord account, players online now, the GSP ranking).
    public partial class MainWindow : Window
    {
        readonly string repo, workDir, buildDir, startPage, shotPath;
        internal readonly HttpClient http = new() { Timeout = TimeSpan.FromSeconds(30) };
        readonly DispatcherTimer communityTimer = new() { Interval = CommunityFeed.Every };
        readonly DispatcherTimer presenceTimer = new() { Interval = TimeSpan.FromMilliseconds(100) };
        DiscordPresence? presence;
        Process? game;
        bool loading = true;

        public MainWindow(string repo, string workDir, string page, string shot)
        {
            this.repo = repo; this.workDir = workDir; buildDir = WindowSmash.BuildDir(repo);
            startPage = page; shotPath = shot;
            http.DefaultRequestHeaders.UserAgent.ParseAdd("NativeSmash-launcher");
            Words.Pick(repo);
            InitializeComponent();
            ApplyWords();

            MouseLeftButtonDown += (_, e) => { if (e.ButtonState == MouseButtonState.Pressed && IsBackground(e.OriginalSource)) try { DragMove(); } catch { } };
            MinBtn.Click += (_, _) => WindowState = WindowState.Minimized;
            CloseBtn.Click += (_, _) => Close();
            foreach (var nav in new[] { NavHome, NavOnline, NavPads, NavVideo, NavMods, NavShaders, NavGame })
                nav.Checked += (s, _) => ShowPage((string)((RadioButton)s).Tag);
            PlayBtn.Click += (_, _) => Play();
            InstallBtn.Click += (_, _) => { if (Needs1305()) ApplyUpdate(buildOnly: true); else Repair(); };
            DirectBox.Click += (_, _) => Save(() => Settings.SetStartDirectly(repo, DirectBox.IsChecked == true));
            VanillaBox.Click += (_, _) => Save(() => Settings.SetLaunchMode(repo, VanillaBox.IsChecked == true ? "vanilla" : "mods"), quiet: true);
            UpdBannerBtn.Click += (_, _) => NavGame.IsChecked = true;
            BoardLink.Click += (_, _) => Launch.Open(CommunityFeed.SitePage);
            AccIn.MouseLeftButtonUp += (_, _) => NavOnline.IsChecked = true;
            SideMe.MouseLeftButtonUp += (_, _) => NavOnline.IsChecked = true;
            KeyDown += OnKey;

            InitOnline();
            InitAccount();
            InitPads();
            InitVideo();
            InitMods();
            InitShaders();
            InitGame();
            RefreshHome();
            loading = false;

            Loaded += async (_, _) =>
            {
                string p = startPage switch { "online" or "pads" or "controllers" or "video" or "mods" or "shaders" or "game" => startPage, _ => "home" };
                if (p == "controllers") { p = "pads"; }
                (p switch { "online" => NavOnline, "pads" => NavPads, "video" => NavVideo, "mods" => NavMods, "shaders" => NavShaders, "game" => NavGame, _ => NavHome }).IsChecked = true;
                if (startPage == "controllers") ShowPadTab("pads");
                FadeIn();
                if (shotPath != "")
                {
                    _ = CommunityRefresh(); _ = DiscordRefresh(); _ = RefreshModsAsync();
                    await Task.Delay(6000); Shot(shotPath); Close(); return;
                }
                if (Environment.GetCommandLineArgs().Contains("--click-play")) Dispatcher.BeginInvoke(Play, DispatcherPriority.Background);
                communityTimer.Tick += (_, _) => _ = CommunityRefresh();
                communityTimer.Start();
                _ = CommunityRefresh();
                _ = DiscordRefresh();
                PresenceStart();
                await Task.Delay(1500);
                _ = CheckUpdates(atStart: true);
                _ = RefreshModsAsync();
            };
            Closing += (_, _) => { PresenceStop(); StopPadSource(); AccountClosing(); };
        }

        // ---- words ----------------------------------------------------------------------------------------------
        void ApplyWords()
        {
            Tagline.Text = T("tagline");
            NavHome.Content = T("home"); NavOnline.Content = T("online"); NavPads.Content = T("pads"); NavVideo.Content = T("video");
            NavMods.Content = T("mods"); NavShaders.Content = T("shaders"); NavGame.Content = T("game");
            MinBtn.ToolTip = T("min"); CloseBtn.ToolTip = T("close");
            Footer.Text = F("footer", GameVersion(), FileVersionInfo.GetVersionInfo(Environment.ProcessPath ?? "").ProductVersion?.Split('+')[0] ?? "");
            PlayBtn.Content = T("playBtn"); InstallBtn.Content = T("install");
            VanillaBox.Content = T("vanilla"); DirectBox.Content = T("direct");
            UpdBannerBtn.Content = T("updBannerBtn");
            LobbyEyebrow.Text = T("lobbyEyebrow"); BoardEyebrow.Text = T("boardEyebrow"); BoardLink.Content = T("boardLink"); BoardEmpty.Text = T("boardEmpty");
            StartingText.Text = T("starting");
        }

        string GameVersion()
        {
            string v = WindowSmash.Version(repo);
            var m = System.Text.RegularExpressions.Regex.Match(v, @"\d+\.\d+\.\d+");
            return m.Success ? m.Value : "13.0.5";
        }

        // ---- pages ----------------------------------------------------------------------------------------------
        void ShowPage(string name)
        {
            var pages = new Dictionary<string, FrameworkElement>
            {
                ["home"] = PageHome, ["online"] = PageOnline, ["pads"] = PagePads, ["video"] = PageVideo,
                ["mods"] = PageMods, ["shaders"] = PageShaders, ["game"] = PageGame,
            };
            foreach (var (k, el) in pages)
            {
                bool on = k == name;
                el.Visibility = on ? Visibility.Visible : Visibility.Collapsed;
                if (on)
                {
                    el.Opacity = 0;
                    var tt = new TranslateTransform(0, 10);
                    el.RenderTransform = tt;
                    el.BeginAnimation(OpacityProperty, new DoubleAnimation(0, 1, TimeSpan.FromMilliseconds(180)));
                    tt.BeginAnimation(TranslateTransform.YProperty, new DoubleAnimation(10, 0, TimeSpan.FromMilliseconds(220)) { EasingFunction = new CubicEase { EasingMode = EasingMode.EaseOut } });
                }
            }
            if (name == "home") RefreshHome();
            if (name == "pads") PadsShown(); else PadsHidden();
            if (name == "mods") _ = RefreshModsAsync();
            if (name == "shaders") RefreshShaders();
            if (name == "online") RefreshFirewall();
            if (name == "game" && UpdStatus.Text == "" && !updBusy) _ = CheckUpdates(atStart: false);
        }

        static bool IsBackground(object source)
        {
            for (var d = source as DependencyObject; d != null; d = VisualTreeHelper.GetParent(d))
            {
                if (d is System.Windows.Controls.Primitives.ButtonBase or TextBox or Slider or System.Windows.Controls.Primitives.ScrollBar) return false;
                if (d is Window) return true;
            }
            return true;
        }

        void FadeIn()
        {
            Shell.Opacity = 0;
            Shell.BeginAnimation(OpacityProperty, new DoubleAnimation(0, 1, TimeSpan.FromMilliseconds(260)));
        }

        // ---- small helpers --------------------------------------------------------------------------------------
        // A group of chips (one of several choices): items as (value, label).
        internal void Chips(WrapPanel panel, IEnumerable<(string Value, string Label)> items, string selected, Action<string> picked)
        {
            panel.Children.Clear();
            string group = panel.Name + Guid.NewGuid().ToString("N")[..6];
            foreach (var (value, label) in items)
            {
                var rb = new RadioButton { Style = (Style)FindResource("Chip"), Content = label, Tag = value, GroupName = group, IsChecked = value == selected };
                rb.Checked += (_, _) => { if (!loading) picked(value); };
                panel.Children.Add(rb);
            }
        }

        internal static void Pick(WrapPanel panel, string value)
        {
            foreach (var c in panel.Children.OfType<RadioButton>()) if ((string)c.Tag == value) c.IsChecked = true;
        }

        DispatcherTimer? toastTimer;
        internal void Toast(string text, bool warn = false)
        {
            ToastText.Text = text;
            ToastBox.BorderBrush = new SolidColorBrush(warn ? Color.FromArgb(0x88, 0xFF, 0xB8, 0x6B) : Color.FromArgb(0x55, 0xA6, 0x6B, 0xFF));
            ToastBox.Visibility = Visibility.Visible;
            ToastBox.BeginAnimation(OpacityProperty, new DoubleAnimation(0, 1, TimeSpan.FromMilliseconds(160)));
            toastTimer?.Stop();
            toastTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(warn ? 6 : 3) };
            toastTimer.Tick += (_, _) => { toastTimer.Stop(); ToastBox.Visibility = Visibility.Collapsed; };
            toastTimer.Start();
        }

        // Writes a setting; a failure is said, not thrown.
        internal void Save(Action write, bool quiet = true)
        {
            if (loading) return;
            try { write(); if (!quiet) Toast(T("saved")); }
            catch (Exception ex) { Toast(ex.Message, warn: true); }
        }

        internal static ImageBrush? Picture(byte[]? bytes)
        {
            if (bytes == null || bytes.Length == 0) return null;
            try
            {
                var img = new BitmapImage();
                img.BeginInit(); img.StreamSource = new MemoryStream(bytes); img.CacheOption = BitmapCacheOption.OnLoad; img.EndInit(); img.Freeze();
                return new ImageBrush(img) { Stretch = Stretch.UniformToFill };
            }
            catch { return null; }
        }

        void OnKey(object sender, KeyEventArgs e)
        {
            if (KeyCapture(e)) return;
            if (e.Key == Key.Enter && PageHome.Visibility == Visibility.Visible && Keyboard.FocusedElement is not TextBox) { Play(); e.Handled = true; }
            if (e.Key == Key.Escape && Keyboard.FocusedElement is not TextBox) { e.Handled = true; Close(); }
        }

        // -Shot: the window drawn into a PNG (to check the look without a screen).
        void Shot(string path)
        {
            try
            {
                UpdateLayout();
                var dpi = VisualTreeHelper.GetDpi(this);
                var bmp = new RenderTargetBitmap((int)(ActualWidth * dpi.DpiScaleX), (int)(ActualHeight * dpi.DpiScaleY), dpi.PixelsPerInchX, dpi.PixelsPerInchY, PixelFormats.Pbgra32);
                bmp.Render(this);
                var enc = new PngBitmapEncoder(); enc.Frames.Add(BitmapFrame.Create(bmp));
                using var f = File.Create(path); enc.Save(f);
            }
            catch { }
        }

        // ---- home -----------------------------------------------------------------------------------------------
        void RefreshHome()
        {
            bool built = WindowSmash.IsBuilt(repo);
            string renderer = SettingDefaults.Renderer(Settings.Display(repo, "renderer"));
            string rlabel = renderer switch { "vulkan" => "VULKAN", "direct3d11" => "DIRECT3D 11", _ => "VULKAN" };
            Kicker.Text = F("kicker", GameVersion(), rlabel);
            bool old = Needs1305();
            PlayTitle.Text = old ? T("need1305") : built ? T("ready") : T("notready");
            PlayHelp.Text = old ? F("need1305Help", GameVersion()) : built ? T("readyHelp") : T("notreadyHelp");
            PlayBtn.IsEnabled = built && !old;
            InstallBtn.Content = old ? T("pick1305") : T("install");
            InstallBtn.Visibility = built && !old ? Visibility.Collapsed : Visibility.Visible;
            DirectBox.IsChecked = Settings.StartDirectly(repo);
            VanillaBox.IsChecked = Settings.LaunchMode(repo) == "vanilla";
            VanillaBox.Visibility = modsCached is { Total: > 0 } ? Visibility.Visible : Visibility.Collapsed;

            StatusChips.Children.Clear();
            void Chip(string text, Brush dot)
            {
                var sp = new StackPanel { Orientation = Orientation.Horizontal };
                sp.Children.Add(new System.Windows.Shapes.Ellipse { Width = 7, Height = 7, Fill = dot, Margin = new Thickness(0, 0, 8, 0), VerticalAlignment = VerticalAlignment.Center });
                sp.Children.Add(new TextBlock { Text = text, Foreground = (Brush)FindResource("Ink"), FontSize = 12 });
                StatusChips.Children.Add(new Border
                {
                    CornerRadius = new CornerRadius(10), Background = (Brush)FindResource("Veil"), BorderBrush = (Brush)FindResource("Edge"),
                    BorderThickness = new Thickness(1), Padding = new Thickness(12, 6, 12, 6), Margin = new Thickness(0, 0, 8, 8), Child = sp,
                });
            }
            var good = (Brush)FindResource("Good"); var warn = (Brush)FindResource("Warn"); var muted = (Brush)FindResource("Muted");
            if (!built) return;
            var (lobby, code, _) = Settings.Lobby(repo);
            Chip(F("chipLobby_" + lobby, code), lobby == "offline" ? muted : good);
            bool shadersOk = ShadersDone();
            Chip(shadersOk ? T("chipShadersOk") : T("chipShadersNo"), shadersOk ? good : warn);
            if (modsCached is { } m) Chip(m.Enabled > 0 && Settings.LaunchMode(repo) != "vanilla" ? F("chipMods", m.Enabled) : T("chipNoMods"), muted);
        }

        // ---- play -----------------------------------------------------------------------------------------------
        bool playing;

        async void Play()
        {
            if (playing) return;
            if (!WindowSmash.IsBuilt(repo)) { Toast(T("notBuilt"), warn: true); return; }
            if (Needs1305()) { RefreshHome(); Toast(T("need1305"), warn: true); return; }
            if (game != null && !game.HasExited) { BringToFront(game); return; }
            if (lobbyBefore == null && !CommitOnline()) { NavOnline.IsChecked = true; return; }   // (an invitation set its own lobby)
            playing = true;
            Starting.Visibility = Visibility.Visible;
            Starting.BeginAnimation(OpacityProperty, new DoubleAnimation(0, 1, TimeSpan.FromMilliseconds(160)));
            StartingSub.Text = T("startingSub");
            StartBar.Value = 60;
            try
            {
                await Task.Run(() => Launch.EnsureOnlineFiles(repo));
                StartBar.Value = 200;
                string mode = Settings.LaunchMode(repo);
                if (modsCached is { Total: > 0 } || mode == "vanilla")
                {
                    StartingSub.Text = T("startingMods");
                    string why = await Task.Run(() => Launch.PrepareMods(repo, mode));
                    if (why == "build") { Toast(T("modsBuild"), warn: true); NavMods.IsChecked = true; return; }
                }
                StartBar.Value = 300;
                PresenceStop();   // (the game's own Rich Presence takes over)
                StartingSub.Text = T("startingWindow");
                game = WindowSmash.StartGame(repo);
                if (game == null) { Toast(T("notBuilt"), warn: true); return; }
                for (int i = 0; i < 450 && !game.HasExited; i++)
                {
                    if (GameWindow(game) != IntPtr.Zero) break;
                    StartBar.Value = Math.Min(950, 300 + i * 2);
                    await Task.Delay(100);
                }
                if (game.HasExited)
                {
                    string log = Path.Combine(buildDir, "play.log");
                    string tail = File.Exists(log) ? Tail(File.ReadAllText(log)) : "";
                    MessageBox.Show(this, F("noStart", tail), "NativeSmash", MessageBoxButton.OK, MessageBoxImage.Warning);
                    return;
                }
                StartBar.Value = 1000;
                await Task.Delay(250);
                var proc = game;
                Hide();
                await Task.Run(() => { try { proc.WaitForExit(); } catch { } });
                Show();
                Activate();
                FadeIn();
                Toast(T("welcomeBack"));
                PresenceStart();
                _ = CommunityRefresh();
                _ = DiscordRefresh();
                RefreshHome();
            }
            catch (Exception ex) { Toast(ex.Message, warn: true); }
            finally
            {
                playing = false;
                RestoreLobby();   // (an invitation's private lobby: the player's own comes back)
                Starting.Visibility = Visibility.Collapsed;
                if (!IsVisible) Show();
            }
        }

        internal static string Tail(string text) => text.Length <= 900 ? text : "…" + text[^900..];

        [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc f, IntPtr l);
        [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
        [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr h);
        [DllImport("user32.dll")] static extern bool SetForegroundWindow(IntPtr h);
        [DllImport("user32.dll")] static extern bool ShowWindow(IntPtr h, int cmd);
        delegate bool EnumProc(IntPtr h, IntPtr l);

        static IntPtr GameWindow(Process p)
        {
            IntPtr found = IntPtr.Zero;
            try
            {
                uint id = (uint)p.Id;
                EnumWindows((h, _) => { GetWindowThreadProcessId(h, out uint pid); if (pid == id && IsWindowVisible(h)) { found = h; return false; } return true; }, IntPtr.Zero);
            }
            catch { }
            return found;
        }

        static void BringToFront(Process p)
        {
            var h = GameWindow(p);
            if (h != IntPtr.Zero) { ShowWindow(h, 9); SetForegroundWindow(h); }
        }

        internal bool GameRunning()
        {
            if (game != null && !game.HasExited) return true;
            try
            {
                string exe = Path.Combine(buildDir, "ssbu.exe");
                foreach (var p in Process.GetProcessesByName("ssbu"))
                    try { if (string.Equals(p.MainModule?.FileName, exe, StringComparison.OrdinalIgnoreCase)) return true; } catch { }
            }
            catch { }
            return false;
        }

        // ---- the lobby's feed: players online, rooms, the GSP ranking ------------------------------------------
        int communityBusy;

        async Task CommunityRefresh()
        {
            if (Interlocked.Exchange(ref communityBusy, 1) == 1) return;
            try
            {
                LobbyBig.Text = LobbyBig.Text == "" ? T("lobbyAsking") : LobbyBig.Text;
                var (status, board) = await CommunityFeed.Fetch(http, CancellationToken.None);
                if (status != null)
                {
                    LobbyDot.Fill = (Brush)FindResource("Good");
                    LobbyBig.Text = status.Online == 1 ? T("players1") : F("playersN", status.Online.ToString("N0"));
                    LobbySmall.Text = status.OpenRooms == 1 ? F("rooms1", status.Playing) : F("rooms", status.Playing, status.OpenRooms);
                }
                else if (board == null)
                {
                    LobbyDot.Fill = (Brush)FindResource("Warn");
                    LobbyBig.Text = T("lobbyDown"); LobbySmall.Text = T("lobbyDownSmall");
                    LobbyBig.FontSize = 15;
                }
                if (status != null) LobbyBig.FontSize = 24;
                if (board != null && CommunityFeed.ParseBoard(board) is { } parsed)
                {
                    string nsa = await Task.Run(() => DiscordAccount.NsaId(repo));
                    string me = nsa == "" ? "" : DiscordAccount.PlayerId(nsa);
                    FillBoard(parsed.Top, parsed.Ranked, me);
                }
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException) { }
            finally { Interlocked.Exchange(ref communityBusy, 0); }
        }

        void FillBoard(List<BoardRow> top, int ranked, string me)
        {
            BoardList.Children.Clear();
            BoardRanked.Text = ranked > 0 ? F("boardRanked", ranked) : "";
            BoardEmpty.Visibility = top.Count == 0 ? Visibility.Visible : Visibility.Collapsed;
            foreach (var r in top.Take(8))
            {
                bool mine = me != "" && r.Id == me;
                var g = new Grid { Margin = new Thickness(0, 3, 0, 3) };
                g.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(28) });
                g.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
                g.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
                var rank = new TextBlock { Text = r.Rank.ToString(), FontWeight = FontWeights.SemiBold, FontSize = 13 };
                rank.Foreground = r.Rank switch
                {
                    1 => new SolidColorBrush(Color.FromRgb(0xFF, 0xD1, 0x66)), 2 => new SolidColorBrush(Color.FromRgb(0xD5, 0xDB, 0xEA)),
                    3 => new SolidColorBrush(Color.FromRgb(0xE0, 0x9A, 0x6A)), _ => (Brush)FindResource("Muted"),
                };
                var who = new TextBlock { Text = CommunityFeed.Who(r) + (mine ? "  " + T("boardYou") : ""), FontSize = 13, TextTrimming = TextTrimming.CharacterEllipsis,
                    Foreground = mine ? new SolidColorBrush(Color.FromRgb(0xB9, 0xC2, 0xFF)) : (Brush)FindResource("Ink") };
                var gsp = new TextBlock { Text = r.Gsp.ToString("N0", CultureInfo.CurrentCulture), FontSize = 13, Foreground = (Brush)FindResource("Muted"), Margin = new Thickness(8, 0, 0, 0) };
                Grid.SetColumn(who, 1); Grid.SetColumn(gsp, 2);
                g.Children.Add(rank); g.Children.Add(who); g.Children.Add(gsp);
                BoardList.Children.Add(g);
            }
        }

        // ---- Rich Presence: "In the launcher", until the game starts -------------------------------------------
        void PresenceStart()
        {
            if (presence != null) return;
            string app = DiscordAccount.AppId(repo);
            if (app == "") return;
            try { presence = DiscordPresence.Start(buildDir, app); }
            catch (Exception e) when (e is DllNotFoundException or BadImageFormatException or SEHException or EntryPointNotFoundException) { presence = null; }
            if (presence == null) return;
            try { presence.Show(Es ? "En el launcher" : "In the launcher", "NativeSmash"); } catch { }
            presenceTimer.Tick -= PresenceTick;
            presenceTimer.Tick += PresenceTick;
            presenceTimer.Start();
        }

        void PresenceTick(object? s, EventArgs e)
        {
            try { presence?.Tick(); } catch (SEHException) { PresenceStop(); }
        }

        void PresenceStop()
        {
            presenceTimer.Stop();
            try { presence?.Dispose(); } catch { }
            presence = null;
        }
    }
}
