# NativeSmash's progress window (WPF): what the installer is doing, in the launcher's look, with a bar, the steps, and the
# technical output behind "Ver detalles". Dot-sourced by tools\install_windowsmash.ps1, which opens it (when Install
# NativeSmash.exe started it with NATIVESMASH_WINDOW=1, so that no console window shows) and feeds it.
#
#   $S = Start-ProgressWindow -Lang es -Root <folder> -MainPid $PID      the window, on a thread of its own (or $null)
#   Set-ProgressStep $S 9                                                 the installer's step n of 10 begins
#   Add-ProgressLine $S '<a line a tool printed>'                         shown in the details; a bar's percent moves the bar
#   Set-ProgressVisible $S $false                                         hides it while another window asks something
#   Close-Progress $S -Error '<why>'  /  -Done                            the end: waits until the player closes the window
#   $Script:ProgressSnapshot: not used; a snapshot is asked with $S.Snapshot = <png path> (for checking the look)

# what each installer step is worth in the bar (they add up to 100); the build is nearly all of it
$Script:ProgressWeights = @{ 1 = 2; 2 = 2; 3 = 4; 4 = 12; 5 = 2; 6 = 2; 7 = 2; 8 = 2; 9 = 70; 10 = 2 }
# the steps the player sees: which installer steps each one holds
$Script:ProgressPhases = @(@(1, 2, 3), @(4, 5, 6), @(7, 8), @(9), @(10))

function Get-ProgressWords {
    param([string]$Lang)
    if ($Lang -eq 'en') {
        return @{
            heads = @{ 1 = 'Looking for updates'; 2 = 'Checking your PC'; 3 = 'Getting Python ready'; 4 = 'Getting the compiler'; 5 = 'Getting controller support ready'; 6 = 'Getting music and video ready'
                       7 = 'Reading your game files'; 8 = 'Creating the shortcut'; 9 = 'Building the game'; 10 = 'Preparing shaders' }
            notes = @{ 1 = 'Checking whether there is a newer version.'; 2 = 'Checking that your PC can run the game.'; 3 = 'Python is what the installer works with.'
                       4 = 'Downloading Microsoft''s compiler (about 570 MB). This is done only once.'; 5 = 'Setting up support for controllers.'; 6 = 'Setting up music and videos.'
                       7 = 'Looking at the files you chose.'; 8 = 'Creating the shortcut on your desktop.'
                       9 = 'The game is built on your PC. The first time takes from 30 minutes to a few hours. You can leave the PC working; do not turn it off or let it sleep.'
                       10 = 'Preparing the shaders in the background.' }
            phases = @('Getting ready', 'Downloading tools', 'Your game files', 'Building the game', 'Finishing')
            details = 'Show details'; hide = 'Hide details'; left = 'left'; calc = 'working out the time left'
            doneHead = 'All done!'; doneNote = 'NativeSmash is ready. Open it from the shortcut on your desktop, or press Play now.'; doneNoteShaders = ' Shaders keep being prepared in the background; you can play meanwhile.'
            errHead = 'Something went wrong'; errNote = 'Nothing is lost: open the installer again and it carries on where it stopped. The details below say why.'
            play = 'Play now'; close = 'Close'; log = 'Open the log'; updatedNote = 'Updated. Opening NativeSmash...'
            confirm = 'Stop the installation? You can open it again later and it carries on where it stopped.'; confirmTitle = 'NativeSmash'
            ending = 'Almost there'
        }
    }
    return @{
        heads = @{ 1 = 'Buscando actualizaciones'; 2 = 'Revisando tu PC'; 3 = 'Preparando Python'; 4 = 'Consiguiendo el compilador'; 5 = 'Preparando el soporte de mandos'; 6 = 'Preparando música y vídeo'
                   7 = 'Leyendo tus archivos del juego'; 8 = 'Creando el acceso directo'; 9 = 'Construyendo el juego'; 10 = 'Preparando los shaders' }
        notes = @{ 1 = 'Mirando si hay una versión más nueva.'; 2 = 'Comprobando que tu PC puede con el juego.'; 3 = 'Python es con lo que trabaja el instalador.'
                   4 = 'Descargando el compilador de Microsoft (unos 570 MB). Solo se hace una vez.'; 5 = 'Dejando listo el soporte de mandos.'; 6 = 'Dejando listas la música y los vídeos.'
                   7 = 'Mirando los archivos que elegiste.'; 8 = 'Creando el acceso directo del escritorio.'
                   9 = 'El juego se construye en tu PC. La primera vez tarda de 30 minutos a unas horas. Puedes dejar el PC trabajando; no lo apagues ni lo dejes dormir.'
                   10 = 'Preparando los shaders en segundo plano.' }
        phases = @('Preparación', 'Descargar herramientas', 'Tus archivos del juego', 'Construir el juego', 'Terminar')
        details = 'Ver detalles'; hide = 'Ocultar detalles'; left = 'faltan'; calc = 'calculando el tiempo que falta'
        updatedNote = 'Actualizado. Abriendo NativeSmash...'; doneHead = '¡Listo!'; doneNote = 'NativeSmash está instalado. Ábrelo desde el acceso directo del escritorio, o pulsa Jugar ahora.'; doneNoteShaders = ' Los shaders se siguen preparando en segundo plano; puedes jugar mientras.'
        errHead = 'Algo salió mal'; errNote = 'No se pierde nada: abre el instalador otra vez y sigue donde se quedó. Los detalles de abajo dicen el motivo.'
        play = 'Jugar ahora'; close = 'Cerrar'; log = 'Abrir el registro'
        confirm = '¿Detener la instalación? Puedes abrirla otra vez más tarde y sigue donde se quedó.'; confirmTitle = 'NativeSmash'
        ending = 'Casi listo'
    }
}

# ---- what a tool prints, understood (pure) -------------------------------------------------------------------------------
function Format-ProgressLeft {
    <# progress.py's "about 2 h 05 min left", "about 40 min left", "under a minute left" in the player's language. #>
    param([string]$Text, [string]$Lang)
    $es = $Lang -ne 'en'
    if ($Text -match 'under a minute left') { return $(if ($es) { 'menos de un minuto' } else { 'under a minute' }) }
    if ($Text -match 'about (\d+) h (\d+) min left') { return $(if ($es) { "unas $($Matches[1]) h $($Matches[2]) min" } else { "about $($Matches[1]) h $($Matches[2]) min" }) }
    if ($Text -match 'about (\d+) min left') { return $(if ($es) { "unos $($Matches[1]) min" } else { "about $($Matches[1]) min" }) }
    return $null
}

function Format-ProgressLabel {
    <# progress.py's "what is being done" in the player's language. #>
    param([string]$Text, [string]$Lang)
    $es = $Lang -ne 'en'
    $big = ''
    if ($Text -match ' - a big one takes minutes$') { $big = $(if ($es) { ' (uno grande tarda unos minutos)' } else { ' (a big one takes minutes)' }); $Text = $Text -replace ' - a big one takes minutes$', '' }
    if ($Text -match '^writing the C code: .+, (\d+) of (\d+) modules') { return $(if ($es) { "Escribiendo el código del juego: parte $($Matches[1]) de $($Matches[2])" } else { "Writing the game's code: part $($Matches[1]) of $($Matches[2])" }) }
    if ($Text -match '^compiling (.+): ([\d,]+) of ([\d,]+) files') { return $(if ($es) { "Compilando: parte $($Matches[2]) de $($Matches[3])" } else { "Compiling: part $($Matches[2]) of $($Matches[3])" }) + $big }
    if ($Text -match '^linking') { return $(if ($es) { 'Uniendo todo en un solo programa' } else { 'Joining everything into one program' }) }
    return $Text
}

function ConvertFrom-ToolLine {
    <# What a line of a tool's output says to the window: @{Kind='bar'; Percent; Left; Label} for progress.py's bar
       ("[####----]  12%  about 40 min left  (compiling x: 3 of 9 files)"), @{Kind='download'; Percent} for
       "label: 40% of 570 MB", @{Kind='substep'; Index} for build_game's "[4/9] ...", else @{Kind='text'}. #>
    param([string]$Line)
    if ($Line -match '^\s*\[[#\-]+\]\s+(\d+)%\s+(.*?)\s*$') {
        $rest = $Matches[2]; $pct = [int]$Matches[1]; $label = ''
        if ($rest -match '^(.*?)\s+\((.*)\)$') { $rest = $Matches[1]; $label = $Matches[2] }
        return @{ Kind = 'bar'; Percent = $pct; Left = $rest; Label = $label }
    }
    if ($Line -match '^\s*.+:\s+(\d+)% of [\d.]+ MB\s*$') { return @{ Kind = 'download'; Percent = [int]$Matches[1] } }
    if ($Line -match '^\s*\[(\d)/9\]') { return @{ Kind = 'substep'; Index = [int]$Matches[1] } }
    return @{ Kind = 'text' }
}

# ---- the window (its own thread) -----------------------------------------------------------------------------------------
$Script:ProgressUi = {
    param($S)
    try {
        Add-Type -AssemblyName PresentationFramework, PresentationCore, WindowsBase
        $xaml = @'
<Window xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
        xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
        Title="NativeSmash" Width="720" SizeToContent="Height" WindowStyle="None" AllowsTransparency="True"
        Background="Transparent" ResizeMode="NoResize" WindowStartupLocation="CenterScreen"
        FontFamily="Segoe UI" TextOptions.TextFormattingMode="Display" UseLayoutRounding="True">
  <Border Margin="14" CornerRadius="22" BorderThickness="1" BorderBrush="#26FFFFFF">
    <Border.Background>
      <LinearGradientBrush StartPoint="0,0" EndPoint="1,1">
        <GradientStop Color="#FF171A33" Offset="0"/><GradientStop Color="#FF0B0D1D" Offset="1"/>
      </LinearGradientBrush>
    </Border.Background>
    <Border.Effect><DropShadowEffect BlurRadius="28" ShadowDepth="0" Opacity="0.55"/></Border.Effect>
    <StackPanel Margin="34,22,34,28">
      <Grid x:Name="TopBar" Background="Transparent" Margin="0,0,0,18">
        <StackPanel Orientation="Horizontal">
          <Image x:Name="Logo" Width="34" Height="34" RenderOptions.BitmapScalingMode="HighQuality"/>
          <TextBlock Text="NativeSmash" Foreground="#FFF2F4FB" FontSize="19" FontWeight="SemiBold" VerticalAlignment="Center" Margin="10,0,0,0"/>
        </StackPanel>
        <StackPanel Orientation="Horizontal" HorizontalAlignment="Right" VerticalAlignment="Top">
          <Button x:Name="MinBtn" Content="&#x2013;" Width="34" Height="28" FontSize="16" Foreground="#FF9AA1BC" Background="Transparent" BorderThickness="0" Cursor="Hand"/>
          <Button x:Name="XBtn" Content="&#x2715;" Width="34" Height="28" FontSize="13" Foreground="#FF9AA1BC" Background="Transparent" BorderThickness="0" Cursor="Hand"/>
        </StackPanel>
      </Grid>
      <TextBlock x:Name="Head" Foreground="#FFF2F4FB" FontSize="30" FontWeight="SemiBold" TextWrapping="Wrap"/>
      <TextBlock x:Name="Note" Foreground="#FF9AA1BC" FontSize="15" TextWrapping="Wrap" Margin="0,8,0,0" LineHeight="22"/>
      <Border x:Name="Track" Height="16" CornerRadius="8" Background="#FF22264A" Margin="0,26,0,0" HorizontalAlignment="Stretch" ClipToBounds="True">
        <Border x:Name="Fill" HorizontalAlignment="Left" Width="0" CornerRadius="8">
          <Border.Background>
            <LinearGradientBrush StartPoint="0,0" EndPoint="1,0"><GradientStop Color="#FF6D7DFF" Offset="0"/><GradientStop Color="#FFA66BFF" Offset="1"/></LinearGradientBrush>
          </Border.Background>
        </Border>
      </Border>
      <Grid Margin="0,10,0,0">
        <TextBlock x:Name="Detail" Foreground="#FF9AA1BC" FontSize="14" TextTrimming="CharacterEllipsis" Margin="0,0,150,0"/>
        <TextBlock x:Name="Pct" Foreground="#FFF2F4FB" FontSize="14" FontWeight="SemiBold" HorizontalAlignment="Right"/>
      </Grid>
      <StackPanel x:Name="Phases" Margin="0,22,0,0"/>
      <TextBlock x:Name="DetailsLink" Foreground="#FF8E9BFF" FontSize="14" TextDecorations="Underline" Cursor="Hand" Margin="0,18,0,0" HorizontalAlignment="Left"/>
      <TextBox x:Name="LogBox" Visibility="Collapsed" Height="190" Margin="0,10,0,0" IsReadOnly="True" TextWrapping="NoWrap"
               VerticalScrollBarVisibility="Auto" HorizontalScrollBarVisibility="Auto" FontFamily="Consolas" FontSize="12"
               Foreground="#FFC8CDE6" Background="#FF0E1124" BorderBrush="#26FFFFFF" Padding="8"/>
      <StackPanel x:Name="Buttons" Visibility="Collapsed" Orientation="Horizontal" Margin="0,24,0,0">
        <Button x:Name="MainBtn" Height="46" MinWidth="170" Padding="22,0" FontSize="16" FontWeight="SemiBold" Foreground="White" BorderThickness="0" Cursor="Hand" Margin="0,0,10,0">
          <Button.Template><ControlTemplate TargetType="Button"><Border x:Name="B" CornerRadius="14" Background="{TemplateBinding Background}"><ContentPresenter HorizontalAlignment="Center" VerticalAlignment="Center"/></Border>
            <ControlTemplate.Triggers><Trigger Property="IsMouseOver" Value="True"><Setter TargetName="B" Property="Opacity" Value="0.88"/></Trigger></ControlTemplate.Triggers></ControlTemplate></Button.Template>
          <Button.Background><LinearGradientBrush StartPoint="0,0" EndPoint="1,1"><GradientStop Color="#FF6D7DFF" Offset="0"/><GradientStop Color="#FFA66BFF" Offset="1"/></LinearGradientBrush></Button.Background>
        </Button>
        <Button x:Name="SecondBtn" Height="46" MinWidth="120" Padding="22,0" FontSize="16" FontWeight="SemiBold" Foreground="White" BorderThickness="0" Cursor="Hand">
          <Button.Template><ControlTemplate TargetType="Button"><Border x:Name="B" CornerRadius="14" Background="{TemplateBinding Background}"><ContentPresenter HorizontalAlignment="Center" VerticalAlignment="Center"/></Border>
            <ControlTemplate.Triggers><Trigger Property="IsMouseOver" Value="True"><Setter TargetName="B" Property="Opacity" Value="0.88"/></Trigger></ControlTemplate.Triggers></ControlTemplate></Button.Template>
          <Button.Background><SolidColorBrush Color="#FF2A2E4D"/></Button.Background>
        </Button>
      </StackPanel>
    </StackPanel>
  </Border>
</Window>
'@
        $win = [Windows.Markup.XamlReader]::Parse($xaml)
        $f = { param($n) $win.FindName($n) }
        $W = Get-ProgressWords $S.Lang
        $brush = { param($hex) New-Object Windows.Media.SolidColorBrush ([Windows.Media.ColorConverter]::ConvertFromString($hex)) }
        $png = Join-Path $S.Root 'docs\assets\nativesmash-512.png'
        if (Test-Path -LiteralPath $png) {
            $bmp = New-Object Windows.Media.Imaging.BitmapImage
            $bmp.BeginInit(); $bmp.UriSource = New-Object Uri($png); $bmp.CacheOption = 'OnLoad'; $bmp.EndInit()
            (& $f 'Logo').Source = $bmp; $win.Icon = $bmp
        }
        # the steps the player sees
        $rows = @()
        foreach ($name in $W.phases) {
            $row = New-Object Windows.Controls.StackPanel; $row.Orientation = 'Horizontal'; $row.Margin = '0,3,0,3'
            $mark = New-Object Windows.Controls.TextBlock; $mark.Width = 26; $mark.FontSize = 15; $mark.FontFamily = 'Segoe UI Symbol'
            $text = New-Object Windows.Controls.TextBlock; $text.FontSize = 15; $text.Text = $name
            [void]$row.Children.Add($mark); [void]$row.Children.Add($text); [void](& $f 'Phases').Children.Add($row)
            $rows += , @($mark, $text)
        }
        (& $f 'DetailsLink').Text = $W.details
        $script:shown = 0.0
        $script:logLines = 0
        $script:closing = $false
        $render = {
            $step = [int]$S.Step
            $state = $S.State
            $target = [double]$S.Percent
            if ($state -eq 'ok') { $target = 100 }
            $script:shown = $script:shown + ($target - $script:shown) * 0.12
            if ([math]::Abs($target - $script:shown) -lt 0.05) { $script:shown = $target }
            $track = & $f 'Track'
            (& $f 'Fill').Width = [math]::Max(0, ($track.ActualWidth * $script:shown / 100))
            (& $f 'Pct').Text = if ($state -eq 'run' -and $S.Eta) { ('{0}%  ·  {1} {2}' -f [int]$script:shown, $W.left, $S.Eta) } else { ('{0}%' -f [int]$script:shown) }
            if ($state -eq 'run') {
                (& $f 'Head').Text = if ($step -ge 1) { $W.heads[$step] } else { '' }
                (& $f 'Note').Text = if ($step -ge 1) { $W.notes[$step] } else { '' }
                (& $f 'Detail').Text = $S.Detail
            }
            $current = -1
            for ($i = 0; $i -lt $W.phases.Count; $i++) { if ($step -in $Script:ProgressPhases[$i]) { $current = $i } }
            for ($i = 0; $i -lt $rows.Count; $i++) {
                $mark, $text = $rows[$i]
                if ($state -eq 'ok' -or $i -lt $current) { $mark.Text = [string][char]0x2713; $mark.Foreground = & $brush '#FF7BE0A4'; $text.Foreground = & $brush '#FF9AA1BC' }
                elseif ($i -eq $current -and $state -eq 'run') { $mark.Text = [string][char]0x25CF; $mark.Foreground = & $brush '#FF8E9BFF'; $text.Foreground = & $brush '#FFF2F4FB' }
                elseif ($i -eq $current -and $state -eq 'error') { $mark.Text = [string][char]0x2715; $mark.Foreground = & $brush '#FFFF7B8A'; $text.Foreground = & $brush '#FFF2F4FB' }
                else { $mark.Text = [string][char]0x25CB; $mark.Foreground = & $brush '#FF5B6285'; $text.Foreground = & $brush '#FF5B6285' }
            }
            $line = $null
            $box = & $f 'LogBox'
            $added = $false
            while ($S.Log.TryDequeue([ref]$line)) {
                $box.AppendText($line + "`r`n"); $script:logLines++; $added = $true
                if ($script:logLines -gt 600) { $box.Text = $box.Text.Substring($box.Text.Length / 3); $script:logLines = 400 }
            }
            if ($added -and $box.Visibility -eq 'Visible') { $box.ScrollToEnd() }
            if ($S.Visible -and -not $win.IsVisible -and -not $script:closing) { $win.Show() }
            if (-not $S.Visible -and $win.IsVisible) { $win.Hide() }
            if ($S.Snapshot) {
                $r = New-Object Windows.Media.Imaging.RenderTargetBitmap([int]$win.ActualWidth, [int]$win.ActualHeight, 96, 96, [Windows.Media.PixelFormats]::Pbgra32)
                $r.Render($win)
                $enc = New-Object Windows.Media.Imaging.PngBitmapEncoder; $enc.Frames.Add([Windows.Media.Imaging.BitmapFrame]::Create($r))
                $fs = [IO.File]::Create($S.Snapshot); $enc.Save($fs); $fs.Close(); $S.Snapshot = $null
            }
        }
        $finish = {
            $buttons = & $f 'Buttons'
            if ($S.State -eq 'ok') {
                (& $f 'Head').Text = $W.doneHead
                (& $f 'Note').Text = $W.doneNote + $(if ($S.ShadersLater) { $W.doneNoteShaders } else { '' })
                (& $f 'Detail').Text = ''
                (& $f 'MainBtn').Content = $W.play; (& $f 'MainBtn').Tag = 'play'
                if ($S.Update) {   # (an update started from the launcher, which closed: the new one opens by itself, and this window goes)
                    (& $f 'Note').Text = $W.updatedNote
                    $script:reopen = New-Object Windows.Threading.DispatcherTimer
                    $script:reopen.Interval = [TimeSpan]::FromMilliseconds(1800)
                    $script:reopen.Add_Tick({ $script:reopen.Stop(); (& $f 'MainBtn').RaiseEvent([Windows.RoutedEventArgs]::new([Windows.Controls.Primitives.ButtonBase]::ClickEvent)) })
                    $script:reopen.Start()
                }
            } else {
                (& $f 'Head').Text = $W.errHead
                (& $f 'Note').Text = $W.errNote + $(if ($S.Message -and $S.Message -notmatch 'message above') { "`r`n" + $S.Message } else { '' })
                (& $f 'Detail').Text = ''
                (& $f 'MainBtn').Content = $W.log; (& $f 'MainBtn').Tag = 'log'
                (& $f 'LogBox').Visibility = 'Visible'; (& $f 'DetailsLink').Text = $W.hide
            }
            (& $f 'SecondBtn').Content = $W.close
            $buttons.Visibility = 'Visible'
        }
        (& $f 'DetailsLink').Add_MouseLeftButtonUp({
            $box = & $f 'LogBox'
            if ($box.Visibility -eq 'Visible') { $box.Visibility = 'Collapsed'; (& $f 'DetailsLink').Text = $W.details }
            else { $box.Visibility = 'Visible'; (& $f 'DetailsLink').Text = $W.hide; $box.ScrollToEnd() }
        })
        (& $f 'TopBar').Add_MouseLeftButtonDown({ try { $win.DragMove() } catch { } })
        (& $f 'MinBtn').Add_Click({ $win.WindowState = 'Minimized' })
        (& $f 'XBtn').Add_Click({ $win.Close() })
        (& $f 'MainBtn').Add_Click({
            $tag = (& $f 'MainBtn').Tag
            if ($tag -eq 'play') { try { Start-Process -FilePath (Join-Path $S.Root 'Play SSBU.bat') -WorkingDirectory $S.Root } catch { } }
            elseif ($tag -eq 'log') { try { Start-Process -FilePath 'notepad.exe' -ArgumentList ('"' + (Join-Path $S.Root 'install.log') + '"') } catch { } ; return }
            $win.Close()
        })
        (& $f 'SecondBtn').Add_Click({ $win.Close() })
        $win.Add_Closing({
            param($s, $e)
            if ($S.State -eq 'run') {
                $answer = [Windows.MessageBox]::Show($W.confirm, $W.confirmTitle, 'YesNo', 'Question')
                if ($answer -ne 'Yes') { $e.Cancel = $true; return }
                $script:closing = $true
                try { Start-Process -FilePath 'taskkill.exe' -ArgumentList @('/PID', "$($S.MainPid)", '/T', '/F') -WindowStyle Hidden } catch { }
            }
            $script:closing = $true
        })
        $win.Add_Closed({ $S.Closed = $true; $win.Dispatcher.InvokeShutdown() })
        $timer = New-Object Windows.Threading.DispatcherTimer
        $timer.Interval = [TimeSpan]::FromMilliseconds(60)
        $script:lastState = 'run'
        $timer.Add_Tick({
            try {
                & $render
                if ($S.State -ne 'run' -and $script:lastState -eq 'run') { $script:lastState = $S.State; & $finish }
            } catch { $S.TickError = "$($_.Exception.Message) @ $($_.InvocationInfo.PositionMessage)" }
        })
        $timer.Start()
        if ($S.Visible) { $win.Show() }
        [Windows.Threading.Dispatcher]::Run()
    } catch {
        $S.Failed = "$($_.Exception.Message)"
        $S.Closed = $true
    }
}

function Start-ProgressWindow {
    param([string]$Lang, [string]$Root, [int]$MainPid)
    try {
        $log = New-Object 'System.Collections.Concurrent.ConcurrentQueue[string]'
        $sync = [hashtable]::Synchronized(@{ Lang = $Lang; Root = $Root; MainPid = $MainPid; Step = 0; Percent = 0.0; Detail = ''; Eta = ''; State = 'run'; Message = ''
                                             Visible = $true; Closed = $false; Failed = $null; TickError = $null; Snapshot = $null; ShadersLater = $false; Log = $log
                                             Update = ($env:NATIVESMASH_UPDATE -eq '1') })
        $rs = [runspacefactory]::CreateRunspace()
        $rs.ApartmentState = 'STA'; $rs.ThreadOptions = 'ReuseThread'; $rs.Open()
        $ps = [powershell]::Create(); $ps.Runspace = $rs
        $block = "function Get-ProgressWords {`n" + (Get-Command Get-ProgressWords).Definition + "`n}`n`$Script:ProgressPhases = " + '@(@(1, 2, 3), @(4, 5, 6), @(7, 8), @(9), @(10))' + "`n& {" + $Script:ProgressUi.ToString() + "`n} `$args[0]"
        [void]$ps.AddScript($block).AddArgument($sync)
        $null = $ps.BeginInvoke()
        $Script:ProgressRunspace = @($ps, $rs)
        return $sync
    } catch { return $null }
}

# ---- feeding it (from the installer's thread) -----------------------------------------------------------------------------
function Set-ProgressPercent {
    param($S, [double]$Within)
    <# The overall share: the steps before this one, plus this step's weight at `Within` (0 to 1); never backwards. #>
    $step = [int]$S.Step
    $before = 0
    for ($k = 1; $k -lt $step; $k++) { $before += $Script:ProgressWeights[$k] }
    $w = if ($step -ge 1) { $Script:ProgressWeights[$step] } else { 0 }
    $value = $before + $w * [math]::Min(1.0, [math]::Max(0.0, $Within))
    if ($value -gt $S.Percent) { $S.Percent = $value }
}

function Set-ProgressStep {
    param($S, [int]$Step)
    if (-not $S) { return }
    $S.Step = $Step; $S.Detail = ''; $S.Eta = ''
    Set-ProgressPercent $S 0
}

function Add-ProgressLine {
    param($S, [string]$Line)
    if (-not $S) { return }
    $text = $Line.TrimEnd()
    if ($text) { $S.Log.Enqueue($text) }
    $parsed = ConvertFrom-ToolLine $text
    switch ($parsed.Kind) {
        'bar' {
            if ([int]$S.Step -eq 9) {
                Set-ProgressPercent $S (0.04 + 0.96 * $parsed.Percent / 100)
                $left = Format-ProgressLeft $parsed.Left $S.Lang
                $S.Eta = if ($left) { $left } else { '' }
                if ($parsed.Label) { $S.Detail = Format-ProgressLabel $parsed.Label $S.Lang }
            }
        }
        'download' { if ([int]$S.Step -eq 4) { Set-ProgressPercent $S ($parsed.Percent / 100) } }
        'substep' { if ([int]$S.Step -eq 9) { Set-ProgressPercent $S (0.005 * $parsed.Index) } }
    }
}

function Set-ProgressVisible {
    param($S, [bool]$Visible)
    if ($S) { $S.Visible = $Visible }
}

function Close-Progress {
    <# The end: shows the result and waits until the player closes the window. Returns whether the window could say it. #>
    param($S, [switch]$Done, [string]$Error, [switch]$ShadersLater, [switch]$Silent)
    if (-not $S) { return $false }
    if ($Silent) { $S.State = 'ok'; $S.Visible = $false; try { $Script:ProgressRunspace[0].Stop() } catch { }; return $true }
    $S.Message = $Error; $S.ShadersLater = [bool]$ShadersLater
    $S.State = if ($Done) { 'ok' } else { 'error' }
    $S.Visible = $true
    while (-not $S.Closed) { Start-Sleep -Milliseconds 250 }
    return (-not $S.Failed)   # (true when the window was there to tell it)
}

function Keep-PcAwake {
    <# The build takes hours: Windows must not put the PC to sleep meanwhile (it lets go when this process ends). #>
    try {
        Add-Type -Namespace NativeSmash -Name Power -MemberDefinition '[System.Runtime.InteropServices.DllImport("kernel32.dll")] public static extern uint SetThreadExecutionState(uint flags);' -ErrorAction Stop
        [void][NativeSmash.Power]::SetThreadExecutionState(0x80000001)   # ES_CONTINUOUS | ES_SYSTEM_REQUIRED
    } catch { }
}
