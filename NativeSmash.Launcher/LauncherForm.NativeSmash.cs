namespace OpenSmash.Launcher
{
    // NativeSmash opens OpenSmash's own window for what only it does (the mod manager and its GameBanana browser):
    // this opens that window on a given view.
    public partial class LauncherForm
    {
        public void NativeSmashOpen(string view) => GoView(view);
    }
}
