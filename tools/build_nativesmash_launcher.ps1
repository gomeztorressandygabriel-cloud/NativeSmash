<#
.SYNOPSIS
Builds NativeSmash.exe, NativeSmash's launcher (NativeSmash.Launcher: its window over OpenSmash's launcher code), into
the folder's root as one self-contained file (no .NET to install on the player's PC). Needs the .NET 8 SDK.

.PARAMETER WaitForProcessId
Wait for this process to end first (the launcher that asked for its own rebuild after an update).
.PARAMETER Start
Open NativeSmash.exe when it is built.
#>
param([int]$WaitForProcessId = 0, [switch]$Start)
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$log = Join-Path $Root 'reports\build_nativesmash_launcher.log'
New-Item -ItemType Directory -Force (Split-Path -Parent $log) | Out-Null
function Say([string]$t) { Write-Host $t; Add-Content -LiteralPath $log -Value ("{0:s}  {1}" -f (Get-Date), $t) }

if ($WaitForProcessId) {
    try { Wait-Process -Id $WaitForProcessId -Timeout 120 -ErrorAction Stop } catch { }
}
$project = Join-Path $Root 'NativeSmash.Launcher\NativeSmash.Launcher.csproj'
$out = Join-Path $Root 'NativeSmash.Launcher\bin\publish'
Say "building $project"
& dotnet publish $project -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -o $out -nologo -v q 2>&1 | ForEach-Object { Say "$_" }
if ($LASTEXITCODE -ne 0) { Say "dotnet publish failed ($LASTEXITCODE)"; exit 1 }
$exe = Join-Path $out 'NativeSmash.exe'
if (-not (Test-Path -LiteralPath $exe)) { Say 'NativeSmash.exe was not made'; exit 1 }
$dest = Join-Path $Root 'NativeSmash.exe'
for ($i = 0; $i -lt 20; $i++) {
    try { Copy-Item -LiteralPath $exe -Destination $dest -Force; break } catch { Start-Sleep -Milliseconds 500 }
}
Say "NativeSmash.exe: $((Get-Item -LiteralPath $dest).Length) bytes"
if ($Start) { Start-Process -FilePath $dest -WorkingDirectory $Root }
exit 0
