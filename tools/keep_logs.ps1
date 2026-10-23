# Keeps the log of the game that just ended: build\play.log and build\monitor.csv move to build\logs\ (named by when they were
# written) before the next game starts and writes new ones; the newest 10 sessions are kept. A game that closed by itself leaves
# its story there to read. Called by the launchers: keep_logs.ps1 <the version's build folder>
param([string]$Build)
try {
    $log = Join-Path $Build 'play.log'
    if (-not (Test-Path -LiteralPath $log)) { return }
    $dir = Join-Path $Build 'logs'
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    $stamp = (Get-Item -LiteralPath $log).LastWriteTime.ToString('yyyyMMdd_HHmmss')
    foreach ($name in @('play.log', 'monitor.csv')) {
        $file = Join-Path $Build $name
        if (Test-Path -LiteralPath $file) {
            $base, $ext = [IO.Path]::GetFileNameWithoutExtension($name), [IO.Path]::GetExtension($name)
            Move-Item -LiteralPath $file -Destination (Join-Path $dir "${base}_$stamp$ext") -Force
        }
    }
    foreach ($base in @('play', 'monitor')) {
        Get-ChildItem -LiteralPath $dir -Filter "${base}_*" | Sort-Object Name -Descending | Select-Object -Skip 10 | Remove-Item -Force
    }
} catch { }
