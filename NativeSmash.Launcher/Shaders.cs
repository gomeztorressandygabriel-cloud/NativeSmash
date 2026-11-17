using System.Diagnostics;
using System.IO;
using System.Text.RegularExpressions;
using System.Windows;
using System.Windows.Media;
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
    // Shaders: tools\shader_aot.py prepares every GPU program ahead (Direct3D 11's and Vulkan's), at low priority, with
    // its progress here; dump\shadercache\complete.txt says it is done for this cache version (OpenSmash's ShadersDone).
    public partial class MainWindow
    {
        Process? shaderProc;
        bool shaderStopped;

        void InitShaders()
        {
            ShadersTitle.Text = T("shadersTitle"); ShadersHelp.Text = T("shadersHelp"); StopShaders.Content = T("stop"); ShadersNote.Text = T("shNote");
            PrepareBtn.Click += (_, _) => StartShaders();
            StopShaders.Click += (_, _) => { shaderStopped = true; KillTree(shaderProc); };
            RefreshShaders();
        }

        string RendererAsked() => SettingDefaults.Renderer(Settings.Display(repo, "renderer"));

        bool ShadersDone()
        {
            try
            {
                string marker = Path.Combine(repo, "dump", "shadercache", "complete.txt");
                string headerFile = Path.Combine(repo, "runtime", "gpu_shader_cache.h");
                if (!File.Exists(marker) || !File.Exists(headerFile)) return false;
                string header = File.ReadAllText(headerFile);
                var ver = Regex.Match(header, @"#define SHADER_CACHE_VERSION (\d+)");
                if (!ver.Success) return false;
                string text = File.ReadAllText(marker);
                if (!text.Contains("version " + ver.Groups[1].Value)) return false;
                var spv = Regex.Match(header, @"#define SHADER_CACHE_SPV_VERSION \(SHADER_CACHE_VERSION \* 100 \+ (\d+)\)");
                string want = spv.Success ? "\nspirv g" + (int.Parse(ver.Groups[1].Value) * 100 + int.Parse(spv.Groups[1].Value)) + "." : "\nspirv ";
                return RendererAsked() == "direct3d11" || !File.Exists(Path.Combine(buildDir, "ssbu_glslang.dll")) || text.Contains(want);
            }
            catch { return false; }
        }

        void RefreshShaders()
        {
            bool running = shaderProc != null && !shaderProc.HasExited;
            ShaderRun.Visibility = running ? Visibility.Visible : Visibility.Collapsed;
            StopShaders.Visibility = running ? Visibility.Visible : Visibility.Collapsed;
            PrepareBtn.IsEnabled = !running;
            if (running) { ShaderState.Text = T("shRunning"); ShaderDot.Fill = (Brush)FindResource("Accent"); return; }
            bool done = ShadersDone();
            string what = RendererAsked() == "direct3d11" ? "Direct3D 11" : "Vulkan";
            ShaderState.Text = done ? T("shReady") : T("shMissing");
            ShaderCaption.Text = done ? F("shReadyCap", what) : T("shMissingCap");
            ShaderDot.Fill = (Brush)FindResource(done ? "Good" : "Warn");
            PrepareBtn.Content = done ? T("prepareAgain") : T("prepare");
        }

        void StartShaders()
        {
            if (shaderProc != null && !shaderProc.HasExited) return;
            string py = Launch.Python(repo);
            if (string.IsNullOrEmpty(py)) { Toast("Python?", warn: true); return; }
            var psi = new ProcessStartInfo
            {
                FileName = py, WorkingDirectory = repo, UseShellExecute = false, CreateNoWindow = true,
                RedirectStandardOutput = true, RedirectStandardError = true,
                StandardOutputEncoding = System.Text.Encoding.UTF8, StandardErrorEncoding = System.Text.Encoding.UTF8,
            };
            psi.EnvironmentVariables["PYTHONIOENCODING"] = "utf-8";
            psi.EnvironmentVariables["PYTHONUNBUFFERED"] = "1";
            psi.ArgumentList.Add(Path.Combine(repo, "tools", "shader_aot.py"));
            psi.ArgumentList.Add("--priority");
            psi.ArgumentList.Add("low");
            var proc = new Process { StartInfo = psi, EnableRaisingEvents = true };
            proc.OutputDataReceived += (_, e) => { if (e.Data != null) Dispatcher.BeginInvoke(() => ShaderLine(e.Data)); };
            proc.ErrorDataReceived += (_, e) => { if (e.Data != null) Dispatcher.BeginInvoke(() => ShaderLine(e.Data)); };
            proc.Exited += (_, _) => Dispatcher.BeginInvoke(() =>
            {
                int code = -1; try { code = proc.ExitCode; } catch { }
                shaderProc = null;
                Toast(shaderStopped ? T("shStopped") : code == 0 ? T("shDone") : T("shStopped"), warn: code != 0 && !shaderStopped);
                shaderStopped = false;
                RefreshShaders();
                if (PageHome.Visibility == Visibility.Visible) RefreshHome();
            });
            try
            {
                proc.Start(); proc.BeginOutputReadLine(); proc.BeginErrorReadLine();
                shaderProc = proc;
                ShaderBar.Value = 0; ShaderDetail.Text = ""; ShaderEta.Text = "";
            }
            catch (Exception ex) { Toast(ex.Message, warn: true); }
            RefreshShaders();
        }

        void ShaderLine(string line)
        {
            var files = Regex.Match(line, @"(listing|reading) shader files:\s*([\d,]+)\s+of\s+([\d,]+)", RegexOptions.IgnoreCase);
            if (files.Success && double.TryParse(files.Groups[2].Value.Replace(",", ""), out double read) && double.TryParse(files.Groups[3].Value.Replace(",", ""), out double total) && total > 0)
            {
                ShaderBar.Value = 1000 * read / total;
                ShaderDetail.Text = F("shListing", read.ToString("N0"), total.ToString("N0"));
                ShaderEta.Text = "";
                return;
            }
            var count = Regex.Match(line, @"([\d,]+)\s+of\s+([\d,]+)\s+shaders(?:,\s*about\s+(.+?)\s+left)?", RegexOptions.IgnoreCase);
            if (count.Success && double.TryParse(count.Groups[1].Value.Replace(",", ""), out double done) && double.TryParse(count.Groups[2].Value.Replace(",", ""), out double all) && all > 0)
            {
                ShaderBar.Value = 1000 * done / all;
                ShaderDetail.Text = F("shCompiling", done.ToString("N0"), all.ToString("N0")) + $"   ({(int)(100 * done / all)}%)";
                ShaderEta.Text = count.Groups[3].Success ? F("shLeft", count.Groups[3].Value.Replace(" min", " min").Replace(" h ", " h ")) : "";
            }
        }

        internal static void KillTree(Process? p)
        {
            try { if (p != null && !p.HasExited) p.Kill(entireProcessTree: true); } catch { }
        }
    }
}
