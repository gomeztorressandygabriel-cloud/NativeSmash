using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Threading;
using OpenSmash.Launcher;
using Forms = System.Windows.Forms;

namespace NativeSmash
{
    // NativeSmash.exe: the folder's launcher. It lives in the game's folder (beside build\ and tools\), or in
    // NativeSmash.Launcher\bin\... while it is being built, and finds the folder by walking up to tools\install_windowsmash.ps1.
    // No game built yet: OpenSmash's installer window first (the same one OpenSmash.exe opens), then this window.
    public static class App
    {
        [STAThread]
        public static int Main(string[] args)
        {
            using var mutex = new Mutex(true, "Global\\NativeSmashLauncher", out bool first);
            if (!first) { BringOtherToFront(); return 0; }

            string? repo = FindRepo(AppContext.BaseDirectory) ?? FindRepo(Environment.CurrentDirectory);
            if (repo == null)
            {
                Forms.MessageBox.Show("NativeSmash.exe tiene que estar dentro de la carpeta de NativeSmash (donde están build y tools).",
                    "NativeSmash", Forms.MessageBoxButtons.OK, Forms.MessageBoxIcon.Warning);
                return 1;
            }
            Directory.SetCurrentDirectory(repo);
            UiLocale.Initialize(repo);

            string workDir = Path.Combine(Path.GetTempPath(), "NativeSmash_" + Guid.NewGuid().ToString("N")[..8]);
            Directory.CreateDirectory(workDir);
            ExtractResources(workDir);

            bool installer = args.Any(a => a.Equals("--installer", StringComparison.OrdinalIgnoreCase));
            if (!WindowSmash.IsBuilt(repo) || installer)
            {
                if (!RunInstaller(repo, workDir)) return 0;
            }

            // launcher.ini show_launcher = 0: Play straight away (Shift held at the start shows the window).
            bool shift = (Forms.Control.ModifierKeys & Forms.Keys.Shift) != 0;
            if (!shift && !args.Contains("--window") && Settings.StartDirectly(repo) && WindowSmash.IsBuilt(repo))
            {
                Launch.EnsureOnlineFiles(repo);
                using var game = WindowSmash.StartGame(repo);
                return 0;
            }

            var app = new System.Windows.Application { ShutdownMode = System.Windows.ShutdownMode.OnMainWindowClose };
            var page = args.SkipWhile(a => a != "--page").Skip(1).FirstOrDefault() ?? "";
            var shot = args.SkipWhile(a => a != "--shot").Skip(1).FirstOrDefault() ?? "";
            var window = new MainWindow(repo, workDir, page, shot);
            app.Run(window);
            try { Directory.Delete(workDir, true); } catch { }
            return 0;
        }

        // OpenSmash's installer (InstallerForm): set up, repair or rebuild. True when the game is built after it.
        public static bool RunInstaller(string repo, string workDir)
        {
            Forms.Application.EnableVisualStyles();
            Forms.Application.SetCompatibleTextRenderingDefault(false);
            using var form = new InstallerForm(workDir, repo);
            UiLocale.Attach(form);
            if (System.Windows.Application.Current == null) Forms.Application.Run(form);
            else form.ShowDialog();
            return form.Installed && WindowSmash.IsBuilt(repo) || WindowSmash.IsBuilt(repo);
        }

        public static string? FindRepo(string start)
        {
            try
            {
                var dir = new DirectoryInfo(Path.GetFullPath(start));
                for (int i = 0; dir != null && i < 8; i++, dir = dir.Parent)
                    if (File.Exists(Path.Combine(dir.FullName, "tools", "install_windowsmash.ps1"))) return dir.FullName;
            }
            catch { }
            return null;
        }

        static void ExtractResources(string workDir)
        {
            var asm = Assembly.GetExecutingAssembly();
            foreach (string name in asm.GetManifestResourceNames())
            {
                if (name.EndsWith(".html", StringComparison.OrdinalIgnoreCase) || name.EndsWith(".js", StringComparison.OrdinalIgnoreCase)
                    || name.StartsWith("Locales.", StringComparison.Ordinal) || name.EndsWith(".g.resources", StringComparison.OrdinalIgnoreCase)) continue;
                int lastDot = name.LastIndexOf('.');
                int prevDot = lastDot > 0 ? name.LastIndexOf('.', lastDot - 1) : -1;
                string fileName = prevDot >= 0 ? name[(prevDot + 1)..] : name;
                try
                {
                    using var stream = asm.GetManifestResourceStream(name);
                    if (stream == null) continue;
                    using var file = File.Create(Path.Combine(workDir, fileName));
                    stream.CopyTo(file);
                }
                catch { }
            }
        }

        [System.Runtime.InteropServices.DllImport("user32.dll")] static extern bool SetForegroundWindow(IntPtr h);
        [System.Runtime.InteropServices.DllImport("user32.dll")] static extern bool ShowWindow(IntPtr h, int cmd);

        static void BringOtherToFront()
        {
            try
            {
                var me = Process.GetCurrentProcess();
                foreach (var p in Process.GetProcessesByName(me.ProcessName))
                    if (p.Id != me.Id && p.MainWindowHandle != IntPtr.Zero) { ShowWindow(p.MainWindowHandle, 9); SetForegroundWindow(p.MainWindowHandle); }
            }
            catch { }
        }
    }
}
