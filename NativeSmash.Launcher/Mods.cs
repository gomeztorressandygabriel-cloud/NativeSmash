using System.IO;
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
using CheckBox = System.Windows.Controls.CheckBox;

namespace NativeSmash
{
    // Mods: what tools\os_mods.py says is in mods\ (each one switched on or off here), and OpenSmash's own mod manager
    // (its window, on its Mods view) for finding, installing and updating them.
    public partial class MainWindow
    {
        Launch.Mods? modsCached;
        int modsBusy;

        void InitMods()
        {
            ModsTitle.Text = T("modsTitle"); ModsHelp.Text = T("modsHelp"); ModsOpen.Content = T("modsOpen"); ModsFolder.Content = T("modsFolder");
            ModsOnline.Text = T("modsOnline"); ModsNumber.Text = "…"; ModsCaption.Text = T("modsReading");
            ModsOpen.Click += (_, _) => OpenModManager();
            ModsFolder.Click += (_, _) => { string d = Path.Combine(repo, "mods"); Directory.CreateDirectory(d); Launch.Open(d); };
        }

        async Task RefreshModsAsync()
        {
            if (Interlocked.Exchange(ref modsBusy, 1) == 1) return;
            try
            {
                var st = await Task.Run(() => Launch.ModStatus(repo));
                modsCached = st;
                ShowMods();
                if (PageHome.Visibility == Visibility.Visible) RefreshHome();
            }
            finally { Interlocked.Exchange(ref modsBusy, 0); }
        }

        void ShowMods()
        {
            ModsList.Children.Clear();
            if (modsCached is not { } m || m.Total == 0)
            {
                ModsNumber.Text = T("modsNone"); ModsCaption.Text = T("modsCountNone");
                return;
            }
            ModsNumber.Text = m.Enabled.ToString();
            ModsCaption.Text = F("modsCount", m.Total, m.Enabled);
            foreach (var (name, title, on) in m.List.OrderByDescending(x => x.On).ThenBy(x => x.Title, StringComparer.CurrentCultureIgnoreCase))
            {
                var box = new CheckBox { Style = (Style)FindResource("Switch"), Content = title, IsChecked = on, Margin = new Thickness(0, 6, 0, 6) };
                string folder = name;
                box.Click += async (_, _) =>
                {
                    bool want = box.IsChecked == true;
                    box.IsEnabled = false;
                    var r = await Task.Run(() => Launch.RunPython(repo, "os_mods.py", new[] { "switch", folder, want ? "on" : "off" }));
                    box.IsEnabled = true;
                    if (r.Code != 0) { box.IsChecked = !want; Toast(Tail(r.Output), warn: true); }
                    _ = RefreshModsAsync();
                };
                ModsList.Children.Add(box);
            }
        }

        LauncherForm? modWindow;

        void OpenModManager()
        {
            if (modWindow != null && !modWindow.IsDisposed) { modWindow.Activate(); return; }
            try
            {
                PresenceStop();
                modWindow = new LauncherForm(workDir, repo, buildDir);
                UiLocale.Attach(modWindow);
                modWindow.Shown += (_, _) => modWindow.NativeSmashOpen("mods");
                modWindow.FormClosed += (_, _) => { modWindow = null; PresenceStart(); _ = RefreshModsAsync(); };
                modWindow.Show();
            }
            catch (Exception ex) { Toast(ex.Message, warn: true); PresenceStart(); }
        }
    }
}
