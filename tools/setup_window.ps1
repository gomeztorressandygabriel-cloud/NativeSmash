# NativeSmash's setup window (WPF): where the player's game files, keys and firmware are, with what was found in
# each, the versions that work best, the online name and the options. Dot-sourced by tools\install_windowsmash.ps1,
# which has the Test-KeysFolder, Test-FirmwareFolder and Test-GameFolder it calls.
#
#   Show-WizardWpf -Detected @{Game; Keys; Firmware} -DefaultName 'Player1234'
#       returns @{Game; Keys; Firmware; OnlineName; Shortcut; Firewall; Shaders}, or $null when cancelled.
#   $Script:WizardSnapshot = <png path>: draws the window into that file, then accepts it (for checking the look).

# the game versions that have been tried: the newest, and the others that work
$Script:TestedVersions = [ordered]@{ '13.0.5' = 'best'; '13.0.3' = 'ok'; '11.0.0' = 'ok' }

function Get-SetupWords {
    param([string]$Lang)
    $w = @{
        es = @{
            title = 'Tus archivos del juego'; sub = 'NativeSmash se construye en tu PC con tu propia copia del juego. Lo que se encontró en este PC ya está rellenado; cambia lo que quieras.'
            game = 'El juego'; gameHint = 'El volcado de tu cartucho (.xci), un .nsp o los .nca; con su parche (update) y los DLC si los tienes.'
            keys = 'Claves'; keysHint = 'La carpeta con prod.keys y title.keys de tu Switch. Deben ser de un firmware igual o más nuevo que tu juego.'
            firmware = 'Firmware'; firmwareHint = 'La carpeta con los .nca del firmware de tu Switch (solo se usan las fuentes). Sirve cualquiera reciente.'
            browse = 'Examinar…'; checking = 'Comprobando…'
            name = 'Tu nombre en línea'; nameHint = 'Lo que ven los demás jugadores.'
            shortcut = 'Poner un acceso directo en el escritorio'
            firewall = 'Permitir el juego en el Firewall de Windows (pide permiso de administrador una vez; hace falta para jugar en línea)'
            shaders = 'Al terminar, preparar los shaders en segundo plano (de minutos a dos horas, con prioridad baja; evita los tirones)'
            note = 'Después se construye el juego: de 30 minutos a unas horas. Si se interrumpe, vuelve a abrir el instalador y sigue donde se quedó.'
            check = 'Comprobar archivos'; go = 'Instalar y compilar'; cancel = 'Cancelar'
            recTitle = 'Versiones recomendadas'; recBest = 'Recomendada: la última y la más probada.'; recOk = 'Probada.'
            recOther = 'Otras versiones pueden funcionar, pero sin garantía.'; recOnline = 'Para jugar en línea, todos deben tener la misma versión del juego.'
            needTitle = 'Qué necesitas'
            need1 = 'Una copia de Super Smash Bros. Ultimate de tu propia Switch.'; need2 = 'Sus claves y su firmware, de la misma consola.'; need3 = 'Unos 20 GB libres para construirlo.'
            legal = 'NativeSmash no incluye el juego, ni claves, ni firmware.'
            detBest = 'Versión detectada: {0}. Es la recomendada.'; detOk = 'Versión detectada: {0}. Está entre las probadas.'
            detOther = 'Versión detectada: {0}. No está entre las probadas: puede fallar.'
            fix = 'Corrige esto antes de seguir (mira la línea bajo cada carpeta): {0}'; pick = 'Elige la carpeta'
            gameN = 'el juego'; keysN = 'las claves'; firmwareN = 'el firmware'
        }
        en = @{
            title = 'Your game files'; sub = 'NativeSmash is built on your PC from your own copy of the game. What was found on this PC is already filled in; change anything you like.'
            game = 'The game'; gameHint = 'Your game card dump (.xci), an .nsp or the .nca files; with its update and DLC if you have them.'
            keys = 'Keys'; keysHint = 'The folder with your Switch''s prod.keys and title.keys. They must be from a firmware equal to or newer than your game.'
            firmware = 'Firmware'; firmwareHint = 'The folder with the .nca files of your Switch''s firmware (only its fonts are used). Any recent one works.'
            browse = 'Browse…'; checking = 'Checking…'
            name = 'Your online name'; nameHint = 'What other players see.'
            shortcut = 'Put a shortcut on the desktop'
            firewall = 'Allow the game through Windows Firewall (asks for administrator rights once; needed to play online)'
            shaders = 'After the build, prepare the shaders in the background (minutes to two hours, low priority; stops the stutter)'
            note = 'Then the game is built, which takes 30 minutes to a few hours. If it stops, open the installer again: it carries on where it stopped.'
            check = 'Check files'; go = 'Install and build'; cancel = 'Cancel'
            recTitle = 'Recommended versions'; recBest = 'Recommended: the latest and the best tested.'; recOk = 'Tested.'
            recOther = 'Other versions may work, with no guarantee.'; recOnline = 'To play online, everyone needs the same game version.'
            needTitle = 'What you need'
            need1 = 'A copy of Super Smash Bros. Ultimate from your own Switch.'; need2 = 'Its keys and firmware, from the same console.'; need3 = 'About 20 GB free to build it.'
            legal = 'NativeSmash does not include the game, keys or firmware.'
            detBest = 'Version found: {0}. It is the recommended one.'; detOk = 'Version found: {0}. It is among the tested ones.'
            detOther = 'Version found: {0}. It is not among the tested ones: it may fail.'
            fix = 'Fix this before going on (see the line under each folder): {0}'; pick = 'Choose the folder'
            gameN = 'the game'; keysN = 'the keys'; firmwareN = 'the firmware'
        }
    }
    return $w[$Lang]
}

$Script:SetupXaml = @'
<Window xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
        xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
        Title="NativeSmash" Width="1180" Height="780" WindowStyle="None" AllowsTransparency="True"
        Background="Transparent" ResizeMode="NoResize" WindowStartupLocation="CenterScreen"
        FontFamily="Segoe UI" TextOptions.TextFormattingMode="Display" UseLayoutRounding="True">
  <Window.Resources>
@@THEME@@
    <Style x:Key="Card" TargetType="Border"><Setter Property="Background" Value="{StaticResource Veil}"/><Setter Property="BorderBrush" Value="{StaticResource Edge}"/>
      <Setter Property="BorderThickness" Value="1"/><Setter Property="CornerRadius" Value="18"/><Setter Property="Padding" Value="20,16"/><Setter Property="Margin" Value="0,0,0,14"/></Style>
    <Style x:Key="Strong" TargetType="TextBlock"><Setter Property="Foreground" Value="{StaticResource Ink}"/><Setter Property="FontSize" Value="16"/><Setter Property="FontWeight" Value="SemiBold"/></Style>
  </Window.Resources>
  <Border x:Name="Shell" CornerRadius="26" BorderBrush="#33FFFFFF" BorderThickness="1" Margin="22">
    <Border.Background><LinearGradientBrush StartPoint="0,0" EndPoint="1,1">
      <GradientStop Color="#FF141935" Offset="0"/><GradientStop Color="#FF0E1124" Offset="0.55"/><GradientStop Color="#FF080A15" Offset="1"/></LinearGradientBrush></Border.Background>
    <Border.Effect><DropShadowEffect BlurRadius="40" ShadowDepth="0" Opacity="0.65"/></Border.Effect>
    <Grid>
      <Ellipse Width="560" Height="560" HorizontalAlignment="Right" VerticalAlignment="Top" Margin="0,-190,-130,0" IsHitTestVisible="False" Opacity="0.5">
        <Ellipse.Fill><RadialGradientBrush><GradientStop Color="#886D7DFF" Offset="0"/><GradientStop Color="#00A66BFF" Offset="1"/></RadialGradientBrush></Ellipse.Fill></Ellipse>
      <Grid Margin="34,26,34,26">
        <Grid.RowDefinitions><RowDefinition Height="Auto"/><RowDefinition Height="*"/><RowDefinition Height="Auto"/></Grid.RowDefinitions>
        <Grid Grid.Row="0" x:Name="Head">
          <StackPanel Orientation="Horizontal">
            <Border Width="38" Height="38" CornerRadius="11" Background="{StaticResource Accent}"><TextBlock Text="N" Foreground="White" FontSize="22" FontWeight="Bold" HorizontalAlignment="Center" VerticalAlignment="Center"/></Border>
            <TextBlock Text="NativeSmash" Style="{StaticResource Strong}" FontSize="22" Margin="14,0,0,0" VerticalAlignment="Center"/>
          </StackPanel>
          <Button x:Name="CloseBtn" Content="✕" Width="34" Height="34" HorizontalAlignment="Right" Foreground="{StaticResource Muted}" Background="Transparent" BorderThickness="0" FontSize="14" Cursor="Hand"/>
        </Grid>
        <Grid Grid.Row="1" Margin="0,18,0,12">
          <Grid.ColumnDefinitions><ColumnDefinition Width="*"/><ColumnDefinition Width="22"/><ColumnDefinition Width="300"/></Grid.ColumnDefinitions>
          <ScrollViewer Grid.Column="0" VerticalScrollBarVisibility="Auto" Padding="0,0,12,0">
            <StackPanel>
              <TextBlock x:Name="Title" Style="{StaticResource Title}"/>
              <TextBlock x:Name="Sub" Style="{StaticResource Help}" FontSize="14" Margin="0,6,0,18"/>

              <Border Style="{StaticResource Card}"><StackPanel>
                <TextBlock x:Name="GameTitle" Style="{StaticResource Strong}"/><TextBlock x:Name="GameHint" Style="{StaticResource Help}" Margin="0,2,0,10"/>
                <Grid><Grid.ColumnDefinitions><ColumnDefinition Width="*"/><ColumnDefinition Width="Auto"/></Grid.ColumnDefinitions>
                  <TextBox x:Name="GameBox" Style="{StaticResource Field}"/><Button x:Name="GameBrowse" Grid.Column="1" Style="{StaticResource Soft}" Margin="10,0,0,0"/></Grid>
                <StackPanel Orientation="Horizontal" Margin="0,10,0,0"><Ellipse x:Name="GameDot" Width="10" Height="10" Fill="#FF9AA1BC" Margin="0,5,10,0" VerticalAlignment="Top"/><TextBlock x:Name="GameStatus" Style="{StaticResource Help}" Margin="0" MaxWidth="560"/></StackPanel>
                <TextBlock x:Name="GameVersion" Style="{StaticResource Help}" Margin="20,4,0,0" FontWeight="SemiBold"/>
              </StackPanel></Border>

              <Border Style="{StaticResource Card}"><StackPanel>
                <TextBlock x:Name="KeysTitle" Style="{StaticResource Strong}"/><TextBlock x:Name="KeysHint" Style="{StaticResource Help}" Margin="0,2,0,10"/>
                <Grid><Grid.ColumnDefinitions><ColumnDefinition Width="*"/><ColumnDefinition Width="Auto"/></Grid.ColumnDefinitions>
                  <TextBox x:Name="KeysBox" Style="{StaticResource Field}"/><Button x:Name="KeysBrowse" Grid.Column="1" Style="{StaticResource Soft}" Margin="10,0,0,0"/></Grid>
                <StackPanel Orientation="Horizontal" Margin="0,10,0,0"><Ellipse x:Name="KeysDot" Width="10" Height="10" Fill="#FF9AA1BC" Margin="0,5,10,0" VerticalAlignment="Top"/><TextBlock x:Name="KeysStatus" Style="{StaticResource Help}" Margin="0" MaxWidth="560"/></StackPanel>
              </StackPanel></Border>

              <Border Style="{StaticResource Card}"><StackPanel>
                <TextBlock x:Name="FirmwareTitle" Style="{StaticResource Strong}"/><TextBlock x:Name="FirmwareHint" Style="{StaticResource Help}" Margin="0,2,0,10"/>
                <Grid><Grid.ColumnDefinitions><ColumnDefinition Width="*"/><ColumnDefinition Width="Auto"/></Grid.ColumnDefinitions>
                  <TextBox x:Name="FirmwareBox" Style="{StaticResource Field}"/><Button x:Name="FirmwareBrowse" Grid.Column="1" Style="{StaticResource Soft}" Margin="10,0,0,0"/></Grid>
                <StackPanel Orientation="Horizontal" Margin="0,10,0,0"><Ellipse x:Name="FirmwareDot" Width="10" Height="10" Fill="#FF9AA1BC" Margin="0,5,10,0" VerticalAlignment="Top"/><TextBlock x:Name="FirmwareStatus" Style="{StaticResource Help}" Margin="0" MaxWidth="560"/></StackPanel>
              </StackPanel></Border>

              <Border Style="{StaticResource Card}"><StackPanel>
                <TextBlock x:Name="NameTitle" Style="{StaticResource Strong}"/><TextBlock x:Name="NameHint" Style="{StaticResource Help}" Margin="0,2,0,10"/>
                <TextBox x:Name="NameBox" Style="{StaticResource Field}" MaxLength="32" Width="300" HorizontalAlignment="Left"/>
                <CheckBox x:Name="ShortcutBox" Style="{StaticResource Switch}" Margin="0,16,0,0" IsChecked="True"/>
                <CheckBox x:Name="FirewallBox" Style="{StaticResource Switch}" IsChecked="True"/>
                <CheckBox x:Name="ShadersBox" Style="{StaticResource Switch}" IsChecked="True"/>
              </StackPanel></Border>
              <TextBlock x:Name="Note" Style="{StaticResource Help}" MaxWidth="680" HorizontalAlignment="Left"/>
            </StackPanel>
          </ScrollViewer>

          <StackPanel Grid.Column="2">
            <Border Style="{StaticResource Card}"><StackPanel>
              <TextBlock x:Name="RecTitle" Style="{StaticResource Strong}"/>
              <StackPanel x:Name="RecList" Margin="0,12,0,0"/>
              <TextBlock x:Name="RecOther" Style="{StaticResource Help}"/><TextBlock x:Name="RecOnline" Style="{StaticResource Help}"/>
            </StackPanel></Border>
            <Border Style="{StaticResource Card}"><StackPanel>
              <TextBlock x:Name="NeedTitle" Style="{StaticResource Strong}"/>
              <TextBlock x:Name="Need1" Style="{StaticResource Help}" Margin="0,10,0,0"/><TextBlock x:Name="Need2" Style="{StaticResource Help}"/><TextBlock x:Name="Need3" Style="{StaticResource Help}"/>
            </StackPanel></Border>
            <TextBlock x:Name="Legal" Style="{StaticResource Help}" FontSize="12"/>
          </StackPanel>
        </Grid>
        <StackPanel Grid.Row="2" Orientation="Horizontal" HorizontalAlignment="Right">
          <Button x:Name="CheckBtn" Style="{StaticResource Soft}"/><Button x:Name="CancelBtn" Style="{StaticResource Soft}"/>
          <Button x:Name="GoBtn" Style="{StaticResource PlayBtn}" FontSize="16" Margin="6,0,0,10"/>
        </StackPanel>
      </Grid>
    </Grid>
  </Border>
</Window>
'@

# ---- the language, asked before anything else ----------------------------------------------------------------
$Script:LanguageXaml = @'
<Window xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
        xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
        Title="NativeSmash" Width="560" Height="470" WindowStyle="None" AllowsTransparency="True"
        Background="Transparent" ResizeMode="NoResize" WindowStartupLocation="CenterScreen"
        FontFamily="Segoe UI" TextOptions.TextFormattingMode="Display" UseLayoutRounding="True">
  <Border Margin="14" CornerRadius="22" BorderThickness="1" BorderBrush="#26FFFFFF">
    <Border.Background>
      <LinearGradientBrush StartPoint="0,0" EndPoint="1,1">
        <GradientStop Color="#FF171A33" Offset="0"/><GradientStop Color="#FF0B0D1D" Offset="1"/>
      </LinearGradientBrush>
    </Border.Background>
    <Border.Effect><DropShadowEffect BlurRadius="28" ShadowDepth="0" Opacity="0.55"/></Border.Effect>
    <Grid Margin="34,26,34,30">
      <Grid.RowDefinitions><RowDefinition Height="Auto"/><RowDefinition Height="Auto"/><RowDefinition Height="Auto"/><RowDefinition Height="*"/></Grid.RowDefinitions>
      <Image x:Name="Logo" Grid.Row="0" Width="92" Height="92" HorizontalAlignment="Center" Margin="0,4,0,10" RenderOptions.BitmapScalingMode="HighQuality"/>
      <TextBlock Grid.Row="1" Text="NativeSmash" Foreground="#FFF2F4FB" FontSize="28" FontWeight="SemiBold" HorizontalAlignment="Center"/>
      <TextBlock Grid.Row="2" Text="Elige tu idioma  ·  Choose your language" Foreground="#FF9AA1BC" FontSize="15" HorizontalAlignment="Center" Margin="0,8,0,22"/>
      <StackPanel Grid.Row="3" VerticalAlignment="Top">
        <Button x:Name="EsButton" Content="Español" Height="58" Margin="0,0,0,12" FontSize="19" FontWeight="SemiBold" Foreground="White" BorderThickness="0" Cursor="Hand">
          <Button.Template><ControlTemplate TargetType="Button"><Border x:Name="B" CornerRadius="16" Background="{TemplateBinding Background}"><ContentPresenter HorizontalAlignment="Center" VerticalAlignment="Center"/></Border>
            <ControlTemplate.Triggers><Trigger Property="IsMouseOver" Value="True"><Setter TargetName="B" Property="Opacity" Value="0.88"/></Trigger></ControlTemplate.Triggers></ControlTemplate></Button.Template>
          <Button.Background><LinearGradientBrush StartPoint="0,0" EndPoint="1,1"><GradientStop Color="#FF6D7DFF" Offset="0"/><GradientStop Color="#FFA66BFF" Offset="1"/></LinearGradientBrush></Button.Background>
        </Button>
        <Button x:Name="EnButton" Content="English" Height="58"  FontSize="19" FontWeight="SemiBold" Foreground="White" BorderThickness="0" Cursor="Hand">
          <Button.Template><ControlTemplate TargetType="Button"><Border x:Name="B" CornerRadius="16" Background="{TemplateBinding Background}"><ContentPresenter HorizontalAlignment="Center" VerticalAlignment="Center"/></Border>
            <ControlTemplate.Triggers><Trigger Property="IsMouseOver" Value="True"><Setter TargetName="B" Property="Opacity" Value="0.88"/></Trigger></ControlTemplate.Triggers></ControlTemplate></Button.Template>
          <Button.Background><LinearGradientBrush StartPoint="0,0" EndPoint="1,1"><GradientStop Color="#FF6D7DFF" Offset="0"/><GradientStop Color="#FFA66BFF" Offset="1"/></LinearGradientBrush></Button.Background>
        </Button>
      </StackPanel>
    </Grid>
  </Border>
</Window>
'@

function Show-LanguageWindow {
    <# A small window with two big buttons; returns 'es' or 'en', or $null when it is closed with Esc. #>
    param([string]$Snapshot)
    Add-Type -AssemblyName PresentationFramework, PresentationCore, WindowsBase
    $win = [Windows.Markup.XamlReader]::Parse($Script:LanguageXaml)
    $png = Join-Path (Split-Path -Parent $PSScriptRoot) 'docs\assets\nativesmash-512.png'
    if (Test-Path -LiteralPath $png) {
        $bmp = New-Object Windows.Media.Imaging.BitmapImage
        $bmp.BeginInit(); $bmp.UriSource = New-Object Uri($png); $bmp.CacheOption = 'OnLoad'; $bmp.EndInit()
        $win.FindName('Logo').Source = $bmp
        $win.Icon = $bmp
    }
    $Script:LanguagePicked = $null
    $win.FindName('EsButton').Add_Click({ $Script:LanguagePicked = 'es'; $win.Close() })
    $win.FindName('EnButton').Add_Click({ $Script:LanguagePicked = 'en'; $win.Close() })
    $win.Add_KeyDown({ param($s, $e) if ($e.Key -eq 'Escape') { $win.Close() } })
    $win.Add_MouseLeftButtonDown({ param($s, $e) if ($e.OriginalSource -isnot [Windows.Controls.TextBlock] -and $e.OriginalSource -isnot [Windows.Controls.Border]) { return }; try { $win.DragMove() } catch { } })
    if ($Snapshot) {
        $win.Add_ContentRendered({
            $r = New-Object Windows.Media.Imaging.RenderTargetBitmap([int]$win.ActualWidth, [int]$win.ActualHeight, 96, 96, [Windows.Media.PixelFormats]::Pbgra32)
            $r.Render($win)
            $enc = New-Object Windows.Media.Imaging.PngBitmapEncoder; $enc.Frames.Add([Windows.Media.Imaging.BitmapFrame]::Create($r))
            $fs = [IO.File]::Create($Snapshot); $enc.Save($fs); $fs.Close(); $Script:LanguagePicked = 'es'; $win.Close()
        })
    }
    [void]$win.ShowDialog()
    return $Script:LanguagePicked
}

# ---- Microsoft's license terms, asked in a window (the console's "type yes" was easy to miss) -------------------------------
$Script:LicenseXaml = @'
<Window xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
        xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
        Title="NativeSmash" Width="640" Height="520" WindowStyle="None" AllowsTransparency="True"
        Background="Transparent" ResizeMode="NoResize" WindowStartupLocation="CenterScreen"
        FontFamily="Segoe UI" TextOptions.TextFormattingMode="Display" UseLayoutRounding="True">
  <Border Margin="14" CornerRadius="22" BorderThickness="1" BorderBrush="#26FFFFFF">
    <Border.Background>
      <LinearGradientBrush StartPoint="0,0" EndPoint="1,1">
        <GradientStop Color="#FF171A33" Offset="0"/><GradientStop Color="#FF0B0D1D" Offset="1"/>
      </LinearGradientBrush>
    </Border.Background>
    <Border.Effect><DropShadowEffect BlurRadius="28" ShadowDepth="0" Opacity="0.55"/></Border.Effect>
    <StackPanel Margin="34,28,34,30">
      <Image x:Name="Logo" Width="64" Height="64" HorizontalAlignment="Left" Margin="0,0,0,12" RenderOptions.BitmapScalingMode="HighQuality"/>
      <TextBlock x:Name="Head" Foreground="#FFF2F4FB" FontSize="24" FontWeight="SemiBold" TextWrapping="Wrap"/>
      <TextBlock x:Name="Body" Foreground="#FF9AA1BC" FontSize="15" TextWrapping="Wrap" Margin="0,12,0,0" LineHeight="22"/>
      <Button x:Name="ReadBtn" Margin="0,16,0,0" HorizontalAlignment="Left" Cursor="Hand" Background="Transparent" BorderThickness="0" Padding="0">
        <TextBlock x:Name="ReadText" Foreground="#FF8E9BFF" FontSize="15" TextDecorations="Underline"/>
      </Button>
      <TextBlock x:Name="Note" Foreground="#FF9AA1BC" FontSize="13" TextWrapping="Wrap" Margin="0,16,0,22"/>
      <Button x:Name="YesBtn" Content="Acepto" Height="52" Margin="0,0,0,10" FontSize="17" FontWeight="SemiBold" Foreground="White" BorderThickness="0" Cursor="Hand">
          <Button.Template><ControlTemplate TargetType="Button"><Border x:Name="B" CornerRadius="16" Background="{TemplateBinding Background}"><ContentPresenter HorizontalAlignment="Center" VerticalAlignment="Center"/></Border>
            <ControlTemplate.Triggers><Trigger Property="IsMouseOver" Value="True"><Setter TargetName="B" Property="Opacity" Value="0.88"/></Trigger></ControlTemplate.Triggers></ControlTemplate></Button.Template>
          <Button.Background><LinearGradientBrush StartPoint="0,0" EndPoint="1,1"><GradientStop Color="#FF6D7DFF" Offset="0"/><GradientStop Color="#FFA66BFF" Offset="1"/></LinearGradientBrush></Button.Background>
        </Button>
      <Button x:Name="NoBtn" Content="No acepto" Height="52"  FontSize="17" FontWeight="SemiBold" Foreground="White" BorderThickness="0" Cursor="Hand">
          <Button.Template><ControlTemplate TargetType="Button"><Border x:Name="B" CornerRadius="16" Background="{TemplateBinding Background}"><ContentPresenter HorizontalAlignment="Center" VerticalAlignment="Center"/></Border>
            <ControlTemplate.Triggers><Trigger Property="IsMouseOver" Value="True"><Setter TargetName="B" Property="Opacity" Value="0.88"/></Trigger></ControlTemplate.Triggers></ControlTemplate></Button.Template>
          <Button.Background><SolidColorBrush Color="#FF2A2E4D"/></Button.Background>
        </Button>
    </StackPanel>
  </Border>
</Window>
'@

function Show-LicenseWindow {
    <# Microsoft's license terms (the Visual Studio Build Tools', whose compiler tools NativeSmash downloads) asked in a window with
       two buttons and a link to read them. Returns 'yes' or 'no' ('no' also when the window is closed). #>
    param([string]$Url, [string]$Lang = 'es', [string]$Snapshot)
    Add-Type -AssemblyName PresentationFramework, PresentationCore, WindowsBase
    $win = [Windows.Markup.XamlReader]::Parse($Script:LicenseXaml)
    $es = $Lang -ne 'en'
    $win.FindName('Head').Text = if ($es) { 'Una cosa más antes de construir el juego' } else { 'One more thing before the game is built' }
    $win.FindName('Body').Text = if ($es) {
        'Para construir el juego en tu PC hace falta el compilador y las herramientas de Windows de Microsoft (unos 570 MB). NativeSmash no las incluye: las descarga de los servidores de Microsoft, y solo si aceptas sus términos de licencia (los de las Visual Studio Build Tools).'
    } else {
        'Building the game on your PC needs Microsoft''s compiler and Windows tools (about 570 MB). NativeSmash does not include them: it downloads them from Microsoft''s servers, and only if you accept their license terms (those of the Visual Studio Build Tools).'
    }
    $win.FindName('ReadText').Text = if ($es) { 'Leer los términos de licencia de Microsoft' } else { 'Read Microsoft''s license terms' }
    $win.FindName('Note').Text = if ($es) { 'Si no los aceptas, el instalador se detiene y no se descarga nada. Puedes volver a abrirlo cuando quieras.' } else { 'If you do not accept them, the installer stops and nothing is downloaded. You can open it again whenever you like.' }
    $win.FindName('YesBtn').Content = if ($es) { 'Acepto y continuar' } else { 'I accept and continue' }
    $win.FindName('NoBtn').Content = if ($es) { 'No acepto' } else { 'I do not accept' }
    $png = Join-Path (Split-Path -Parent $PSScriptRoot) 'docs\assets\nativesmash-512.png'
    if (Test-Path -LiteralPath $png) {
        $bmp = New-Object Windows.Media.Imaging.BitmapImage
        $bmp.BeginInit(); $bmp.UriSource = New-Object Uri($png); $bmp.CacheOption = 'OnLoad'; $bmp.EndInit()
        $win.FindName('Logo').Source = $bmp
        $win.Icon = $bmp
    }
    $Script:LicenseAnswer = 'no'
    $win.FindName('ReadBtn').Add_Click({ if ($Url) { try { Start-Process $Url } catch { } } })
    $win.FindName('YesBtn').Add_Click({ $Script:LicenseAnswer = 'yes'; $win.Close() })
    $win.FindName('NoBtn').Add_Click({ $Script:LicenseAnswer = 'no'; $win.Close() })
    $win.Add_KeyDown({ param($s, $e) if ($e.Key -eq 'Escape') { $win.Close() } })
    if ($Snapshot) {
        $win.Add_ContentRendered({
            $r = New-Object Windows.Media.Imaging.RenderTargetBitmap([int]$win.ActualWidth, [int]$win.ActualHeight, 96, 96, [Windows.Media.PixelFormats]::Pbgra32)
            $r.Render($win)
            $enc = New-Object Windows.Media.Imaging.PngBitmapEncoder; $enc.Frames.Add([Windows.Media.Imaging.BitmapFrame]::Create($r))
            $fs = [IO.File]::Create($Snapshot); $enc.Save($fs); $fs.Close(); $win.Close()
        })
    }
    [void]$win.ShowDialog()
    return $Script:LicenseAnswer
}

function Show-WizardWpf {
    param([hashtable]$Detected, [string]$DefaultName)
    Add-Type -AssemblyName PresentationFramework, PresentationCore, WindowsBase
    Add-Type -AssemblyName System.Windows.Forms
    $lang = if ($Script:Language) { $Script:Language } elseif ((Get-Culture).TwoLetterISOLanguageName -eq 'es') { 'es' } else { 'en' }
    $W = Get-SetupWords $lang
    $themeFile = Join-Path $PSScriptRoot 'ui_theme.xaml'
    $theme = [IO.File]::ReadAllText($themeFile)
    $win = [Windows.Markup.XamlReader]::Parse($Script:SetupXaml.Replace('@@THEME@@', $theme))
    # the window's own icon (taskbar and Alt+Tab)
    try {
        $iconPng = Join-Path (Split-Path -Parent $PSScriptRoot) 'docs\assets\nativesmash-512.png'
        if (Test-Path -LiteralPath $iconPng) {
            $iconBmp = New-Object Windows.Media.Imaging.BitmapImage
            $iconBmp.BeginInit(); $iconBmp.UriSource = New-Object Uri($iconPng); $iconBmp.CacheOption = 'OnLoad'; $iconBmp.EndInit()
            $win.Icon = $iconBmp
        }
    } catch { }
    $f = { param($n) $win.FindName($n) }

    foreach ($p in @(@('Title', 'title'), @('Sub', 'sub'), @('GameTitle', 'game'), @('GameHint', 'gameHint'), @('KeysTitle', 'keys'), @('KeysHint', 'keysHint'),
                     @('FirmwareTitle', 'firmware'), @('FirmwareHint', 'firmwareHint'), @('NameTitle', 'name'), @('NameHint', 'nameHint'),
                     @('Note', 'note'), @('RecTitle', 'recTitle'), @('RecOther', 'recOther'), @('RecOnline', 'recOnline'), @('NeedTitle', 'needTitle'),
                     @('Need1', 'need1'), @('Need2', 'need2'), @('Need3', 'need3'), @('Legal', 'legal'))) { (& $f $p[0]).Text = $W[$p[1]] }
    foreach ($p in @(@('GameBrowse', 'browse'), @('KeysBrowse', 'browse'), @('FirmwareBrowse', 'browse'), @('ShortcutBox', 'shortcut'), @('FirewallBox', 'firewall'),
                     @('ShadersBox', 'shaders'), @('CheckBtn', 'check'), @('CancelBtn', 'cancel'), @('GoBtn', 'go'))) { (& $f $p[0]).Content = $W[$p[1]] }
    (& $f 'GameBox').Text = [string]$Detected.Game; (& $f 'KeysBox').Text = [string]$Detected.Keys; (& $f 'FirmwareBox').Text = [string]$Detected.Firmware
    (& $f 'NameBox').Text = $DefaultName

    # the recommended versions, one line each
    $recList = & $f 'RecList'
    foreach ($v in $Script:TestedVersions.Keys) {
        $best = $Script:TestedVersions[$v] -eq 'best'
        $row = New-Object Windows.Controls.StackPanel; $row.Margin = '0,0,0,10'
        $head = New-Object Windows.Controls.StackPanel; $head.Orientation = 'Horizontal'
        $tag = New-Object Windows.Controls.TextBlock; $tag.Text = $(if ($best) { '★ ' } else { '✓ ' }); $tag.FontSize = 15
        $tag.Foreground = $(if ($best) { [Windows.Media.Brushes]::Gold } else { [Windows.Media.Brushes]::MediumAquamarine })
        $ver = New-Object Windows.Controls.TextBlock; $ver.Text = $v; $ver.FontSize = 15; $ver.FontWeight = 'SemiBold'; $ver.Foreground = $win.FindResource('Ink')
        [void]$head.Children.Add($tag); [void]$head.Children.Add($ver)
        $txt = New-Object Windows.Controls.TextBlock; $txt.Text = $(if ($best) { $W.recBest } else { $W.recOk }); $txt.FontSize = 12.5; $txt.Foreground = $win.FindResource('Muted'); $txt.TextWrapping = 'Wrap'
        [void]$row.Children.Add($head); [void]$row.Children.Add($txt); [void]$recList.Children.Add($row)
    }

    $green = [Windows.Media.BrushConverter]::new().ConvertFromString('#FF62E3A4')
    $red = [Windows.Media.BrushConverter]::new().ConvertFromString('#FFFF7A85')
    $amber = [Windows.Media.BrushConverter]::new().ConvertFromString('#FFFFC56B')
    $grey = [Windows.Media.BrushConverter]::new().ConvertFromString('#FF9AA1BC')
    $results = @{}
    $clean = { param($t) $t.Trim().Trim('"') }

    $refresh = {
        foreach ($k in 'Game', 'Keys', 'Firmware') { (& $f "${k}Status").Text = $W.checking; (& $f "${k}Dot").Fill = $grey }
        (& $f 'GameVersion').Text = ''
        $win.Dispatcher.Invoke([Action]{}, [Windows.Threading.DispatcherPriority]::Render)
        $results['Keys'] = Test-KeysFolder (& $clean (& $f 'KeysBox').Text)
        $results['Firmware'] = Test-FirmwareFolder (& $clean (& $f 'FirmwareBox').Text)
        $results['Game'] = Test-GameFolder (& $clean (& $f 'GameBox').Text) (& $clean (& $f 'KeysBox').Text)
        foreach ($k in 'Game', 'Keys', 'Firmware') {
            (& $f "${k}Status").Text = $results[$k].Text
            (& $f "${k}Dot").Fill = $(if ($results[$k].Ok) { $green } else { $red })
        }
        if ($results['Game'].Ok -and $results['Game'].Text -match '\b(\d+\.\d+\.\d+)\b') {
            $v = $Matches[1]; $vt = & $f 'GameVersion'
            if ($Script:TestedVersions.Contains($v)) {
                $vt.Text = ($(if ($Script:TestedVersions[$v] -eq 'best') { $W.detBest } else { $W.detOk }) -f $v); $vt.Foreground = $green
            } else { $vt.Text = ($W.detOther -f $v); $vt.Foreground = $amber }
        }
    }

    foreach ($k in 'Game', 'Keys', 'Firmware') {
        $box = & $f "${k}Box"
        (& $f "${k}Browse").Add_Click({
            $dlg = New-Object Windows.Forms.FolderBrowserDialog
            $dlg.Description = $W.pick
            if ($box.Text -and (Test-Path -LiteralPath $box.Text)) { $dlg.SelectedPath = $box.Text }
            if ($dlg.ShowDialog() -eq [Windows.Forms.DialogResult]::OK) { $box.Text = $dlg.SelectedPath; & $refresh }
        }.GetNewClosure())
    }
    (& $f 'CheckBtn').Add_Click({ & $refresh })
    (& $f 'CancelBtn').Add_Click({ $win.DialogResult = $false; $win.Close() })
    (& $f 'CloseBtn').Add_Click({ $win.DialogResult = $false; $win.Close() })
    (& $f 'Head').Add_MouseLeftButtonDown({ try { $win.DragMove() } catch { } })
    (& $f 'GoBtn').Add_Click({
        & $refresh
        $bad = @(); foreach ($k in 'Game', 'Keys', 'Firmware') { if (-not $results[$k].Ok) { $bad += $W["${k}N".ToLower()] } }
        if ($bad.Count -gt 0) { [void][Windows.MessageBox]::Show($win, ($W.fix -f ($bad -join ', ')), 'NativeSmash', 'OK', 'Warning'); return }
        if (-not (& $f 'NameBox').Text.Trim()) { (& $f 'NameBox').Text = $DefaultName }
        $win.DialogResult = $true; $win.Close()
    })
    $win.Add_ContentRendered({ & $refresh })

    if ($Script:WizardSnapshot) {   # a picture of the window, then accept it
        $script:snapTimer = New-Object Windows.Threading.DispatcherTimer
        $script:snapTimer.Interval = [TimeSpan]::FromMilliseconds(1200)
        $script:snapTimer.Add_Tick({
            $script:snapTimer.Stop()
            $content = $win.Content
            $content.Measure([Windows.Size]::new($win.Width, $win.Height)); $content.Arrange([Windows.Rect]::new(0, 0, $win.Width, $win.Height)); $content.UpdateLayout()
            $rtb = New-Object Windows.Media.Imaging.RenderTargetBitmap([int]$win.Width, [int]$win.Height, 96, 96, [Windows.Media.PixelFormats]::Pbgra32)
            $rtb.Render($content)
            $enc = New-Object Windows.Media.Imaging.PngBitmapEncoder; $enc.Frames.Add([Windows.Media.Imaging.BitmapFrame]::Create($rtb))
            $fs = [IO.File]::Create($Script:WizardSnapshot); $enc.Save($fs); $fs.Close()
            $win.DialogResult = $true; $win.Close()
        })
        $win.Add_ContentRendered({ $script:snapTimer.Start() })
    }

    if (-not $win.ShowDialog()) { return $null }
    return @{
        Game = (& $clean (& $f 'GameBox').Text); Keys = (& $clean (& $f 'KeysBox').Text); Firmware = (& $clean (& $f 'FirmwareBox').Text)
        OnlineName = (& $f 'NameBox').Text.Trim(); Shortcut = [bool](& $f 'ShortcutBox').IsChecked; Firewall = [bool](& $f 'FirewallBox').IsChecked
        Shaders = [bool](& $f 'ShadersBox').IsChecked
    }
}
