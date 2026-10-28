# Pictures of the Online page's new look (NATIVESMASH_SHOTS=<folder>, NATIVESMASH_SHOTS_KIND=online): the page as it is, then with
# made-up players online and an invitation (what the players server will fill in), saved as PNG files; then the launcher quits at
# once - nothing is written to the settings files. Dot-sourced by launcher_wpf.ps1 once its window is shown.
$shotDir = $env:NATIVESMASH_SHOTS
New-Item -ItemType Directory -Force -Path $shotDir | Out-Null

function Wait-Render { $win.Dispatcher.Invoke([action] {}, [Windows.Threading.DispatcherPriority]::ContextIdle) }
function Pause-Ui([int]$Ms) {
    $until = [DateTime]::Now.AddMilliseconds($Ms)
    while ([DateTime]::Now -lt $until) { Wait-Render; Start-Sleep -Milliseconds 30 }
}
function Scroll-To([string]$Name) {
    $sv = Find 'PageOnline'
    if (-not $Name) { $sv.ScrollToVerticalOffset(0); Pause-Ui 300; return }
    $el = Find $Name
    $y = $el.TransformToAncestor($sv.Content).Transform((New-Object Windows.Point 0, 0)).Y
    $sv.ScrollToVerticalOffset([Math]::Max(0, $y - 30))
    Pause-Ui 300
}
function Save-Shot([string]$Name) {
    Wait-Render
    $win.UpdateLayout()
    $dpi = [Windows.Media.VisualTreeHelper]::GetDpi($win)
    $w = [int]($win.ActualWidth * $dpi.DpiScaleX); $h = [int]($win.ActualHeight * $dpi.DpiScaleY)
    $bmp = New-Object Windows.Media.Imaging.RenderTargetBitmap $w, $h, (96 * $dpi.DpiScaleX), (96 * $dpi.DpiScaleY), ([Windows.Media.PixelFormats]::Pbgra32)
    $bmp.Render($win)
    $enc = New-Object Windows.Media.Imaging.PngBitmapEncoder
    $enc.Frames.Add([Windows.Media.Imaging.BitmapFrame]::Create($bmp))
    $fs = [IO.File]::Create((Join-Path $shotDir $Name))
    $enc.Save($fs); $fs.Close()
}
function Pick([string]$Panel, [string]$Value) {
    foreach ($ch in (Find $Panel).Children) { if ([string]$ch.Tag -eq $Value) { $ch.IsChecked = $true; return $ch } }
}

try {
    Pause-Ui 1500
    (Find 'NavOnline').IsChecked = $true
    Show-Page 'online'
    [void](Pick 'NetcodeChips' 'rollback')
    [void](Pick 'VsDelayChips' 'auto')
    Show-Advanced $false
    Pause-Ui 500
    Scroll-To $null
    Save-Shot '1-en-linea-arriba.png'
    Scroll-To 'VsCardTitle'
    Save-Shot '2-vs-en-red-sin-servidor.png'
    $NsNet.On = $true; Show-Account
    Pause-Ui 300
    Scroll-To 'VsCardTitle'
    Save-Shot '2b-iniciar-sesion.png'
    $NsNet.SignedIn = $true; $NsNet.Name = 'KingGMZ'; Show-Account

    $script:onlineTotal = 37
    $script:players = @(
        @{ Name = 'Zeuss'; State = 'launcher'; Ping = '1240 pts' },
        @{ Name = 'Aomine'; State = 'searching'; Ping = '1105 pts' },
        @{ Name = 'BloodyRD'; State = 'playing'; Ping = '1380 pts' },
        @{ Name = 'Lunetax'; State = 'launcher'; Ping = '990 pts' })
    $script:invites = @(@{ Name = 'Sennin'; Ping = '' })
    Show-People
    Pause-Ui 400
    Scroll-To 'VsCardTitle'
    Save-Shot '3-vs-en-red-con-jugadores.png'
    Say-Net ((T 'inviteSent') -f 'Zeuss')
    Pause-Ui 200
    Scroll-To 'PlayersLabel'
    Save-Shot '4-jugadores-y-codigo.png'
    Show-Advanced $true
    Pause-Ui 300
    Scroll-To 'AdvToggle'
    Save-Shot '5-conectar-con-codigo.png'
} catch {
    Set-Content -LiteralPath (Join-Path $shotDir 'error.txt') -Value ($_ | Out-String) -Encoding UTF8
}
$win.Close()
