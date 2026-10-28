# Pictures of the launcher for players (NATIVESMASH_SHOTS=<folder>): the Online page with the rollback test's choices made and
# marked in red with numbers, saved as PNG files, then the launcher quits at once - nothing is written to the settings files
# (the choices made here are only on screen). Dot-sourced by launcher_wpf.ps1 once its window is shown.
$shotDir = $env:NATIVESMASH_SHOTS
New-Item -ItemType Directory -Force -Path $shotDir | Out-Null

function Wait-Render { $win.Dispatcher.Invoke([action] {}, [Windows.Threading.DispatcherPriority]::ContextIdle) }
function Pause-Ui([int]$Ms) {   # (the window's opening fade and the pages' changes take a moment)
    $until = [DateTime]::Now.AddMilliseconds($Ms)
    while ([DateTime]::Now -lt $until) { Wait-Render; Start-Sleep -Milliseconds 30 }
}
function Scroll-To([string]$Name) {   # the Online page scrolled so that this element is near the top
    $sv = Find 'PageOnline'
    $el = Find $Name
    $y = $el.TransformToAncestor($sv.Content).Transform((New-Object Windows.Point 0, 0)).Y
    $sv.ScrollToVerticalOffset([Math]::Max(0, $y - 40))
    Pause-Ui 300
}

function Save-Shot([string]$Name, $Marks) {
    Wait-Render
    $win.UpdateLayout()
    $dpi = [Windows.Media.VisualTreeHelper]::GetDpi($win)
    $w = [int]($win.ActualWidth * $dpi.DpiScaleX); $h = [int]($win.ActualHeight * $dpi.DpiScaleY)
    $base = New-Object Windows.Media.Imaging.RenderTargetBitmap $w, $h, (96 * $dpi.DpiScaleX), (96 * $dpi.DpiScaleY), ([Windows.Media.PixelFormats]::Pbgra32)
    $base.Render($win)
    $dv = New-Object Windows.Media.DrawingVisual
    $dc = $dv.RenderOpen()
    $dc.DrawImage($base, (New-Object Windows.Rect 0, 0, $win.ActualWidth, $win.ActualHeight))
    $red = New-Object Windows.Media.SolidColorBrush ([Windows.Media.Color]::FromRgb(0xFF, 0x2D, 0x2D))
    $pen = New-Object Windows.Media.Pen $red, 4
    $face = New-Object Windows.Media.Typeface 'Segoe UI Black'
    foreach ($m in $Marks) {
        $el = $m.El
        $r = $el.TransformToAncestor($win).TransformBounds((New-Object Windows.Rect 0, 0, $el.ActualWidth, $el.ActualHeight))
        $r.Inflate(7, 5)
        $dc.DrawRoundedRectangle($null, $pen, $r, 10, 10)
        $c = New-Object Windows.Point ($r.Left - 4), ($r.Top + $r.Height / 2)
        $dc.DrawEllipse($red, $null, $c, 17, 17)
        $t = New-Object Windows.Media.FormattedText ([string]$m.N), ([Globalization.CultureInfo]::InvariantCulture), ([Windows.FlowDirection]::LeftToRight), $face, 20, ([Windows.Media.Brushes]::White), $dpi.PixelsPerDip
        $dc.DrawText($t, (New-Object Windows.Point ($c.X - $t.Width / 2), ($c.Y - $t.Height / 2)))
    }
    $dc.Close()
    $out = New-Object Windows.Media.Imaging.RenderTargetBitmap $w, $h, (96 * $dpi.DpiScaleX), (96 * $dpi.DpiScaleY), ([Windows.Media.PixelFormats]::Pbgra32)
    $out.Render($dv)
    $enc = New-Object Windows.Media.Imaging.PngBitmapEncoder
    $enc.Frames.Add([Windows.Media.Imaging.BitmapFrame]::Create($out))
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
    $rb = Pick 'NetcodeChips' 'rollback'
    $host_ = Pick 'VsChips' 'host'
    Pause-Ui 500
    Scroll-To 'DelayLabel'
    Save-Shot '1-rollback.png' @(@{ El = (Find 'NavOnline'); N = 1 }, @{ El = $rb; N = 2 })

    $d2 = Pick 'VsDelayChips' '2'
    Scroll-To 'VsTitle'
    Save-Shot '2-anfitrion.png' @(@{ El = $host_; N = 3 }, @{ El = (Find 'VsMine'); N = 4 }, @{ El = (Find 'VsCodeRow'); N = 5 }, @{ El = $d2; N = 6 })

    $join = Pick 'VsChips' 'join'
    $d2 = Pick 'VsDelayChips' '2'
    Pause-Ui 300
    Scroll-To 'VsTitle'
    Save-Shot '3-unirse.png' @(@{ El = $join; N = 3 }, @{ El = (Find 'VsPeerBox'); N = 4 }, @{ El = $d2; N = 5 })

    (Find 'NavPlay').IsChecked = $true
    Show-Page 'play'
    Pause-Ui 500
    Save-Shot '4-jugar.png' @(@{ El = (Find 'PlayBtn'); N = 7 })
} catch {
    [IO.File]::WriteAllText((Join-Path $shotDir 'error.txt'), [string]$_)
}
[Environment]::Exit(0)   # (no Closing handler: nothing saved)
