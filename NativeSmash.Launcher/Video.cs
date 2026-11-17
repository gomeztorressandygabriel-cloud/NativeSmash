using System.Windows;
using OpenSmash.Launcher;
using static NativeSmash.Words;
using Brush = System.Windows.Media.Brush;
using Orientation = System.Windows.Controls.Orientation;
using HorizontalAlignment = System.Windows.HorizontalAlignment;
using VerticalAlignment = System.Windows.VerticalAlignment;
using FontStyle = System.Windows.FontStyle;
using Image = System.Windows.Controls.Image;

namespace NativeSmash
{
    // Video and sound: display.ini as the game's settings menu (Esc) writes it; the values and their order are
    // SettingDefaults.Choices, the table OpenSmash's tests check against the runtime.
    public partial class MainWindow
    {
        void InitVideo()
        {
            VideoTitle.Text = T("videoTitle"); VideoHelp.Text = T("videoHelp"); WindowLabel.Text = T("window"); SizeLabel.Text = T("size");
            ScaleLabel.Text = T("scale"); ScaleHelp.Text = T("scaleHelp"); PacingLabel.Text = T("pacing"); PacingHelp.Text = T("pacingHelp");
            RendererLabel.Text = T("renderer"); RendererHelp.Text = T("rendererHelp"); GpuLabel.Text = T("gpu");
            OverlayBox.Content = T("overlay"); CursorBox.Content = T("cursor"); VolumeLabel.Text = T("volume"); LangLabel.Text = T("lang");

            string D(string k) => Settings.Display(repo, k);
            Chips(WindowChips, SettingDefaults.Choices["window"].Select(v => (v, T("win_" + v))), D("window"), v => Save(() => Settings.SetDisplay(repo, "window", v)));

            string size = D("width") + "x" + D("height");
            var sizes = SettingDefaults.Choices["window_size"].ToList();
            if (!sizes.Contains(size)) sizes.Insert(0, size);
            Chips(SizeChips, sizes.Select(v => (v, v.Replace("x", " × "))), size, v =>
            {
                var p = v.Split('x');
                Save(() => { Settings.SetDisplay(repo, "width", p[0]); Settings.SetDisplay(repo, "height", p[1]); });
            });

            var scales = SettingDefaults.Choices["render_scale"].ToList();
            string scale = D("render_scale");
            if (!scales.Contains(scale)) scales.Add(scale);
            Chips(ScaleChips, scales.Select(v => (v, All("scale_" + v, v))), scale, v => Save(() => Settings.SetDisplay(repo, "render_scale", v)));

            Chips(PacingChips, SettingDefaults.Choices["pacing"].Select(v => (v, T("pac_" + v))), D("pacing"), v => Save(() => Settings.SetDisplay(repo, "pacing", v)));
            Chips(RendererChips, SettingDefaults.Choices["renderer"].Select(v => (v, T("ren_" + v))), SettingDefaults.Renderer(D("renderer")),
                v => { Save(() => Settings.SetDisplay(repo, "renderer", v)); RefreshShaders(); });

            var gpus = new List<string> { "auto", "warp" };
            string gpu = D("gpu");
            if (!gpus.Contains(gpu)) gpus.Add(gpu);
            Chips(GpuChips, gpus.Select(v => (v, All("gpu_" + v, v))), gpu, v => Save(() => Settings.SetDisplay(repo, "gpu", v)));

            OverlayBox.IsChecked = D("overlay") != "0";
            OverlayBox.Click += (_, _) => Save(() => Settings.SetDisplay(repo, "overlay", OverlayBox.IsChecked == true ? "1" : "0"));
            CursorBox.IsChecked = D("hide_cursor") != "0";
            CursorBox.Click += (_, _) => Save(() => Settings.SetDisplay(repo, "hide_cursor", CursorBox.IsChecked == true ? "1" : "0"));

            VolumeSlider.Value = int.TryParse(D("volume"), out int vol) ? Math.Clamp(vol, 0, 100) : 100;
            VolumeValue.Text = (int)VolumeSlider.Value + " %";
            VolumeSlider.ValueChanged += (_, _) => VolumeValue.Text = (int)VolumeSlider.Value + " %";
            VolumeSlider.PreviewMouseLeftButtonUp += (_, _) => Save(() => Settings.SetDisplay(repo, "volume", ((int)VolumeSlider.Value).ToString()));
            VolumeSlider.LostKeyboardFocus += (_, _) => Save(() => Settings.SetDisplay(repo, "volume", ((int)VolumeSlider.Value).ToString()));

            var codes = UiLocale.Codes; var names = UiLocale.Names;
            Chips(LangChips, codes.Select((c, i) => (c, c == "auto" ? (Es ? "El del sistema" : "System language") : names[i])), D("ui_language"),
                v => Save(() => Settings.SetDisplay(repo, "ui_language", v)));
        }

        static string All(string key, string fallback) { string t = T(key); return t == key ? fallback : t; }
    }
}
