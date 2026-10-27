# Sends an error report after a game ends, when there is something to report: a new crash or stop report in reports\
# (crash_*.txt, stopped_*.txt), or online errors in the game's log. Run by the launchers\*.bat files once the game exits.
#
# Off when send_reports.txt says "off" (the launcher's "send error reports" box). Before sending, what could say who the
# player is comes out: the Windows user and PC names, the user folder's path, the online name and room code, IP and e-mail
# addresses. The report goes to the project's receiver (tools/reports_receiver: a Netlify function, nativesmash-reports.netlify.app), which files it in a private
# GitHub repository; the game holds no GitHub key. Quiet: no window, nothing shown, and no internet just means it is tried
# again after the next game.
param([string]$Tree = '.', [switch]$Preview)   # (-Preview: the report written to reports/report_preview.json, not sent nor marked sent)
$ErrorActionPreference = 'Stop'
try {
    $Root = Split-Path -Parent $PSScriptRoot
    Set-Location -LiteralPath $Root
    $optFile = Join-Path $Root 'send_reports.txt'
    if ((Test-Path -LiteralPath $optFile) -and ((Get-Content -LiteralPath $optFile -TotalCount 1) -match '^\s*off')) { exit 0 }

    $reportsDir = Join-Path $Root 'reports'
    $sentFile = Join-Path $reportsDir '.sent'
    $treeDir = if ($Tree -and $Tree -ne '.') { Join-Path $Root $Tree } else { $Root }
    $logFile = Join-Path $treeDir 'build\play.log'

    # reports not sent yet; the first time, only the last 10 minutes' (not an old backlog)
    $sent = @{}
    $first = -not (Test-Path -LiteralPath $sentFile)
    if (-not $first) { Get-Content -LiteralPath $sentFile | ForEach-Object { $sent[$_] = 1 } }
    $new = @()
    if (Test-Path -LiteralPath $reportsDir) {
        $new = @(Get-ChildItem -LiteralPath $reportsDir -File | Where-Object { $_.Name -match '^(crash|stopped)_.*\.txt$' -and -not $sent[$_.Name] } |
                 Where-Object { -not $first -or $Preview -or $_.LastWriteTime -gt (Get-Date).AddMinutes(-10) } | Sort-Object LastWriteTime)
        if ($first -and -not $Preview) {
            $old = Get-ChildItem -LiteralPath $reportsDir -File | Where-Object { $_.Name -match '^(crash|stopped)_.*\.txt$' -and $new.Name -notcontains $_.Name }
            if ($old) { Add-Content -LiteralPath $sentFile -Value ($old | ForEach-Object Name) -Encoding UTF8 }
        }
    }

    # the game's log: online errors in it, and its last lines
    $log = @()
    if (Test-Path -LiteralPath $logFile) { $log = @(Get-Content -LiteralPath $logFile -Tail 400 -ErrorAction SilentlyContinue) }
    $netErrors = @($log | Where-Object { $_ -match '(?i)^\[(net|ldn|relay|online|lks|lockstep|upnp)[^\]]*\].*(fail|error|could not|cannot|lost|timed? ?out|refused|no direct path|gave up|disconnect)' })
    $logStamp = if (Test-Path -LiteralPath $logFile) { 'log_' + (Get-Item -LiteralPath $logFile).LastWriteTime.ToString('yyyyMMdd_HHmmss') } else { '' }
    $onlineNew = $netErrors.Count -gt 0 -and $logStamp -and -not $sent[$logStamp]
    if (-not $new -and -not $onlineNew) { exit 0 }

    # what could say who the player is
    $secrets = @()
    if ($env:USERPROFILE) { $secrets += $env:USERPROFILE; $secrets += $env:USERPROFILE.Replace('\', '/') }
    foreach ($v in @($env:USERNAME, $env:COMPUTERNAME)) { if ($v -and $v.Length -ge 3) { $secrets += $v } }
    $nameFile = Join-Path $Root 'online_name.txt'
    if (Test-Path -LiteralPath $nameFile) { $n = (Get-Content -LiteralPath $nameFile | Where-Object { $_.Trim() -and $_ -notmatch '^\s*[#;]' } | Select-Object -First 1); if ($n -and $n.Trim().Length -ge 2) { $secrets += $n.Trim() } }
    $serverFile = Join-Path $Root 'online_server.txt'
    if (Test-Path -LiteralPath $serverFile) {   # (its first line that is not a comment: host:port, then the room code)
        $parts = (([string](Get-Content -LiteralPath $serverFile | Where-Object { $_.Trim() -and $_ -notmatch '^\s*[#;]' } | Select-Object -First 1)).Trim() -split '\s+')
        if ($parts.Count -ge 2 -and $parts[1].Length -ge 3) { $secrets += $parts[1] }
    }
    function Clean([string]$s) {
        if (-not $s) { return '' }
        foreach ($x in ($secrets | Sort-Object Length -Descending)) {   # (a name as a whole word only: "User" is not "UserData")
            $s = [regex]::Replace($s, '(?<![A-Za-z0-9])' + [regex]::Escape($x) + '(?![A-Za-z0-9])', '<private>', 'IgnoreCase')
        }
        $s = [regex]::Replace($s, '\b(?!127\.0\.0\.1\b)(?!0\.0\.0\.0\b)(\d{1,3}\.){3}\d{1,3}\b', '<ip>')
        $s = [regex]::Replace($s, '\b([0-9a-fA-F]{1,4}:){4,7}[0-9a-fA-F]{1,4}\b', '<ip>')
        $s = [regex]::Replace($s, '[\w.+-]+@[\w-]+\.[\w.-]+', '<email>')
        return $s
    }
    function Cut([string]$s, [int]$max) { if ($s.Length -gt $max) { return $s.Substring(0, $max) + "`n[... cut]" } else { return $s } }

    $version = ''
    $vf = Join-Path $Root 'version.txt'
    if (Test-Path -LiteralPath $vf) { $version = (Get-Content -LiteralPath $vf -TotalCount 1).Trim() }
    $gpu = ''
    if (Test-Path -LiteralPath $logFile) { $gpu = (Get-Content -LiteralPath $logFile -TotalCount 300 | Where-Object { $_ -match '^\[gfx\] drawing with ' } | Select-Object -First 1) }
    $texts = @()
    foreach ($f in ($new | Select-Object -Last 3)) {
        $texts += [ordered]@{ name = $f.Name; text = (Cut (Clean (Get-Content -LiteralPath $f.FullName -Raw -ErrorAction SilentlyContinue)) 15000) }
    }
    $body = [ordered]@{
        kind = if ($new) { 'crash' } else { 'online' }
        version = $version
        windows = [Environment]::OSVersion.VersionString
        gpu = (Clean $gpu)
        summary = (Clean (($new | Select-Object -Last 1 | ForEach-Object { Get-Content -LiteralPath $_.FullName -TotalCount 5 | Where-Object { $_.Trim() } | Select-Object -First 1 }) -join '').Trim('* '))
        reports = $texts
        online_errors = (Cut (Clean (($netErrors | Select-Object -Last 40) -join "`n")) 8000)
        log_tail = (Cut (Clean (($log | Select-Object -Last 250) -join "`n")) 25000)
    }
    if (-not $body.summary -and $netErrors) { $body.summary = (Clean ($netErrors | Select-Object -Last 1)) }
    $json = $body | ConvertTo-Json -Depth 5 -Compress

    if ($Preview) { [IO.File]::WriteAllText((Join-Path $reportsDir 'report_preview.json'), $json); exit 0 }
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    $url = 'https://nativesmash-reports.netlify.app/api/report'
    Invoke-WebRequest -Uri $url -Method Post -Body ([Text.Encoding]::UTF8.GetBytes($json)) -ContentType 'application/json; charset=utf-8' `
        -Headers @{ 'x-nativesmash' = '1' } -UseBasicParsing -TimeoutSec 20 | Out-Null

    $done = @($new | ForEach-Object Name)
    if ($onlineNew) { $done += $logStamp }
    if ($done) { Add-Content -LiteralPath $sentFile -Value $done -Encoding UTF8 }
} catch {
    if ($Preview) { Write-Output $_ }
    exit 0   # (no internet, receiver down...: tried again after the next game)
}
