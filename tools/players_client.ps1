# The launcher's side of NativeSmash's players server (Supabase: server/supabase/schema.sql): signing in, being online, the list of
# players online, invitations and quick match. Dot-sourced by launcher_wpf.ps1. The talking to the server runs on a thread of its
# own (a runspace) so the window never waits for it: the window puts requests in $NsNet.Ask and reads what came back from $NsNet
# (the list, the invitations, the count) and $NsNet.News (things that happened: signed in, a game to start, an error).
#
# The session (the server's refresh token and the player's name) is kept in %LOCALAPPDATA%\NativeSmash\session.json, out of the
# game's folder so that an update keeps it; the password is never kept.

$PlayersConfig = $null
try { $PlayersConfig = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'players.json') -Raw -Encoding UTF8 | ConvertFrom-Json } catch { }
$PlayersSessionFile = Join-Path $env:LOCALAPPDATA 'NativeSmash\session.json'

$NsNet = [hashtable]::Synchronized(@{
    On = [bool]($PlayersConfig -and $PlayersConfig.url -and $PlayersConfig.key)
    Url = if ($PlayersConfig) { ([string]$PlayersConfig.url).TrimEnd('/') } else { '' }
    Key = if ($PlayersConfig) { [string]$PlayersConfig.key } else { '' }
    SessionFile = $PlayersSessionFile
    Version = ''
    SignedIn = $false; Name = ''; UserId = ''
    State = 'launcher'          # what this player is doing: launcher | searching
    List = @(); Invites = @(); Friends = @(); Online = -1; Searching = 0; Stamp = 0
    Ask = New-Object 'System.Collections.Concurrent.ConcurrentQueue[hashtable]'
    News = New-Object 'System.Collections.Concurrent.ConcurrentQueue[hashtable]'
    Stop = $false
})

$PlayersLoop = {
    param($P)
    $access = ''; $refresh = ''; $accessUntil = [DateTime]::MinValue
    $sentInvite = $null; $quick = $false; $quickNc = 'rollback'
    $nextBeat = [DateTime]::MinValue; $nextList = [DateTime]::MinValue; $nextPoll = [DateTime]::MinValue

    function Say($Kind, $More) { $n = @{ Kind = $Kind }; if ($More) { foreach ($k in $More.Keys) { $n[$k] = $More[$k] } }; $P.News.Enqueue($n) }
    function Web([string]$Method, [string]$Path, $Body, [switch]$Anon) {
        $h = @{ apikey = $P.Key; 'Content-Type' = 'application/json' }
        $h.Authorization = 'Bearer ' + $(if ($Anon -or -not $access) { $P.Key } else { $access })
        $args_ = @{ Method = $Method; Uri = $P.Url + $Path; Headers = $h; TimeoutSec = 10; UseBasicParsing = $true }
        if ($null -ne $Body) { $args_.Body = [Text.Encoding]::UTF8.GetBytes(($Body | ConvertTo-Json -Compress -Depth 5)) }
        $res = Invoke-RestMethod @args_
        # (Windows PowerShell hands a JSON array on as one object: an empty answer - no invitations - was one invitation from nobody.
        # Returned from a variable, its items go out one by one, and an empty one is nothing)
        return $res
    }
    function Rpc([string]$Name, $Body) { if ($null -eq $Body) { $Body = @{} }; return Web 'POST' "/rest/v1/rpc/$Name" $Body }
    function Keep-Session($Tok) {
        $script:access = $Tok.access_token; $script:refresh = $Tok.refresh_token
        $script:accessUntil = [DateTime]::UtcNow.AddSeconds([int]$Tok.expires_in - 120)
        $P.UserId = [string]$Tok.user.id
        try {
            $prof = Web 'GET' "/rest/v1/profiles?id=eq.$($P.UserId)&select=username" $null
            if ($prof -and $prof[0].username) { $P.Name = [string]$prof[0].username }
        } catch { }
        try {
            New-Item -ItemType Directory -Force -Path (Split-Path $P.SessionFile) | Out-Null
            @{ refresh = $script:refresh; name = $P.Name } | ConvertTo-Json | Set-Content -LiteralPath $P.SessionFile -Encoding UTF8
        } catch { }
        $P.SignedIn = $true
        $script:nextBeat = [DateTime]::MinValue; $script:nextList = [DateTime]::MinValue
        Say 'signedIn' @{ Name = $P.Name }
    }
    function Sign-Out {
        try { if ($script:access) { [void](Rpc 'go_offline' $null) } } catch { }
        $script:access = ''; $script:refresh = ''
        try { Remove-Item -LiteralPath $P.SessionFile -ErrorAction SilentlyContinue } catch { }
        $P.SignedIn = $false; $P.Name = ''; $P.UserId = ''; $P.List = @(); $P.Invites = @(); $P.Stamp++
        Say 'signedOut' $null
    }
    function Why($E) {   # (the server's own words when it gave any)
        try { $m = ($E.ErrorDetails.Message | ConvertFrom-Json); foreach ($k in 'message', 'msg', 'error_description', 'error') { if ($m.$k) { return [string]$m.$k } } } catch { }
        return $E.Exception.Message
    }

    # the session from last time
    try {
        if (Test-Path -LiteralPath $P.SessionFile) {
            $s = Get-Content -LiteralPath $P.SessionFile -Raw -Encoding UTF8 | ConvertFrom-Json
            if ($s.refresh) { $P.Name = [string]$s.name; Keep-Session (Web 'POST' '/auth/v1/token?grant_type=refresh_token' @{ refresh_token = [string]$s.refresh } -Anon) }
        }
    } catch { Say 'signedOut' $null }

    while (-not $P.Stop) {
        try {
            $now = [DateTime]::UtcNow
            if ($access -and $now -gt $accessUntil -and $refresh) { Keep-Session (Web 'POST' '/auth/v1/token?grant_type=refresh_token' @{ refresh_token = $refresh } -Anon) }

            $ask = $null
            while ($P.Ask.TryDequeue([ref]$ask)) {
                try {
                    switch ($ask.Op) {
                        'signIn' { Keep-Session (Web 'POST' '/auth/v1/token?grant_type=password' @{ email = $ask.Email; password = $ask.Password } -Anon) }
                        'signOut' { Sign-Out }
                        'invite' {
                            $r = @(Rpc 'send_invite' @{ to_user = $ask.To; netcode = $ask.Netcode })
                            $sentInvite = @{ Id = $r[0].id; Code = [string]$r[0].room_code; Netcode = $ask.Netcode; To = $ask.Name }
                            Say 'inviteSent' @{ Name = $ask.Name }
                        }
                        'cancelInvite' { if ($sentInvite) { [void](Rpc 'cancel_invite' @{ invite_id = $sentInvite.Id }); Say 'inviteCancelled' @{ Name = $sentInvite.To }; $sentInvite = $null } }
                        'state' { $P.State = $ask.State; $nextBeat = [DateTime]::MinValue }
                        'friendAdd' { $r = [string](Rpc 'friend_request' @{ name = $ask.Name }); Say $(if ($r -eq 'accepted') { 'friendNow' } else { 'friendSent' }) @{ Name = $ask.Name }; $nextList = [DateTime]::MinValue }
                        'friendAnswer' { [void](Rpc 'friend_answer' @{ name = $ask.Name; accept = [bool]$ask.Accept }); $nextList = [DateTime]::MinValue }
                        'friendRemove' { [void](Rpc 'friend_remove' @{ name = $ask.Name }); $nextList = [DateTime]::MinValue }
                        'openMatch' {
                            $r = @(Rpc 'my_open_match' $null)
                            if ($r.Count -and -not $r[0].reported) { Say 'openMatch' @{ Id = $r[0].id; Opponent = [string]$r[0].opponent } } else { Say 'noMatch' $null }
                        }
                        'report' { $r = [string](Rpc 'report_result' @{ match_id = $ask.Id; result = $ask.Result }); Say 'reported' @{ Result = $r } }
                        'answer' {
                            $r = @(Rpc 'answer_invite' @{ invite_id = $ask.Id; accept = [bool]$ask.Accept })
                            $P.Invites = @($P.Invites | Where-Object { $_.Id -ne $ask.Id }); $P.Stamp++
                            if ($ask.Accept -and $r.Count -and $r[0].room_code) { Say 'start' @{ Role = 'join'; Code = [string]$r[0].room_code; Netcode = [string]$r[0].netcode; With = $ask.Name } }
                        }
                        'quick' {
                            $quickNc = $ask.Netcode
                            $r = @(Rpc 'quick_match' @{ netcode = $ask.Netcode })
                            if ($r.Count -and $r[0].status -eq 'paired') { $quick = $false; $P.State = 'launcher'; Say 'start' @{ Role = $(if ($r[0].hosts) { 'host' } else { 'join' }); Code = [string]$r[0].room_code; Netcode = $ask.Netcode; With = [string]$r[0].opponent } }
                            else { $quick = $true; $P.State = 'searching'; $nextBeat = [DateTime]::MinValue; Say 'searching' $null }
                        }
                        'quickCancel' { [void](Rpc 'quick_cancel' $null); $quick = $false; $P.State = 'launcher'; $nextBeat = [DateTime]::MinValue; Say 'quickStopped' $null }
                    }
                } catch { Say 'error' @{ Text = (Why $_) } }
            }

            if ($access) {
                if ($now -ge $nextBeat) { [void](Rpc 'heartbeat' @{ state = $P.State; version = $P.Version }); $nextBeat = $now.AddSeconds(20) }
                if ($now -ge $nextList) {
                    $P.List = @(Rpc 'online_players' $null | ForEach-Object { @{ Id = [string]$_.user_id; Name = [string]$_.username; State = [string]$_.state; Rating = $_.rating } })
                    $P.Invites = @(Rpc 'my_invites' $null | ForEach-Object { @{ Id = $_.id; Name = [string]$_.username; Netcode = [string]$_.netcode } })
                    $P.Online = [int](Rpc 'online_count' $null)
                    try { $P.Friends = @(Rpc 'my_friends' $null | ForEach-Object { @{ Id = [string]$_.user_id; Name = [string]$_.username; Avatar = [string]$_.avatar; Rating = $_.rating; Relation = [string]$_.relation; State = [string]$_.state } }) } catch { }
                    if ($quick) { try { $P.Searching = [int](Rpc 'searching_count' $null) } catch { } }
                    $P.Stamp++
                    $nextList = $now.AddSeconds(4)
                }
                if ($now -ge $nextPoll) {
                    if ($sentInvite) {
                        $st = [string](Rpc 'invite_status' @{ invite_id = $sentInvite.Id })
                        if ($st -eq 'accepted') { Say 'start' @{ Role = 'host'; Code = $sentInvite.Code; Netcode = $sentInvite.Netcode; With = $sentInvite.To }; $sentInvite = $null }
                        elseif ($st -in @('declined', 'expired', 'cancelled', '')) { Say 'inviteOver' @{ Name = $sentInvite.To; Why = $st }; $sentInvite = $null }
                    }
                    if ($quick) {
                        $r = @(Rpc 'quick_retry' @{ netcode = $quickNc })   # (paired yet - or paired now: each wait widens the range of points)
                        if ($r.Count -and $r[0].status -eq 'paired') { $quick = $false; $P.State = 'launcher'; Say 'start' @{ Role = $(if ($r[0].hosts) { 'host' } else { 'join' }); Code = [string]$r[0].room_code; Netcode = $quickNc; With = [string]$r[0].opponent } }
                        elseif (-not $r.Count) { $quick = $false; $P.State = 'launcher'; Say 'quickStopped' $null }
                    }
                    $nextPoll = $now.AddSeconds(2)
                }
            } elseif ($now -ge $nextList) {   # (signed out: only how many are online)
                try { $P.Online = [int](Web 'POST' '/rest/v1/rpc/online_count' @{} -Anon); $P.Stamp++ } catch { }
                $nextList = $now.AddSeconds(15)
            }
        } catch { Say 'error' @{ Text = (Why $_) }; Start-Sleep -Seconds 3 }
        Start-Sleep -Milliseconds 400
    }
    try { if ($access) { [void](Rpc 'go_offline' $null) } } catch { }
}

function Start-PlayersClient([string]$Version) {
    if (-not $NsNet.On) { return }
    $NsNet.Version = $Version
    $rs = [runspacefactory]::CreateRunspace(); $rs.Open()
    $ps = [powershell]::Create(); $ps.Runspace = $rs
    [void]$ps.AddScript($PlayersLoop).AddArgument($NsNet)
    $script:PlayersJob = @{ PS = $ps; Handle = $ps.BeginInvoke() }
}
function Stop-PlayersClient {
    if (-not $script:PlayersJob) { return }
    $NsNet.Stop = $true
    try { [void]$script:PlayersJob.Handle.AsyncWaitHandle.WaitOne(3000) } catch { }
}
function Request-Players([hashtable]$Ask) { if ($NsNet.On) { $NsNet.Ask.Enqueue($Ask) } }
