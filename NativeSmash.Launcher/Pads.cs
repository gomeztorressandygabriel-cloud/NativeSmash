using System.IO;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
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
using KeyEventArgs = System.Windows.Input.KeyEventArgs;
using RadioButton = System.Windows.Controls.RadioButton;

namespace NativeSmash
{
    // Controllers: the keyboard (controllers.ini [general] keyboard_player and background, [keyboard] keys and movement)
    // and the pads, read through the game's own SDL3.dll as OpenSmash's remap page does (SdlPadSource), each remap
    // written to [device <GUID>] with PadProfile (the section the game picks for that model of pad on any port).
    public partial class MainWindow
    {
        string PadsIni => Settings.File(repo, "controllers.ini");

        // the runtime's [keyboard] defaults (runtime/hle_pad.c)
        static readonly (string Action, string Key)[] KeyDefaults =
        {
            ("A", "Z"), ("B", "X"), ("X", "C"), ("Y", "V"), ("L", "A"), ("R", "S"), ("ZL", "Q"), ("ZR", "W"), ("PLUS", "Enter"), ("MINUS", "Backspace"),
            ("LSTICK_LEFT", "Left"), ("LSTICK_RIGHT", "Right"), ("LSTICK_UP", "Up"), ("LSTICK_DOWN", "Down"),
            ("RSTICK_LEFT", "J"), ("RSTICK_RIGHT", "L"), ("RSTICK_UP", "I"), ("RSTICK_DOWN", "K"),
            ("UP", "none"), ("DOWN", "none"), ("LEFT", "none"), ("RIGHT", "none"), ("LSTICK", "none"), ("RSTICK", "none"),
        };

        static string ActionLabel(string a) => a switch
        {
            "PLUS" => Es ? "+ (pausa)" : "+ (pause)", "MINUS" => "−",
            "LSTICK_LEFT" => Es ? "Stick izq. ←" : "Left stick ←", "LSTICK_RIGHT" => Es ? "Stick izq. →" : "Left stick →",
            "LSTICK_UP" => Es ? "Stick izq. ↑" : "Left stick ↑", "LSTICK_DOWN" => Es ? "Stick izq. ↓" : "Left stick ↓",
            "RSTICK_LEFT" => Es ? "Stick der. ←" : "Right stick ←", "RSTICK_RIGHT" => Es ? "Stick der. →" : "Right stick →",
            "RSTICK_UP" => Es ? "Stick der. ↑" : "Right stick ↑", "RSTICK_DOWN" => Es ? "Stick der. ↓" : "Right stick ↓",
            "UP" => Es ? "Cruceta ↑" : "D-pad ↑", "DOWN" => Es ? "Cruceta ↓" : "D-pad ↓", "LEFT" => Es ? "Cruceta ←" : "D-pad ←", "RIGHT" => Es ? "Cruceta →" : "D-pad →",
            "LSTICK" => Es ? "Clic stick izq." : "Left stick click", "RSTICK" => Es ? "Clic stick der." : "Right stick click",
            _ => a,
        };

        static string KeyShown(string k) => k.ToLowerInvariant() switch
        {
            "" or "none" => "—", "left" => "←", "right" => "→", "up" => "↑", "down" => "↓", "space" => Es ? "Espacio" : "Space",
            "backspace" => Es ? "Retroceso" : "Backspace", "enter" => "Enter", "shift" => "Shift", "ctrl" => "Ctrl", "alt" => "Alt", "tab" => "Tab",
            _ => k,
        };

        string padTab = "keys";

        void InitPads()
        {
            PadsTitle.Text = T("padsTitle"); KbLabel.Text = T("kbPlays"); BackgroundBox.Content = T("background");
            MoveLabel.Text = T("moveLabel"); MoveHelp.Text = T("moveHelp"); KeysTitle.Text = T("keysTitle"); KeysHelp.Text = T("keysHelp");
            ResetKeys.Content = T("resetKeys"); OpenPads.Content = T("openPads"); OpenPads2.Content = T("openPads");
            PadPickLabel.Text = T("padPick"); PadNone.Text = T("padNone"); DpadBox.Content = T("dpad"); TauntBox.Content = T("taunts");
            PadMapTitle.Text = T("padMapTitle"); PadMapHelp.Text = T("padMapHelp"); ResetPad.Content = T("resetPad"); PadsHelp.Text = T("padsHelp");

            Chips(PadTabs, new[] { ("keys", T("tabKeys")), ("pads", T("tabPads")) }, "keys", ShowPadTab);
            Chips(KbChips, Enumerable.Range(1, 8).Select(i => (i.ToString(), F("player", i))).Append(("0", T("nobody"))),
                Settings.Pads(repo, "keyboard_player"), v => Save(() => Settings.SetPads(repo, "keyboard_player", v)));
            BackgroundBox.IsChecked = Settings.Pads(repo, "background") != "0";
            BackgroundBox.Click += (_, _) => Save(() => Settings.SetPads(repo, "background", BackgroundBox.IsChecked == true ? "1" : "0"));
            string move = Settings.Ini(PadsIni, "keyboard", "movement") ?? "full";
            Chips(MoveChips, new[] { ("full", T("move_full")), ("ssf2", T("move_ssf2")) }, move.Equals("ssf2", StringComparison.OrdinalIgnoreCase) ? "ssf2" : "full",
                v => Save(() => Settings.SetIni(PadsIni, "keyboard", "movement", v)));
            ResetKeys.Click += (_, _) => Save(() => { foreach (var (a, k) in KeyDefaults) Settings.SetIni(PadsIni, "keyboard", a, k); KeysNote.Text = ""; BuildKeyGrid(); });
            OpenPads.Click += (_, _) => OpenIni();
            OpenPads2.Click += (_, _) => OpenIni();
            DpadBox.Click += (_, _) => PadWrite("dpad_moves", DpadBox.IsChecked == true ? "on" : null);
            TauntBox.Click += (_, _) => PadWrite("stick_taunts", TauntBox.IsChecked == true ? "on" : null);
            ResetPad.Click += (_, _) =>
            {
                if (pad == null) return;
                string text = IniText();
                foreach (string k in PadProfile.Buttons.Concat(PadProfile.Switches)) text = PadProfile.Set(text, PadProfile.SectionFor(pad.Guid), k, null);
                try { File.WriteAllText(PadsIni, text, new UTF8Encoding(false)); } catch (Exception ex) { Toast(ex.Message, true); }
                ShowPad();
            };
            BuildKeyGrid();
        }

        void OpenIni()
        {
            try
            {
                if (!File.Exists(PadsIni)) Settings.SetPads(repo, "keyboard_player", Settings.Pads(repo, "keyboard_player"));
                System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo("notepad.exe", "\"" + PadsIni + "\"") { UseShellExecute = true });
            }
            catch { }
        }

        void ShowPadTab(string tab)
        {
            padTab = tab;
            Pick(PadTabs, tab);
            KbSection.Visibility = tab == "keys" ? Visibility.Visible : Visibility.Collapsed;
            PadSection.Visibility = tab == "pads" ? Visibility.Visible : Visibility.Collapsed;
            if (tab == "pads") StartPadSource(); else StopPadSource();
        }

        void PadsShown() { if (padTab == "pads") StartPadSource(); }
        void PadsHidden() { StopPadSource(); capturingKey = null; capturingPad = null; }

        // ---- the keyboard's keys --------------------------------------------------------------------------------
        string? capturingKey;
        readonly Dictionary<string, Button> keyButtons = new();

        string KeyOf(string action) => Settings.Ini(PadsIni, "keyboard", action) ?? KeyDefaults.First(d => d.Action == action).Key;

        void BuildKeyGrid()
        {
            KeyGrid.Children.Clear(); keyButtons.Clear();
            foreach (var (action, _) in KeyDefaults)
            {
                var b = KeyCell(ActionLabel(action), KeyShown(KeyOf(action)), false, 170);
                b.Click += (_, _) => { capturingKey = action; KeysNote.Text = ""; RefreshKeyCells(); };
                keyButtons[action] = b;
                KeyGrid.Children.Add(b);
            }
        }

        Button KeyCell(string label, string value, bool lit, double width = 300)
        {
            var g = new Grid { Width = width };
            g.Children.Add(new TextBlock { Text = label, Foreground = (Brush)FindResource("Muted"), FontSize = 13, VerticalAlignment = VerticalAlignment.Center });
            g.Children.Add(new TextBlock { Text = value, Foreground = (Brush)FindResource("Ink"), FontSize = 14, FontWeight = FontWeights.SemiBold, HorizontalAlignment = HorizontalAlignment.Right, VerticalAlignment = VerticalAlignment.Center });
            var b = new Button { Style = (Style)FindResource("Soft"), Content = g, HorizontalContentAlignment = HorizontalAlignment.Stretch };
            if (lit) b.BorderBrush = (Brush)FindResource("Accent");
            return b;
        }

        void RefreshKeyCells()
        {
            foreach (var (action, b) in keyButtons)
            {
                var g = (Grid)b.Content;
                ((TextBlock)g.Children[1]).Text = capturingKey == action ? T("pressKey") : KeyShown(KeyOf(action));
                ((TextBlock)g.Children[1]).Foreground = capturingKey == action ? new SolidColorBrush(System.Windows.Media.Color.FromRgb(0xB9, 0xC2, 0xFF)) : (Brush)FindResource("Ink");
            }
        }

        static string? KeyName(KeyEventArgs e)
        {
            var k = e.Key == Key.System ? e.SystemKey : e.Key;
            if (k >= Key.A && k <= Key.Z) return k.ToString();
            if (k >= Key.D0 && k <= Key.D9) return ((int)(k - Key.D0)).ToString();
            if (k >= Key.NumPad0 && k <= Key.NumPad9) return "Num" + (int)(k - Key.NumPad0);
            if (k >= Key.F9 && k <= Key.F12) return k.ToString();
            return k switch
            {
                Key.Left => "Left", Key.Right => "Right", Key.Up => "Up", Key.Down => "Down", Key.Enter => "Enter", Key.Back => "Backspace",
                Key.Space => "Space", Key.Tab => "Tab", Key.LeftShift or Key.RightShift => "Shift", Key.LeftCtrl or Key.RightCtrl => "Ctrl",
                Key.LeftAlt or Key.RightAlt => "Alt",
                _ => KeyInterop.VirtualKeyFromKey(k) is int vk and > 0 ? "0x" + vk.ToString("X2") : null,
            };
        }

        // A key press while waiting for one (the keyboard's keys) or Esc while waiting for a pad's button.
        bool KeyCapture(KeyEventArgs e)
        {
            if (capturingPad != null && e.Key == Key.Escape) { capturingPad = null; ShowPad(); e.Handled = true; return true; }
            if (capturingKey == null) return false;
            e.Handled = true;
            var k = e.Key == Key.System ? e.SystemKey : e.Key;
            if (k == Key.Escape) { capturingKey = null; RefreshKeyCells(); return true; }
            if (k >= Key.F1 && k <= Key.F8) { KeysNote.Text = T("keyReserved"); return true; }
            string? name = KeyName(e);
            if (name == null) return true;
            string action = capturingKey;
            capturingKey = null;
            Save(() =>
            {
                foreach (var (other, _) in KeyDefaults)
                    if (other != action && KeyOf(other).Equals(name, StringComparison.OrdinalIgnoreCase))
                    {
                        Settings.SetIni(PadsIni, "keyboard", other, "none");
                        KeysNote.Text = F("keySwapped", KeyShown(name), ActionLabel(other));
                    }
                Settings.SetIni(PadsIni, "keyboard", action, name);
            });
            RefreshKeyCells();
            return true;
        }

        // ---- pads -----------------------------------------------------------------------------------------------
        IPadSource? padSource;
        IReadOnlyList<PadDevice> padList = Array.Empty<PadDevice>();
        PadDevice? pad;
        string? capturingPad;
        ISet<string> padBefore = new HashSet<string>();
        DispatcherTimer? padTimer;
        DateTime nextScan;
        readonly Dictionary<string, Button> padButtons = new();

        void StartPadSource()
        {
            if (padSource != null) return;
            try { padSource = new SdlPadSource(buildDir, repo); }
            catch (Exception e) { padSource = null; PadNone.Text = F("padNoSdl", e.Message); PadNone.Visibility = Visibility.Visible; return; }
            if (padSource.Problem != "") { PadNone.Text = padSource.Problem.Contains("SDL3.dll") ? T("padNoSdlDll") : F("padNoSdl", padSource.Problem); }
            padTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(16) };
            padTimer.Tick += (_, _) => PadPoll();
            padTimer.Start();
            nextScan = DateTime.MinValue;
        }

        void StopPadSource()
        {
            padTimer?.Stop(); padTimer = null;
            try { padSource?.Dispose(); } catch { }
            padSource = null;
        }

        void PadPoll()
        {
            if (padSource == null) return;
            if (DateTime.UtcNow >= nextScan)
            {
                nextScan = DateTime.UtcNow.AddSeconds(1);
                var now = padSource.Pads();
                if (!now.Select(p => p.Id).SequenceEqual(padList.Select(p => p.Id)))
                {
                    padList = now;
                    if (pad == null || !padList.Any(p => p.Id == pad.Id)) pad = padList.FirstOrDefault();
                    Chips(PadPickChips, padList.Select(p => (p.Id.ToString(), p.Name)), pad?.Id.ToString() ?? "",
                        v => { pad = padList.FirstOrDefault(p => p.Id.ToString() == v); capturingPad = null; ShowPad(); });
                    ShowPad();
                }
            }
            if (pad == null) return;
            var held = padSource.Pressed(pad.Id);
            if (capturingPad != null)
            {
                string? pressed = PadProfile.NewlyPressed(padBefore, held);
                padBefore = held;
                if (pressed != null) { string b = capturingPad; capturingPad = null; PadWrite(b, pressed); return; }
            }
            // what is held lights up
            string ini = IniText();
            foreach (var (b, btn) in padButtons)
            {
                var (value, _) = PadProfile.Effective(ini, pad.Guid, pad.Name, pad.Type, b);
                bool lit = capturingPad == null && value.Split(',').Any(v => held.Contains(v.Trim()));
                btn.BorderBrush = lit ? (Brush)FindResource("Accent") : (Brush)FindResource("Edge");
                btn.Background = lit ? new SolidColorBrush(System.Windows.Media.Color.FromArgb(0x44, 0x6D, 0x7D, 0xFF)) : (Brush)FindResource("Veil");
            }
        }

        string IniText() { try { return File.Exists(PadsIni) ? File.ReadAllText(PadsIni) : ""; } catch (IOException) { return ""; } }

        void ShowPad()
        {
            PadNone.Visibility = pad == null ? Visibility.Visible : Visibility.Collapsed;
            PadBody.Visibility = pad == null ? Visibility.Collapsed : Visibility.Visible;
            PadMapGrid.Children.Clear(); padButtons.Clear();
            if (pad == null) return;
            PadWhere.Text = F("padWhere", pad.Name + " (" + pad.Type + ")", pad.Guid);
            string ini = IniText();
            DpadBox.IsChecked = PadProfile.Effective(ini, pad.Guid, pad.Name, pad.Type, "dpad_moves").Value.Equals("on", StringComparison.OrdinalIgnoreCase);
            TauntBox.IsChecked = PadProfile.Effective(ini, pad.Guid, pad.Name, pad.Type, "stick_taunts").Value.Equals("on", StringComparison.OrdinalIgnoreCase);
            foreach (string b in PadProfile.Buttons)
            {
                var (value, from) = PadProfile.Effective(ini, pad.Guid, pad.Name, pad.Type, b);
                string shown = capturingPad == b ? T("pressPad")
                    : string.Join(" + ", value.Split(',', StringSplitOptions.RemoveEmptyEntries).Select(v => PadProfile.SourceLabel(v.Trim()))) + (from == "" ? "  ·  " + T("padDefault") : "");
                var cell = KeyCell(PadProfile.ButtonLabel(b), shown, false);
                string button = b;
                cell.Click += (_, _) => { if (padSource == null || pad == null) return; capturingPad = button; padBefore = padSource.Pressed(pad.Id); ShowPad(); };
                cell.MouseRightButtonUp += (_, _) => PadWrite(button, null);
                padButtons[b] = cell;
                PadMapGrid.Children.Add(cell);
            }
        }

        void PadWrite(string key, string? value)
        {
            if (pad == null) return;
            try
            {
                string text = PadProfile.Set(IniText(), PadProfile.SectionFor(pad.Guid), key, value, pad.Name + " (" + pad.Type + ")");
                Directory.CreateDirectory(Path.GetDirectoryName(PadsIni)!);
                File.WriteAllText(PadsIni, text, new UTF8Encoding(false));
                PadMapNote.Text = "";
            }
            catch (Exception ex) { PadMapNote.Text = ex.Message; }
            ShowPad();
        }
    }
}
