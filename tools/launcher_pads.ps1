# The launcher's controller settings (the Controllers page's "Controllers" tab): the pads connected now, and for the
# one picked, what controllers.ini says about it, with each console button set by pressing a button on the pad.
# Dot-sourced by tools\launcher_wpf.ps1 once its window exists; uses its T, Find, New-Chips, Get-Chip, Get-IniValue.
#
# The pads are read the way the game reads them, so that names and sections match what it writes in its log
# ("[input] player 1: "XInput pad 1" (xinput, GUID xinput0)"): through SDL3 when the game's build\SDL3.dll is there,
# else through XInput (players 1-4). A pad's settings go under the [device ...] that already names it (its GUID,
# then its name, then its type), or else under [device <its name>], as the game's own settings do.

Add-Type -Language CSharp -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public class NsPad {
    public string Name, Type, Guid;
    public int XIndex = -1;
    public uint SdlId;
    public IntPtr Handle;
}

public static class NsPads {
    [StructLayout(LayoutKind.Sequential)] struct XGamepad { public ushort Buttons; public byte LT, RT; public short LX, LY, RX, RY; }
    [StructLayout(LayoutKind.Sequential)] struct XState { public uint Packet; public XGamepad Pad; }
    [DllImport("xinput1_4.dll", EntryPoint = "XInputGetState")] static extern int XInputGetState(int index, out XState state);
    [DllImport("kernel32", CharSet = CharSet.Unicode, SetLastError = true)] static extern IntPtr LoadLibraryW(string path);
    [DllImport("kernel32", CharSet = CharSet.Ansi)] static extern IntPtr GetProcAddress(IntPtr module, string name);

    [StructLayout(LayoutKind.Sequential)] public struct SdlGuid { public ulong A, B; }
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] [return: MarshalAs(UnmanagedType.I1)] delegate bool InitFn(uint flags);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] [return: MarshalAs(UnmanagedType.I1)] delegate bool HintFn(string name, string value);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate IntPtr GetPadsFn(out int count);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate void FreeFn(IntPtr p);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate IntPtr OpenFn(uint id);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate IntPtr NameFn(IntPtr pad);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int TypeFn(IntPtr pad);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate IntPtr TypeNameFn(int type);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate SdlGuid GuidFn(uint id);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate void GuidStrFn(SdlGuid guid, StringBuilder buf, int len);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate void VoidFn();
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] [return: MarshalAs(UnmanagedType.I1)] delegate bool ButtonFn(IntPtr pad, int button);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate short AxisFn(IntPtr pad, int axis);

    static InitFn sdlInit; static HintFn sdlHint; static GetPadsFn sdlGetPads; static FreeFn sdlFree; static OpenFn sdlOpen;
    static NameFn sdlName; static TypeFn sdlType; static TypeNameFn sdlTypeName; static GuidFn sdlGuid; static GuidStrFn sdlGuidStr;
    static VoidFn sdlPump, sdlUpdate; static ButtonFn sdlButton; static AxisFn sdlAxis;
    static bool sdl;
    static readonly Dictionary<uint, NsPad> open = new Dictionary<uint, NsPad>();

    // the pad's own controls, as controllers.ini names them (SDL3's button numbers)
    public static readonly string[] ButtonNames = { "south", "east", "west", "north", "back", "guide", "start", "leftstick", "rightstick",
        "leftshoulder", "rightshoulder", "dpup", "dpdown", "dpleft", "dpright", "misc1", "rightpaddle1", "leftpaddle1", "rightpaddle2",
        "leftpaddle2", "touchpad", "misc2", "misc3", "misc4", "misc5", "misc6" };
    static readonly ushort[] XBits = { 0x1000, 0x2000, 0x4000, 0x8000, 0x0020, 0, 0x0010, 0x0040, 0x0080, 0x0100, 0x0200, 0x0001, 0x0002, 0x0004, 0x0008 };

    public static bool UsingSdl { get { return sdl; } }

    static T Fn<T>(IntPtr m, string name) where T : class {
        IntPtr p = GetProcAddress(m, name);
        if (p == IntPtr.Zero) throw new Exception(name + " is missing");
        return Marshal.GetDelegateForFunctionPointer(p, typeof(T)) as T;
    }

    // SDL3 from the game's own SDL3.dll; null when it works, else why not (XInput is used then)
    public static string StartSdl(string path) {
        try {
            IntPtr m = LoadLibraryW(path);
            if (m == IntPtr.Zero) return "cannot load " + path;
            sdlInit = Fn<InitFn>(m, "SDL_Init"); sdlHint = Fn<HintFn>(m, "SDL_SetHint"); sdlGetPads = Fn<GetPadsFn>(m, "SDL_GetGamepads");
            sdlFree = Fn<FreeFn>(m, "SDL_free"); sdlOpen = Fn<OpenFn>(m, "SDL_OpenGamepad"); sdlName = Fn<NameFn>(m, "SDL_GetGamepadName");
            sdlType = Fn<TypeFn>(m, "SDL_GetGamepadType"); sdlTypeName = Fn<TypeNameFn>(m, "SDL_GetGamepadStringForType");
            sdlGuid = Fn<GuidFn>(m, "SDL_GetGamepadGUIDForID"); sdlGuidStr = Fn<GuidStrFn>(m, "SDL_GUIDToString");
            sdlPump = Fn<VoidFn>(m, "SDL_PumpEvents"); sdlUpdate = Fn<VoidFn>(m, "SDL_UpdateGamepads");
            sdlButton = Fn<ButtonFn>(m, "SDL_GetGamepadButton"); sdlAxis = Fn<AxisFn>(m, "SDL_GetGamepadAxis");
            sdlHint("SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS", "1");
            sdlHint("SDL_JOYSTICK_ENHANCED_REPORTS", "auto");   // (as the game: no mode change on PlayStation pads)
            if (!sdlInit(0x2000u)) return "SDL_Init failed";   // SDL_INIT_GAMEPAD
            sdl = true;
            return null;
        } catch (Exception e) { return e.Message; }
    }

    static string Utf8(IntPtr p) {
        if (p == IntPtr.Zero) return null;
        int n = 0;
        while (Marshal.ReadByte(p, n) != 0) n++;
        byte[] b = new byte[n];
        Marshal.Copy(p, b, 0, n);
        return Encoding.UTF8.GetString(b);
    }

    // the pads connected now, in the order the game numbers them
    public static List<NsPad> List() {
        List<NsPad> pads = new List<NsPad>();
        if (sdl) {
            sdlPump();
            int count;
            IntPtr ids = sdlGetPads(out count);
            HashSet<uint> seen = new HashSet<uint>();
            for (int i = 0; i < count; i++) {
                uint id = (uint)Marshal.ReadInt32(ids, i * 4);
                seen.Add(id);
                NsPad p;
                if (!open.TryGetValue(id, out p)) {
                    IntPtr h = sdlOpen(id);
                    if (h == IntPtr.Zero) continue;
                    StringBuilder g = new StringBuilder(64);
                    sdlGuidStr(sdlGuid(id), g, g.Capacity);
                    p = new NsPad();
                    p.Handle = h; p.SdlId = id; p.Guid = g.ToString();
                    p.Name = Utf8(sdlName(h)) ?? "gamepad";
                    p.Type = Utf8(sdlTypeName(sdlType(h))) ?? "gamepad";
                    open[id] = p;
                }
                pads.Add(p);
            }
            if (ids != IntPtr.Zero) sdlFree(ids);
            foreach (uint id in new List<uint>(open.Keys)) if (!seen.Contains(id)) open.Remove(id);
            return pads;
        }
        for (int i = 0; i < 4; i++) {
            XState s;
            try { if (XInputGetState(i, out s) != 0) continue; } catch (DllNotFoundException) { return pads; }
            NsPad p = new NsPad();
            p.XIndex = i; p.Name = "XInput pad " + (i + 1); p.Type = "xinput"; p.Guid = "xinput" + i;
            pads.Add(p);
        }
        return pads;
    }

    // the pad's controls held down now; a trigger counts once pulled past triggerPercent
    public static string[] Held(NsPad p, int triggerPercent) {
        List<string> held = new List<string>();
        if (p == null) return held.ToArray();
        if (p.XIndex >= 0) {
            XState s;
            if (XInputGetState(p.XIndex, out s) != 0) return held.ToArray();
            for (int b = 0; b < XBits.Length; b++) if (XBits[b] != 0 && (s.Pad.Buttons & XBits[b]) != 0) held.Add(ButtonNames[b]);
            if (s.Pad.LT * 100 > triggerPercent * 255) held.Add("lefttrigger");
            if (s.Pad.RT * 100 > triggerPercent * 255) held.Add("righttrigger");
        } else if (sdl && p.Handle != IntPtr.Zero) {
            sdlPump(); sdlUpdate();
            for (int b = 0; b < ButtonNames.Length; b++) if (sdlButton(p.Handle, b)) held.Add(ButtonNames[b]);
            if (sdlAxis(p.Handle, 4) * 100 > triggerPercent * 32767) held.Add("lefttrigger");
            if (sdlAxis(p.Handle, 5) * 100 > triggerPercent * 32767) held.Add("righttrigger");
        }
        return held.ToArray();
    }
}
'@

# ---- what the settings are ------------------------------------------------------------------------
# the console's buttons, in controllers.ini's names, in the order the page shows them
$PadButtons = @('A', 'B', 'X', 'Y', 'L', 'R', 'ZL', 'ZR', 'PLUS', 'MINUS', 'LSTICK', 'RSTICK', 'UP', 'DOWN', 'LEFT', 'RIGHT')
$PadKeys = @('style', 'layout', 'deadzone', 'rumble', 'trigger_threshold') + $PadButtons
$PadBuiltIn = @{ style = 'auto'; layout = 'positional'; deadzone = '0'; rumble = '100'; trigger_threshold = '25' }

function Get-PadFamily([string]$Type) {
    switch -Regex ($Type) {
        '^ps\d' { 'ps' }
        '^(switchpro|joycon)' { 'nintendo' }
        'gamecube' { 'gamecube' }
        default { 'xbox' }
    }
}

function Get-PadSections {
    <# The [device ...] sections controllers.ini has now, without the "device " (in file order). #>
    $names = @()
    if (Test-Path -LiteralPath $padsIni) {
        foreach ($l in [IO.File]::ReadAllLines($padsIni)) { if ($l -match '^\s*\[\s*device\s+(.+?)\s*\]') { $names += $Matches[1] } }
    }
    return $names
}

function Get-PadSection($Pad) {
    <# The section a pad takes its settings from, as the game picks it: its GUID, then its name, then its type; else its name. #>
    $have = Get-PadSections
    foreach ($want in @($Pad.Guid, $Pad.Name, $Pad.Type)) {
        foreach ($h in $have) { if ($h -ne 'default' -and [string]::Equals($h, $want, [StringComparison]::OrdinalIgnoreCase)) { return $h } }
    }
    return $Pad.Name
}

function Get-PadInherited([string]$Key) {
    <# What a pad gets for $Key when its own section doesn't say: [device default], else the game's own default. #>
    $v = Get-IniValue $padsIni 'device default' $Key
    if ($null -ne $v -and $v -ne '') { return $v }
    return $PadBuiltIn[$Key]
}

function Get-PadValue([string]$Key) {
    <# The picked pad's value for $Key: what this window has for its section, else what it inherits. #>
    $v = $script:PadEdit[$script:PadSec][$Key]
    if ($null -ne $v -and $v -ne '') { return [string]$v }
    return Get-PadInherited $Key
}

function Get-PadDefaultMap([string]$Button) {
    <# The pad control a console button comes from without a remap (the game's dev_configure). #>
    $family = Get-PadFamily $script:PadNow.Type
    $style = Get-PadValue 'style'
    $gc = $style -eq 'gamecube' -or ($style -eq 'auto' -and $family -eq 'gamecube')
    if ($gc) {
        switch ($Button) { 'L' { return 'lefttrigger' } 'R' { return 'righttrigger' } 'ZR' { return 'rightshoulder' } 'ZL' { return 'none' } }
    }
    # by the printed letters: SDL pads with letters (an Xbox pad's A is its bottom button; a GameCube's A too)
    $lettered = $script:PadNow.XIndex -lt 0 -and ($family -eq 'xbox' -or $family -eq 'gamecube' -or $family -eq 'nintendo')
    if ($lettered -and ((Get-PadValue 'layout') -eq 'labels' -or $family -eq 'gamecube')) {
        $faces = switch ($family) {
            'xbox' { @{ A = 'south'; B = 'east'; X = 'west'; Y = 'north' } }
            'gamecube' { @{ A = 'south'; X = 'east'; B = 'west'; Y = 'north' } }
            default { @{ B = 'south'; A = 'east'; Y = 'west'; X = 'north' } }
        }
        if ($faces.ContainsKey($Button)) { return $faces[$Button] }
    }
    $positional = @{ A = 'east'; B = 'south'; X = 'north'; Y = 'west'; L = 'leftshoulder'; R = 'rightshoulder'; ZL = 'lefttrigger'; ZR = 'righttrigger'
                     PLUS = 'start'; MINUS = 'back'; LSTICK = 'leftstick'; RSTICK = 'rightstick'; UP = 'dpup'; DOWN = 'dpdown'; LEFT = 'dpleft'; RIGHT = 'dpright' }
    return $positional[$Button]
}

function Get-PadMap([string]$Button) {
    <# The pad controls (one or more) a console button comes from for the picked pad. #>
    $v = $script:PadEdit[$script:PadSec][$Button]
    if (-not $v) { $v = Get-IniValue $padsIni 'device default' $Button }
    if (-not $v) { $v = Get-PadDefaultMap $Button }
    return @(([string]$v -split ',') | ForEach-Object { $_.Trim().ToLower() } | Where-Object { $_ })
}

function Show-PadControl([string]$Control) {
    <# A pad control as its pad prints it ("A", "LB", "✕", "Cruceta ↑"...). #>
    $family = Get-PadFamily $script:PadNow.Type
    $faces = switch ($family) {
        'ps' { @{ south = '✕'; east = '○'; west = '□'; north = '△'; leftshoulder = 'L1'; rightshoulder = 'R1'; lefttrigger = 'L2'; righttrigger = 'R2'; start = 'Options'; back = 'Share'; guide = 'PS'; leftstick = 'L3'; rightstick = 'R3' } }
        'nintendo' { @{ south = 'B'; east = 'A'; west = 'Y'; north = 'X'; leftshoulder = 'L'; rightshoulder = 'R'; lefttrigger = 'ZL'; righttrigger = 'ZR'; start = '+'; back = '−'; guide = 'Home'; misc1 = (T 'padCapture'); leftstick = (T 'padLStick'); rightstick = (T 'padRStick') } }
        'gamecube' { @{ south = 'A'; east = 'X'; west = 'B'; north = 'Y'; rightshoulder = 'Z'; lefttrigger = 'L'; righttrigger = 'R'; start = 'Start' } }
        default { @{ south = 'A'; east = 'B'; west = 'X'; north = 'Y'; leftshoulder = 'LB'; rightshoulder = 'RB'; lefttrigger = 'LT'; righttrigger = 'RT'; start = 'Menu'; back = 'View'; guide = 'Xbox'; leftstick = (T 'padLStick'); rightstick = (T 'padRStick') } }
    }
    $dpad = @{ dpup = '↑'; dpdown = '↓'; dpleft = '←'; dpright = '→' }
    if ($faces.ContainsKey($Control)) { return $faces[$Control] }
    if ($dpad.ContainsKey($Control)) { return "$(T 'padDpad') $($dpad[$Control])" }
    if ($Control -eq 'leftstick') { return T 'padLStick' }
    if ($Control -eq 'rightstick') { return T 'padRStick' }
    if ($Control -eq 'none') { return '—' }
    return $Control
}

# ---- the page -------------------------------------------------------------------------------------
$script:PadEdit = @{}       # section -> @{ key -> value written under it ('' or $null: none) }
$script:PadList = @()
$script:PadNow = $null
$script:PadSec = $null
$script:PadCapture = $null  # the console button waiting for a press
$script:PadHeldBefore = @()
$script:PadCard = @{}
$script:PadFilling = $false

$sdlDll = if ($script:tree) { Join-Path $script:tree 'build\SDL3.dll' } else { $null }
if ($sdlDll -and (Test-Path -LiteralPath $sdlDll)) { [void][NsPads]::StartSdl($sdlDll) }

function Read-PadSection([string]$Section) {
    <# This window's copy of a section's values, read from the file once (and remembered as "before" for saving). #>
    if ($script:PadEdit.ContainsKey($Section)) { return }
    $vals = @{}
    foreach ($k in $PadKeys) {
        $v = Get-IniValue $padsIni "device $Section" $k
        $vals[$k] = if ($null -eq $v) { '' } else { [string]$v }
        if ($script:before) { $script:before["$padsIni|device $Section|$k"] = $vals[$k] }
    }
    $script:PadEdit[$Section] = $vals
}

function Get-PadValues {
    <# For Get-WindowValues: every section this window has read, key by key ($null: the key goes). #>
    $out = [ordered]@{}
    foreach ($sec in $script:PadEdit.Keys) {
        foreach ($k in $PadKeys) {
            $v = $script:PadEdit[$sec][$k]
            $out["$padsIni|device $sec|$k"] = if ($v) { [string]$v } else { $null }
        }
    }
    return $out
}

function Set-PadChips($Panel, $Items, [string]$Key) {
    <# A chip row for one of the pad's settings; a value the file has that the row lacks is added to it. #>
    $cur = Get-PadValue $Key
    if (-not ($Items | Where-Object { $_.Value -eq $cur })) { $Items = @($Items) + @(@{ Value = $cur; Text = $cur }) }
    New-Chips $Panel $Items $cur
    foreach ($c in $Panel.Children) {
        $c.Tag = [string]$c.Tag
        $c.Add_Checked({
            param($s, $e)
            if ($script:PadFilling -or -not $script:PadSec) { return }
            $script:PadEdit[$script:PadSec][$s.Parent.Tag] = [string]$s.Tag
            if ($s.Parent.Tag -in @('style', 'layout')) { Update-PadMap }
        })
    }
    $Panel.Tag = $Key
}

function Update-PadMap {
    foreach ($b in $PadButtons) {
        if ($script:PadCapture -eq $b) { continue }
        $script:PadCard[$b].Content = ((Get-PadMap $b) | ForEach-Object { Show-PadControl $_ }) -join ' + '
    }
}

function Show-Pad($Pad) {
    $script:PadCapture = $null
    $script:PadNow = $Pad
    if (-not $Pad) { (Find 'PadBody').Visibility = 'Collapsed'; return }
    $script:PadSec = Get-PadSection $Pad
    Read-PadSection $script:PadSec
    $script:PadFilling = $true
    $isX = $Pad.XIndex -ge 0
    (Find 'PadWhere').Text = ((T 'padWhere') -f $Pad.Name, $(if ($isX) { 'XInput' } else { $Pad.Type }), $script:PadSec)
    Set-PadChips (Find 'PadStyleChips') @(@{ Value = 'auto'; Text = (T 'padStyleAuto') }, @{ Value = 'pro'; Text = 'Pro Controller' }, @{ Value = 'gamecube'; Text = 'GameCube' }) 'style'
    Set-PadChips (Find 'PadLayoutChips') @(@{ Value = 'positional'; Text = (T 'padPositional') }, @{ Value = 'labels'; Text = (T 'padLabels') }) 'layout'
    # an XInput pad has no letters the game can see: only by position
    (Find 'PadLayoutBox').Visibility = if ($isX -or (Get-PadFamily $Pad.Type) -eq 'ps') { 'Collapsed' } else { 'Visible' }
    Set-PadChips (Find 'PadDeadChips') @(0, 5, 10, 15, 20, 30 | ForEach-Object { @{ Value = "$_"; Text = "$_ %" } }) 'deadzone'
    Set-PadChips (Find 'PadRumbleChips') (@(@{ Value = '0'; Text = (T 'padOff') }) + @(25, 50, 75, 100 | ForEach-Object { @{ Value = "$_"; Text = "$_ %" } })) 'rumble'
    Set-PadChips (Find 'PadTrigChips') @(10, 25, 40, 60, 80 | ForEach-Object { @{ Value = "$_"; Text = "$_ %" } }) 'trigger_threshold'
    $script:PadFilling = $false
    (Find 'PadMapNote').Text = ''
    $family = Get-PadFamily $Pad.Type
    (Find 'PadLetters').Visibility = if ($family -eq 'xbox' -or $family -eq 'gamecube') { 'Visible' } else { 'Collapsed' }
    Update-PadMap
    (Find 'PadBody').Visibility = 'Visible'
}

function Update-PadList {
    <# The connected pads as chips; the picked one stays picked while it is connected. #>
    $pads = @([NsPads]::List())
    $sig = ($pads | ForEach-Object { $_.Guid + '|' + $_.Name }) -join ';'
    if ($sig -eq $script:PadSig) { return }
    $script:PadSig = $sig
    $script:PadList = $pads
    $keep = if ($script:PadNow) { $script:PadNow.Guid + '|' + $script:PadNow.Name } else { '' }
    $items = @()
    for ($i = 0; $i -lt $pads.Count; $i++) {
        $p = $pads[$i]
        $text = if ($p.XIndex -ge 0) { (T 'padXinput') -f ($p.XIndex + 1) } else { "$($i + 1). $($p.Name)" }
        $items += @{ Value = "$i"; Text = $text }
    }
    $pick = 0
    for ($i = 0; $i -lt $pads.Count; $i++) { if ($pads[$i].Guid + '|' + $pads[$i].Name -eq $keep) { $pick = $i } }
    New-Chips (Find 'PadPickChips') $items "$pick"
    foreach ($c in (Find 'PadPickChips').Children) { $c.Add_Checked({ param($s, $e) Show-Pad $script:PadList[[int]$s.Tag] }) }
    (Find 'PadNone').Visibility = if ($pads.Count) { 'Collapsed' } else { 'Visible' }
    if ($pads.Count) { Show-Pad $pads[$pick] } else { Show-Pad $null }
}

# the console buttons, each a card with what it comes from on the pad
foreach ($b in $PadButtons) {
    $card = New-Object Windows.Controls.StackPanel
    $card.Width = 200; $card.Margin = '0,0,12,6'
    $lab = New-Object Windows.Controls.TextBlock
    $lab.Text = T "pb_$b"; $lab.Foreground = $win.FindResource('Muted'); $lab.FontSize = 12; $lab.Margin = '2,0,0,3'
    $btn = New-Object Windows.Controls.Button
    $btn.Style = $win.FindResource('Soft'); $btn.Margin = '0'; $btn.Tag = $b
    $btn.HorizontalContentAlignment = 'Center'
    [void]$card.Children.Add($lab); [void]$card.Children.Add($btn)
    [void](Find 'PadMapGrid').Children.Add($card)
    $script:PadCard[$b] = $btn
    $btn.Add_Click({
        param($s, $e)
        if (-not $script:PadNow) { return }
        if ($script:PadCapture) { $script:PadCard[$script:PadCapture].Content = ((Get-PadMap $script:PadCapture) | ForEach-Object { Show-PadControl $_ }) -join ' + ' }
        $script:PadCapture = [string]$s.Tag
        $script:PadHeldBefore = @([NsPads]::Held($script:PadNow, 50))   # (what is already held doesn't count)
        (Find 'PadMapNote').Text = ''
        $s.Content = T 'pressPad'
    })
}

function Set-PadButton([string]$Button, [string]$Control) {
    <# $Button now comes from $Control; a button that came from $Control alone takes $Button's old control (a swap). #>
    $old = @(Get-PadMap $Button)
    foreach ($o in $PadButtons) {
        if ($o -ne $Button) {
            $m = @(Get-PadMap $o)
            if ($m.Count -eq 1 -and $m[0] -eq $Control) { $script:PadEdit[$script:PadSec][$o] = ($old -join ', ') }
        }
    }
    $script:PadEdit[$script:PadSec][$Button] = $Control
    Update-PadMap
}

(Find 'PadLetters').Add_Click({
    <# A, B, X and Y as printed on the pad. An SDL pad with letters does it with layout = labels; an XInput pad
       (whose letters the game can't see) and a pad without that setting get the four remaps. #>
    if (-not $script:PadSec) { return }
    $family = Get-PadFamily $script:PadNow.Type
    $faces = switch ($family) {
        'nintendo' { @{ A = 'east'; B = 'south'; X = 'north'; Y = 'west' } }
        'gamecube' { @{ A = 'south'; B = 'west'; X = 'east'; Y = 'north' } }
        default { @{ A = 'south'; B = 'east'; X = 'west'; Y = 'north' } }
    }
    foreach ($b in $faces.Keys) { $script:PadEdit[$script:PadSec][$b] = $faces[$b] }
    Update-PadMap
})
(Find 'ResetPad').Add_Click({
    if (-not $script:PadSec) { return }
    foreach ($k in $PadKeys) { $script:PadEdit[$script:PadSec][$k] = '' }
    Show-Pad $script:PadNow
})

# watching the pads: plugged in or out, the button held lights up, a press while a button waits sets it
$script:padTimer = New-Object Windows.Threading.DispatcherTimer
$script:padTimer.Interval = [TimeSpan]::FromMilliseconds(40)
$script:padTick = 0
$script:padTimer.Add_Tick({
    if ((Find 'PadSection').Visibility -ne 'Visible' -or (Find 'PagePads').Visibility -ne 'Visible') { return }
    if (($script:padTick++ % 25) -eq 0) { Update-PadList }
    if (-not $script:PadNow) { return }
    $thr = [int](Get-Number (Get-PadValue 'trigger_threshold'))
    $held = @([NsPads]::Held($script:PadNow, [Math]::Max(1, $thr)))
    if ($script:PadCapture) {
        $new = @($held | Where-Object { $script:PadHeldBefore -notcontains $_ -and $_ -ne 'guide' })   # (guide: the game's menu)
        $script:PadHeldBefore = @($script:PadHeldBefore | Where-Object { $held -contains $_ })
        if ($new.Count) {
            $b = $script:PadCapture
            $script:PadCapture = $null
            Set-PadButton $b $new[0]
        }
        return
    }
    $accent = $win.FindResource('Accent')
    foreach ($b in $PadButtons) {
        $lit = $false
        foreach ($c in (Get-PadMap $b)) { if ($held -contains $c) { $lit = $true } }
        $card = $script:PadCard[$b]
        if ($lit) { $card.Background = $accent } else { $card.ClearValue([Windows.Controls.Control]::BackgroundProperty) }
    }
})

# the two tabs of the page: the keyboard, the pads
New-Chips (Find 'PadTabs') @(@{ Value = 'keys'; Text = (T 'tabKeys') }, @{ Value = 'pads'; Text = (T 'tabPads') }) $(if ($Page -eq 'controllers') { 'pads' } else { 'keys' })
$showTab = {
    $pads = (Get-Chip (Find 'PadTabs')) -eq 'pads'
    (Find 'KbSection').Visibility = if ($pads) { 'Collapsed' } else { 'Visible' }
    (Find 'PadSection').Visibility = if ($pads) { 'Visible' } else { 'Collapsed' }
    if ($pads) { Update-PadList; $script:padTimer.Start() } else { $script:padTimer.Stop(); $script:PadCapture = $null }
}
foreach ($c in (Find 'PadTabs').Children) { $c.Add_Checked($showTab) }
$win.Add_PreviewKeyDown({
    param($s, $e)
    if ($script:PadCapture -and $e.Key -eq 'Escape') {
        $e.Handled = $true
        $b = $script:PadCapture; $script:PadCapture = $null
        Update-PadMap
    }
})
$win.Add_Closed({ $script:padTimer.Stop() })
& $showTab
