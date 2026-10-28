# A check of signing in from the launcher (NATIVESMASH_SHOTS=<folder>, NATIVESMASH_SHOTS_KIND=signin, NATIVESMASH_TEST_MAIL and
# NATIVESMASH_TEST_PASS): types them, presses Sign in as a player would, and after a few seconds writes what the window shows and
# any error to <folder>\signin.txt and a picture to signin.png; then the launcher quits.
$shotDir = $env:NATIVESMASH_SHOTS
New-Item -ItemType Directory -Force -Path $shotDir | Out-Null
function Wait-Render { $win.Dispatcher.Invoke([action] {}, [Windows.Threading.DispatcherPriority]::ContextIdle) }
function Pause-Ui([int]$Ms) { $until = [DateTime]::Now.AddMilliseconds($Ms); while ([DateTime]::Now -lt $until) { Wait-Render; Start-Sleep -Milliseconds 30 } }
$out = New-Object Collections.Generic.List[string]
try {
    Start-PlayersClient 'test'; $playersTimer.Start()
    Pause-Ui 1500
    (Find 'NavOnline').IsChecked = $true
    Show-Page 'online'
    (Find 'EmailBox').Text = $env:NATIVESMASH_TEST_MAIL
    (Find 'PassBox').Password = $env:NATIVESMASH_TEST_PASS
    (Find 'SignInBtn').RaiseEvent([Windows.RoutedEventArgs]::new([Windows.Controls.Primitives.ButtonBase]::ClickEvent))
    Pause-Ui 7000
    $out.Add("status: " + (Find 'NetStatus').Text)
    $out.Add("signedIn: $($NsNet.SignedIn) name: $($NsNet.Name) online: $($NsNet.Online) players: $($NsNet.List.Count)")
} catch { $out.Add("error: " + ($_ | Out-String)) }
foreach ($e in $Error) { $out.Add("PS error: " + ($e | Out-String)) }
$out | Set-Content -LiteralPath (Join-Path $shotDir 'signin.txt') -Encoding UTF8
$win.Close()
