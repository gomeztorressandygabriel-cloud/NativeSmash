# A picture of the Online page as players see it now (NATIVESMASH_SHOTS_KIND=lw2): Local Wireless, and the VS netplay switched off.
$shotDir = $env:NATIVESMASH_SHOTS
New-Item -ItemType Directory -Force -Path $shotDir | Out-Null
function Wait-Render { $win.Dispatcher.Invoke([action] {}, [Windows.Threading.DispatcherPriority]::ContextIdle) }
function Pause-Ui([int]$Ms) { $until = [DateTime]::Now.AddMilliseconds($Ms); while ([DateTime]::Now -lt $until) { Wait-Render; Start-Sleep -Milliseconds 30 } }
try {
    Pause-Ui 1500
    (Find 'NavOnline').IsChecked = $true
    Show-Page 'online'
    Pause-Ui 500
    (Find 'PageOnline').ScrollToEnd()
    Pause-Ui 400
    $dpi = [Windows.Media.VisualTreeHelper]::GetDpi($win)
    $bmp = New-Object Windows.Media.Imaging.RenderTargetBitmap ([int]($win.ActualWidth * $dpi.DpiScaleX)), ([int]($win.ActualHeight * $dpi.DpiScaleY)), (96 * $dpi.DpiScaleX), (96 * $dpi.DpiScaleY), ([Windows.Media.PixelFormats]::Pbgra32)
    $bmp.Render($win)
    $enc = New-Object Windows.Media.Imaging.PngBitmapEncoder
    $enc.Frames.Add([Windows.Media.Imaging.BitmapFrame]::Create($bmp))
    $fs = [IO.File]::Create((Join-Path $shotDir 'vs-desactivado.png')); $enc.Save($fs); $fs.Close()
} catch { Set-Content -LiteralPath (Join-Path $shotDir 'error.txt') -Value ($_ | Out-String) -Encoding UTF8 }
$win.Close()
