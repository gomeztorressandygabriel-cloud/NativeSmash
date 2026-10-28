<#
.SYNOPSIS
NativeSmash's launcher: one window to find the game, set it up, and play. When Play is pressed the window
starts the game, waits for the game's window and fades away to leave it in its place.

.DESCRIPTION
Pages: Play, Online, Controllers, Video, Shaders, Game (versions, setup, tools).
It edits the same files the game's settings menu (Esc) edits, in place, with the writer of tools\launcher.ps1
(which this script loads without opening that window): display.ini, controllers.ini, online_server.txt,
online_name.txt, online_direct.txt, launcher.ini, last_version.txt. Only what changed is written, when Play
is pressed. Close starts nothing and saves nothing.

Windows PowerShell 5.1 and WPF (both are part of Windows 10/11): nothing to install or compile.

.PARAMETER Auto
What Play SSBU.bat passes: start the game straight away when launcher.ini says show_launcher = 0 and
Shift is not held; otherwise open the window.
.PARAMETER DryRun
Say what Play would change and start, and change or start nothing.
.PARAMETER ClickPlay
Press Play by itself a moment after the window opens (to test the start and the fade without a mouse).
.PARAMETER Shot
Draw the window into this PNG file and exit (to check the look without opening it). -Page picks the page.
.PARAMETER Page
play, online, pads, controllers (the pads page's Controllers tab), video, shaders or game (with -Shot, or to open on that page).
.PARAMETER Lang
es or en (default: the PC's language).
.PARAMETER Root
The port's folder (default: this file's folder's parent).
#>
param([switch]$Auto, [switch]$DryRun, [switch]$ClickPlay, [string]$Shot, [string]$Page = 'play', [string]$Lang, [string]$Root)

if (-not $Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path

# The settings writer, the version list and the game's command line are the upstream launcher's.
. (Join-Path $PSScriptRoot 'launcher.ps1') -NoWindow -Root $Root -DryRun:$DryRun   # (its param block would reset $DryRun otherwise)

try {
    Add-Type -AssemblyName PresentationFramework, PresentationCore, WindowsBase
} catch {
    Write-Host "The launcher window cannot open here ($($_.Exception.Message))."
    exit 2
}

# ---- words -------------------------------------------------------------------------------------
if (-not $Lang) {   # what the player chose in the installer, else the PC's language
    $langFile = Join-Path (Split-Path -Parent $PSScriptRoot) 'language.txt'
    if (Test-Path -LiteralPath $langFile) { $Lang = ([string](Get-Content -LiteralPath $langFile -TotalCount 1)).Trim().ToLower() }
    if ($Lang -notin @('es', 'en')) { $Lang = $null }
}
if (-not $Lang) { $Lang = if ((Get-Culture).TwoLetterISOLanguageName -eq 'es') { 'es' } else { 'en' } }
$Words = @{
    play = @('Jugar', 'Play'); online = @('En línea', 'Online'); pads = @('Controles', 'Controllers')
    video = @('Video', 'Video'); shaders = @('Shaders', 'Shaders'); game = @('Juego', 'Game')
    tagline = @('Super Smash Bros. Ultimate · PC nativo', 'Super Smash Bros. Ultimate · native PC')
    ready = @('Todo listo para jugar', 'Ready to play'); notready = @('Falta instalar el juego', 'The game is not installed yet')
    readyHelp = @('Elige cómo quieres jugar a la izquierda y pulsa Jugar.', 'Set things up on the left and press Play.')
    notreadyHelp = @('NativeSmash no incluye el juego: se prepara con tu propia copia. Pulsa Buscar e instalar y elige tus archivos.', 'NativeSmash does not include the game: it is set up from your own copy. Press Find and install and pick your files.')
    version = @('Versión del juego', 'Game version'); playBtn = @('JUGAR', 'PLAY'); starting = @('Iniciando el juego…', 'Starting the game…')
    direct = @('Iniciar directamente la próxima vez (Shift al abrir muestra esta ventana)', 'Start the game directly next time (hold Shift to see this window)')
    install = @('Buscar e instalar el juego', 'Find and install the game')
    onlineTitle = @('Juego en línea', 'Online play'); name = @('Tu nombre', 'Your name'); lobby = @('Sala', 'Lobby')
    lobPublic = @('Pública', 'Public'); lobPrivate = @('Privada (código)', 'Private (code)'); lobCustom = @('Tu servidor', 'Your server'); lobOff = @('Sin conexión', 'Offline')
    code = @('Código de la sala', 'Lobby code'); server = @('Servidor (host:puerto)', 'Server (host:port)')
    directConn = @('Conexiones directas', 'Direct connections')
    report = @('Enviar informes de errores automáticamente', 'Send error reports automatically')
    reportHelp = @('Si el juego se cierra por un error o falla el online, al cerrarse se envía un informe al proyecto para arreglarlo. Sin datos personales: se quitan tu nombre de usuario, el de tu PC, tu nombre online, tu código de sala y tus direcciones de internet. Va a un repositorio privado del proyecto.', 'If the game stops on an error or the online fails, a report goes to the project when it closes, so it can be fixed. No personal data: your user name, your PC''s name, your online name, your room code and your internet addresses are taken out. It goes to a private repository of the project.')
    netcodeLabel = @('Modo', 'Mode')
    netcodeNormal = @('Normal', 'Normal')
    netcodeRollback = @('Rollback (en pruebas)', 'Rollback (testing)')
    netcodeHelp = @('Rollback: el juego adivina los botones del otro en vez de esperarlos y, si se equivoca, corrige al momento; se nota mucho menos el retraso. Normal: espera siempre los botones del otro. Todos los de la partida tienen que elegir el mismo.', 'Rollback: the game guesses the other player''s buttons instead of waiting for them and corrects at once when wrong; the delay shows much less. Normal: it always waits for the other player''s buttons. Everyone in the match must pick the same.')
    delayLabel = @('Retraso de entrada en línea, en cuadros (menos es más rápido; si es muy poco, se traba)', 'Online input delay, in frames (lower is quicker; too low and it stutters)')
    delayAuto = @('Auto (el del juego)', 'Auto (the game''s)')
    delayHelp = @('Cuántos cuadros guarda el juego antes de aplicar tu botón, para esperar al otro jugador. Menos es más rápido, pero si es muy poco para la conexión el juego se traba esperando. Con una buena conexión directa, prueba 2 o 3.', 'How many frames the game holds your button before applying it, to wait for the other player. Lower is quicker, but if it is too low for the connection the game stutters waiting. On a good direct connection, try 2 or 3.')
    vsTitle = @('Si preferís pasaros un código: uno crea la sala y le pasa el código al otro, que se une con él.', 'If you would rather share a code: one makes the room and gives the code to the other, who joins with it.')
    lwTitle = @('Local Wireless', 'Local Wireless')
    lwHelp = @('El modo en línea del propio juego: entra en Smash, Local Wireless, y crea o busca una sala con tus amigos.', 'The game''s own online mode: go to Smash, Local Wireless, and make or find a room with your friends.')
    vsCardTitle = @('VS en red', 'VS netplay')
    vsCardHelp = @('Combates de Smash entre PCs con rollback: elige el modo y el retraso, y busca partida o invita a un jugador conectado.', 'Smash battles between PCs with rollback: pick the mode and the delay, then find a match or invite a player who is online.')
    onlineSoon = @('Jugadores en línea: pronto', 'Players online: soon')
    onlineCount = @('{0} en línea', '{0} online')
    quick = @('Partida rápida', 'Quick match')
    quickHelp = @('Te empareja con otro jugador que también esté buscando, con una conexión parecida a la tuya.', 'Matches you with another player who is looking too, with a connection like yours.')
    quickSoon = @('La partida rápida, la lista de jugadores y las invitaciones llegan con el servidor de jugadores de NativeSmash, que estamos preparando. Mientras tanto, usa "Conectar con un código".', 'Quick match, the players list and invitations come with NativeSmash''s players server, which is on its way. Meanwhile, use "Connect with a code".')
    invitesLabel = @('Invitaciones', 'Invitations')
    inviteFrom = @('{0} te invita a jugar', '{0} invites you to play')
    accept = @('Aceptar', 'Accept'); decline = @('Rechazar', 'Decline'); invite = @('Invitar', 'Invite')
    playersLabel = @('Jugadores conectados', 'Players online')
    playersEmpty = @('Aquí verás a los jugadores que tengan el launcher o el juego abierto, para invitarles a jugar. Si aceptan, entráis los dos en la misma sala sin pasaros ningún código.', 'Players with the launcher or the game open show here, to invite them to play. When they accept, you both go into the same room without sharing a code.')
    stLauncher = @('En el launcher', 'In the launcher'); stPlaying = @('Jugando', 'Playing'); stSearching = @('Buscando partida', 'Looking for a match')
    accTitle = @('Inicia sesión para jugar en línea', 'Sign in to play online')
    accHelp = @('Con tu cuenta de NativeSmash ves quién está conectado, invitas o recibes invitaciones y buscas partida rápida. ¿No tienes? Créala en la web: es gratis.', 'With your NativeSmash account you see who is online, invite or get invited and find quick matches. None yet? Make one on the web page: it is free.')
    emailLabel = @('Correo', 'Email'); passLabel = @('Contraseña', 'Password')
    signIn = @('Entrar', 'Sign in'); signUp = @('Crear cuenta', 'Make an account'); signOut = @('Cerrar sesión', 'Sign out')
    signedAs = @('Conectado como {0}', 'Signed in as {0}')
    signingIn = @('Entrando…', 'Signing in…'); needBoth = @('Escribe tu correo y tu contraseña.', 'Type your email and your password.')
    quickCancel = @('Cancelar búsqueda', 'Stop looking')
    searching = @('Buscando a alguien con quien jugar… Cuando lo encuentre, el juego se abre solo.', 'Looking for someone to play with… When found, the game opens by itself.')
    inviteSent = @('Invitación enviada a {0}. Si acepta, el juego se abre solo.', 'Invitation sent to {0}. When they accept, the game opens by itself.')
    inviteOver = @('{0} no aceptó la invitación.', '{0} did not accept the invitation.')
    starting2 = @('Partida con {0}: abriendo el juego…', 'Match with {0}: opening the game…')
    netError = @('El servidor de jugadores dijo: {0}', 'The players server said: {0}')
    badLogin = @('Correo o contraseña incorrectos.', 'Wrong email or password.')
    notConfirmed = @('Tu cuenta aún no está confirmada: abre el enlace del correo que te enviamos al crearla.', 'Your account is not confirmed yet: open the link in the email we sent when you made it.')
    notOnline = @('Ese jugador ya no está conectado.', 'That player is not online any more.')
    inviteGone = @('Esa invitación ya no está (pasó más de un minuto).', 'That invitation is gone (over a minute passed).')
    tooMany = @('Demasiadas invitaciones seguidas: espera un momento.', 'Too many invitations in a row: wait a moment.')
    noServer = @('No se pudo hablar con el servidor de jugadores. ¿Tienes internet?', 'The players server could not be reached. Are you online?')
    forgot = @('¿Olvidaste tu contraseña?', 'Forgot your password?')
    cancelInvite = @('Cancelar invitación', 'Cancel the invitation')
    inviteCancelled = @('Invitación a {0} cancelada.', 'Invitation to {0} cancelled.')
    invitedYou = @('¡{0} te invita a jugar!', '{0} invites you to play!')
    friendsLabel = @('Amigos', 'Friends')
    friendsEmpty = @('Todavía no tienes amigos añadidos. Escribe el nombre de un jugador para enviarle una solicitud.', 'No friends added yet. Type a player''s name to send them a request.')
    friendAdd = @('Añadir amigo', 'Add friend')
    friendSent = @('Solicitud de amistad enviada a {0}.', 'Friend request sent to {0}.')
    friendNow = @('{0} y tú ya sois amigos.', 'You and {0} are friends now.')
    friendIncoming = @('Quiere ser tu amigo', 'Wants to be your friend')
    friendOutgoing = @('Solicitud enviada', 'Request sent')
    friendOffline = @('Desconectado', 'Offline')
    friendRemove = @('Quitar', 'Remove')
    noSuchPlayer = @('No hay ningún jugador con ese nombre.', 'There is no player with that name.')
    searchingFor = @('Buscando partida… {0}  ·  {1} buscando ahora', 'Looking for a match… {0}  ·  {1} looking now')
    resultTitle = @('¿Cómo fue la partida contra {0}?', 'How did the match against {0} go?')
    resultHelp = @('Tu respuesta y la de tu rival suben o bajan vuestros puntos del ranking. Si no coinciden, la partida no cuenta.', 'Your answer and your opponent''s move your ranking points. If they do not match, the match does not count.')
    resWin = @('Gané', 'I won'); resLoss = @('Perdí', 'I lost'); resNone = @('No terminamos', 'We did not finish')
    resDone = @('¡Hecho! Los puntos ya están actualizados.', 'Done! The points are updated.')
    resWaiting = @('Guardado. Los puntos se actualizan cuando tu rival responda también.', 'Saved. The points update when your opponent answers too.')
    resDisputed = @('Vuestras respuestas no coinciden: esta partida no cuenta.', 'Your answers do not match: this match does not count.')
    resVoid = @('Anotado: la partida no cuenta.', 'Noted: the match does not count.')
    needSignIn = @('Inicia sesión arriba para invitar o buscar partida.', 'Sign in above to invite or find a match.')
    advShow = @('Conectar con un código', 'Connect with a code'); advHide = @('Ocultar la conexión con código', 'Hide the code connection')
    vsOff = @('Apagado', 'Off'); vsHost = @('Crear sala (jugador 1)', 'Make a room (player 1)'); vsJoin = @('Unirme a una sala (jugador 2)', 'Join a room (player 2)')
    vsPeer = @('Código de la sala de tu amigo', 'Your friend''s room code')
    vsCode = @('Código de tu sala: {0}   (pásaselo a tu amigo)', 'Your room code: {0}   (give it to your friend)')
    vsCopy = @('Copiar el código', 'Copy the code'); vsNewCode = @('Otro código', 'Another code'); vsCopied = @('Código copiado: pégaselo a tu amigo (WhatsApp, Discord...).', 'Code copied: paste it to your friend (WhatsApp, Discord...).')
    vsDelay = @('Retraso (cuadros)', 'Delay (frames)')
    vsDelayHelp = @('Auto se ajusta solo a vuestra conexión. Si eliges un número y va a tirones, súbelo.', 'Auto follows your connection. With a number, raise it if it stutters.')
    vsPlayers = @('Jugadores (contándote a ti)', 'Players (you included)')
    vsAuto = @('Auto', 'Auto')
    vsMine = @('Tu dirección para tu amigo: {0}', 'Your address for your friend: {0}')
    vsMineNone = @('No encontré tu dirección de Radmin VPN: ¿está conectado?', 'Your Radmin VPN address was not found: is it connected?')
    vsHelp = @('Sin Radmin ni puertos: os encontráis en el servidor de salas por el código. Pulsad Jugar todos; el que esté listo antes espera a los demás. El savegame del host viaja solo a los demás al empezar (vuestros savegames no se tocan). El host es el jugador 1 y maneja los menús. Todos necesitáis la misma versión del juego. Es una prueba: puede desincronizarse (sale un aviso en pantalla).', 'No Radmin, no ports: you meet in the lobby server by the code. Everyone presses Play; whoever is ready first waits for the others. The host''s save travels to the others by itself at the start (your own saves are not touched). The host is player 1 and drives the menus. Everyone needs the same game version. It is a test: it may go out of sync (a warning shows on screen).')
    vsSaveMake = @('Crear el savegame para el netplay', 'Make the save for netplay')
    vsSaveUse = @('Usar el savegame del host (ZIP)', 'Use the host''s save (ZIP)')
    vsSaveReady = @('Savegame del netplay: listo. Las partidas en red empiezan siempre desde él; tu savegame propio no se toca.', 'Netplay save: ready. Netplay games always start from it; your own save is not touched.')
    vsSaveNone = @('Todavía no hay savegame del netplay. El host lo crea con su botón y os pasa el ZIP (Discord, WhatsApp...); el otro lo elige con "Usar el savegame del host". Sin él, cada uno usa el suyo y es fácil que los juegos se separen.', 'No netplay save yet. The host makes it with the button and sends the ZIP (Discord...); the other picks it with "Use the host''s save". Without it each uses their own, and the games easily part.')
    vsSaveMade = @('Hecho: {0}. Pásaselo a tu amigo (Discord, WhatsApp...). Vuelve a crearlo si tu savegame cambia.', 'Done: {0}. Send it to your friend (Discord...). Make it again if your save changes.')
    vsSaveFail = @('No se pudo: ', 'It could not be done: ')
    vsNoPeer = @('Escribe el código de la sala de tu amigo para unirte.', 'Type your friend''s room code to join.')
    directHelp = @('Más rápidas. Apagadas, todo pasa por la sala y nadie ve tu dirección de internet.', 'Faster. Off: everything goes through the lobby and nobody learns your internet address.')
    padsTitle = @('Controles', 'Controllers'); kbPlays = @('El teclado juega como', 'The keyboard plays as'); nobody = @('Nadie', 'Nobody')
    background = @('Los mandos funcionan con otra ventana al frente', 'Controllers work while another window is in front')
    padsHelp = @('Lo que cambies aquí se guarda al pulsar Jugar. Dentro del juego también puedes ajustarlo: pulsa Esc, o el botón Inicio del mando, y entra en Controles.', 'What you change here is saved when you press Play. In the game you can set it up too: press Esc, or the controller''s Home button, and go to Controllers.')
    openPads = @('Abrir controllers.ini', 'Open controllers.ini')
    tabKeys = @('Teclado', 'Keyboard'); tabPads = @('Mandos', 'Controllers')
    padPick = @('Mando conectado', 'Connected controller'); padXinput = @('Mando {0} · Xbox (XInput)', 'Controller {0} · Xbox (XInput)')
    padLetters = @('A, B, X e Y como en mi mando', 'A, B, X and Y as on my controller')
    padNone = @('No hay ningún mando conectado. Conéctalo, o pulsa un botón para despertarlo, y aparecerá aquí.', 'No controller is connected. Plug one in, or press a button to wake it, and it shows up here.')
    padWhere = @('{0} ({1}). Se guarda en controllers.ini, en [device {2}].', '{0} ({1}). Saved in controllers.ini, under [device {2}].')
    padStyle = @('Juega como', 'Plays as'); padStyleAuto = @('Automático', 'Automatic')
    padLayout = @('Botones A, B, X e Y', 'A, B, X and Y buttons'); padPositional = @('Por posición, como en Switch', 'By position, as on a Switch'); padLabels = @('Por la letra impresa', 'By the printed letter')
    padDead = @('Zona muerta de los sticks', 'Stick dead zone'); padRumble = @('Vibración', 'Rumble'); padOff = @('Apagada', 'Off')
    padTrig = @('Gatillos: cuánto apretar para ZL y ZR', 'Triggers: how far to pull for ZL and ZR')
    padMapTitle = @('Botones del mando', 'Controller buttons')
    padMapHelp = @('Pulsa un botón de la lista y luego el botón del mando que quieras. Si ese botón ya hacía otra cosa, se intercambian. Esc cancela. Lo que pulses en el mando se ilumina aquí.', 'Pick a button in the list, then press the controller button you want. If that button already did something else, the two swap. Esc cancels. What you press on the controller lights up here.')
    pressPad = @('Pulsa en el mando…', 'Press on the controller…'); resetPad = @('Restaurar este mando', 'Restore this controller')
    padDpad = @('Cruceta', 'D-pad'); padLStick = @('Stick izq. (clic)', 'Left stick (click)'); padRStick = @('Stick der. (clic)', 'Right stick (click)'); padCapture = @('Captura', 'Capture')
    pb_A = @('A', 'A'); pb_B = @('B', 'B'); pb_X = @('X', 'X'); pb_Y = @('Y', 'Y'); pb_L = @('L', 'L'); pb_R = @('R', 'R'); pb_ZL = @('ZL', 'ZL'); pb_ZR = @('ZR', 'ZR')
    pb_PLUS = @('+ (pausa)', '+ (pause)'); pb_MINUS = @('−', '−'); pb_LSTICK = @('Stick izquierdo (clic)', 'Left stick (click)'); pb_RSTICK = @('Stick derecho (clic)', 'Right stick (click)')
    pb_UP = @('Cruceta ↑', 'D-pad ↑'); pb_DOWN = @('Cruceta ↓', 'D-pad ↓'); pb_LEFT = @('Cruceta ←', 'D-pad ←'); pb_RIGHT = @('Cruceta →', 'D-pad →')
    updTitle = @('Actualizaciones', 'Updates'); updCheck = @('Buscar actualizaciones', 'Check for updates'); updChecking = @('Buscando…', 'Checking…')
    updNow = @('Actualizar ahora', 'Update now'); updUpToDate = @('NativeSmash está al día.', 'NativeSmash is up to date.')
    updAvailable = @('Hay una actualización disponible', 'An update is available'); updChanges = @('{0} cambios nuevos', '{0} new changes')
    updLatest = @('Lo último', 'Latest'); updThis = @('Esta copia', 'This copy')
    updFail = @('No se pudo comprobar (¿sin internet, o el repositorio aún no existe?).', 'Could not check (no internet, or the repository does not exist yet?).')
    updUnknown = @('No sé de qué versión es esta copia: se sabe si se descarga de GitHub o se clona.', 'This copy''s version is not known: it is when it is downloaded from GitHub or cloned.')
    updOwn = @('Esta copia tiene cambios propios, así que no se actualiza sola.', 'This copy has changes of its own, so it does not update itself.')
    updConfirm = @('Se descargará la versión nueva y se recompilará lo que cambió. Tus partidas y ajustes se conservan. ¿Seguir?', 'The new version will be downloaded and what changed rebuilt. Your saves and settings are kept. Go on?')
    keysTitle = @('Teclas del teclado', 'Keyboard keys'); pressKey = @('Pulsa una tecla…', 'Press a key…')
    resetKeys = @('Restaurar teclas', 'Restore keys')
    keysHelp = @('Pulsa una acción y luego la tecla que quieras. Esc cancela. F1 a F6, F11 y Esc son del juego y no se pueden usar.', 'Click an action, then the key you want. Esc cancels. F1 to F6, F11 and Esc are the game''s own and cannot be used.')
    keyReserved = @('Esa tecla es del juego. Elige otra.', 'That key belongs to the game. Pick another.')
    act_a = @('A', 'A'); act_b = @('B', 'B'); act_x = @('X', 'X'); act_y = @('Y', 'Y'); act_l = @('L', 'L'); act_r = @('R', 'R')
    act_zl = @('ZL', 'ZL'); act_zr = @('ZR', 'ZR'); act_plus = @('+ (pausa)', '+ (pause)'); act_minus = @('−', '−')
    act_left = @('Stick izquierdo ←', 'Left stick ←'); act_right = @('Stick izquierdo →', 'Left stick →')
    act_up = @('Stick izquierdo ↑', 'Left stick ↑'); act_down = @('Stick izquierdo ↓', 'Left stick ↓')
    act_rs_left = @('Stick derecho ←', 'Right stick ←'); act_rs_right = @('Stick derecho →', 'Right stick →')
    act_rs_up = @('Stick derecho ↑', 'Right stick ↑'); act_rs_down = @('Stick derecho ↓', 'Right stick ↓')
    videoTitle = @('Pantalla y rendimiento', 'Display and performance'); windowMode = @('Ventana', 'Window')
    winWindowed = @('En ventana', 'Windowed'); winBorderless = @('Sin bordes', 'Borderless'); winFull = @('Pantalla completa', 'Full screen')
    size = @('Tamaño de la ventana', 'Window size'); pacing = @('Ritmo de imagen', 'Frame pacing')
    texLabel = @('Calidad de texturas', 'Texture quality')
    fskipBox = @('Saltar fotogramas si hace falta', 'Skip frames when needed')
    fskipHelp = @('Si tu PC no puede dibujar 60 fotogramas por segundo, el juego deja de dibujar algunos para seguir a su velocidad normal, en lugar de ir en cámara lenta. Solo actúa cuando hace falta.', 'If your PC cannot draw 60 frames a second, the game stops drawing some of them to stay at its normal speed instead of slowing down. It only acts when needed.')
    texAuto = @('Automática', 'Automatic'); texHigh = @('Alta', 'High'); texMed = @('Media', 'Medium'); texLow = @('Baja', 'Low')
    texHelp = @('Media y Baja dejan fuera el tamaño más grande de las texturas grandes: se ven casi igual con la imagen a menos de su tamaño completo, y el juego usa menos memoria, lo que importa en un PC con poca RAM. Automática: Media con menos de 12 GB de RAM, Alta con más.', 'Medium and Low leave out the biggest size of large textures: they look almost the same with the picture smaller than full size, and the game uses less memory, which matters on a PC with little RAM. Automatic: Medium with under 12 GB of RAM, High with more.')
    pacClock = @('60 estable', 'Steady 60'); pacVrr = @('G-Sync / FreeSync', 'G-Sync / FreeSync'); pacDisplay = @('Seguir el monitor', 'Follow the monitor')
    gpu = @('Tarjeta gráfica', 'Graphics card'); gpuAuto = @('La más rápida (auto)', 'The fastest (auto)')
    vsyncBox = @('Sin cortes en la imagen (sincronizar con el monitor)', 'No tearing (sync with the monitor)')
    vsyncHelp = @('Activado: la imagen nunca se corta, a cambio de un fotograma más de retraso (16,7 ms). Desactivado: menos retraso, pero puede verse una línea de corte.', 'On: the picture never tears, at the price of one more frame of delay (16.7 ms). Off: less delay, but a tear line can show.')
    fpsHelp = @('El juego va siempre a 60 fotogramas por segundo, como en la Switch: es el ritmo de todos en línea, así que tu pantalla no afecta a nadie. En un monitor de 120 Hz sigue a 60.', 'The game always runs at 60 frames a second, as on the Switch: it is the pace of everyone online, so your screen affects nobody. On a 120 Hz monitor it still runs at 60.')
    upLabel = @('Escalado de imagen (AMD FSR 1)', 'Image upscaling (AMD FSR 1)')
    upOff = @('Desactivado', 'Off'); upQuality = @('Calidad', 'Quality'); upBalanced = @('Equilibrado', 'Balanced')
    upPerf = @('Rendimiento', 'Performance'); upUltra = @('Ultra rendimiento', 'Ultra performance')
    sharpLabel = @('Nitidez', 'Sharpness'); shNone = @('Ninguna', 'None'); shSoft = @('Suave', 'Soft'); shNormal = @('Normal', 'Normal'); shSharp = @('Nítida', 'Sharp')
    upHelp = @('El juego se dibuja con menos píxeles y FSR 1 de AMD reconstruye la imagen: más fotogramas en PCs justos. Funciona con cualquier tarjeta gráfica y se aplica al abrir el juego.', 'The game is drawn with fewer pixels and AMD FSR 1 rebuilds the picture: more frames on modest PCs. It works with any graphics card and takes effect when the game starts.')
    hideCursor = @('Ocultar el puntero cuando no se mueve', 'Hide the pointer when it stops moving'); overlay = @('Mostrar los fotogramas por segundo (F1)', 'Show the frame rate (F1)')
    shadersTitle = @('Shaders', 'Shaders')
    shadersHelp = @('Los shaders son los programas gráficos del juego. Se preparan la primera vez que se usa cada efecto y se guardan en disco; así cada tirón ocurre una sola vez. Preparar todos de antemano tarda de minutos a horas y evita tirones nuevos.', 'Shaders are the game''s graphics programs. Each one is prepared the first time an effect is used and kept on disk, so each hitch happens only once. Preparing them all ahead takes minutes to hours and avoids new hitches.')
    shadersCount = @('Shaders guardados', 'Shaders saved'); shadersPlayed = @('usados en partida', 'used in play'); prepare = @('Preparar todos ahora', 'Prepare them all now')
    gameTitle = @('El juego', 'The game'); noVersions = @('Ninguna versión instalada todavía.', 'No game version installed yet.')
    tools = @('Herramientas', 'Tools'); unlock = @('Desbloquear luchadores', 'Unlock all fighters'); openFolder = @('Abrir la carpeta', 'Open the folder')
    openReports = @('Informes de fallos', 'Crash reports'); openLog = @('Registro de la última partida', 'Last game log')
    close = @('Cerrar', 'Close'); notBuilt = @('No hay una versión del juego instalada.', 'No game version is installed.')
    badServer = @('El servidor es host:puerto, por ejemplo 192.168.1.20:39420.', 'Your own server is host:port, e.g. 192.168.1.20:39420.')
    saveFail = @('No se pudieron guardar los ajustes: ', 'The settings could not be saved: ')
    noStart = @('El juego no abrió su ventana. Mira el registro de la partida (play.log).', 'The game did not open its window. Look at the game log (play.log).')
}
function T([string]$Key) { $i = if ($Lang -eq 'es') { 0 } else { 1 }; return $Words[$Key][$i] }

# ---- current settings --------------------------------------------------------------------------
$displayIni = Join-Path $Root 'display.ini'
$padsIni = Join-Path $Root 'controllers.ini'
$launcherIni = Join-Path $Root 'launcher.ini'
$serverFile = Join-Path $Root 'online_server.txt'
$nameFile = Join-Path $Root 'online_name.txt'
$directFile = Join-Path $Root 'online_direct.txt'
$reportFile = Join-Path $Root 'send_reports.txt'   # (tools\send_report.ps1: "off" stops the error reports)
$netcodeFile = Join-Path $Root 'online_netcode.txt'
$delayFile = Join-Path $Root 'online_delay.txt'
$vsFile = Join-Path $Root 'vs_netplay.txt'   # the VS netplay (docs\NETPLAY.md): off | host <delay> | join <address> <delay>

function Get-Setting([string]$Path, [string]$Section, [string]$Key) {
    $v = Get-IniValue $Path $Section $Key
    if ($null -eq $v -or $v -eq '') { return $Defaults[$Key] }
    return $v
}

$versions = @(Get-GameVersions $Root)
$preferred = $null
if ($env:SSBU_TREE) { $preferred = $versions | Where-Object { $_.Tree -eq $env:SSBU_TREE } | Select-Object -First 1 }
if (-not $preferred) { $preferred = Get-DefaultVersion $versions $Root }
$script:tree = if ($preferred) { $preferred.Tree } else { $null }

Add-Type -AssemblyName System.Windows.Forms   # (only for the Shift key)
if ($Auto -and -not $Shot -and -not (Test-ShowLauncher) -and -not ([Windows.Forms.Control]::ModifierKeys -band [Windows.Forms.Keys]::Shift)) {
    if (-not $preferred) { Write-Host 'No game version is built here yet - see docs\VERSIONS.md.'; exit 1 }
    Write-Host "Game version $($preferred.Label)"
    Save-Version $preferred.Tree
    Start-Game $preferred.Tree
    exit 0
}

$serverWords = @(([string](Get-LineValue $serverFile)).Trim() -split '[ \t]+') + @('', '')
$lobbyHost = $serverWords[0]; $lobbyCode = $serverWords[1]
$netMode = if (-not $lobbyHost -or $lobbyHost -eq 'off') { 'offline' }
           elseif ($lobbyHost -eq $PublicLobby) { if ($lobbyCode) { 'private' } else { 'public' } }
           else { 'custom' }

$shaderDir = Join-Path $Root 'dump\shadercache'
$shaderTotal = 0; $shaderPlayed = 0
if (Test-Path -LiteralPath $shaderDir) {
    $played = [DateTime]'2002-01-01'
    foreach ($f in [IO.DirectoryInfo]::new($shaderDir).EnumerateFiles('*.dxbc')) { $shaderTotal++; if ($f.LastWriteTimeUtc -ge $played) { $shaderPlayed++ } }
}

# ---- the window --------------------------------------------------------------------------------
$xaml = @'
<Window xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
        xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
        Title="NativeSmash" Width="1160" Height="680" WindowStyle="None" AllowsTransparency="True"
        Background="Transparent" ResizeMode="NoResize" WindowStartupLocation="CenterScreen"
        FontFamily="Segoe UI" TextOptions.TextFormattingMode="Display" UseLayoutRounding="True">
  <Window.Resources>
    <Color x:Key="AccentA">#FF6D7DFF</Color>
    <Color x:Key="AccentB">#FFA66BFF</Color>
    <SolidColorBrush x:Key="Ink" Color="#FFF2F4FB"/>
    <SolidColorBrush x:Key="Muted" Color="#FF9AA1BC"/>
    <SolidColorBrush x:Key="Veil" Color="#14FFFFFF"/>
    <SolidColorBrush x:Key="Edge" Color="#26FFFFFF"/>
    <LinearGradientBrush x:Key="Accent" StartPoint="0,0" EndPoint="1,1">
      <GradientStop Color="{StaticResource AccentA}" Offset="0"/><GradientStop Color="{StaticResource AccentB}" Offset="1"/>
    </LinearGradientBrush>

    <Style x:Key="Label" TargetType="TextBlock"><Setter Property="Foreground" Value="{StaticResource Muted}"/><Setter Property="FontSize" Value="13"/><Setter Property="Margin" Value="0,18,0,8"/></Style>
    <Style x:Key="Help" TargetType="TextBlock"><Setter Property="Foreground" Value="{StaticResource Muted}"/><Setter Property="FontSize" Value="13"/><Setter Property="TextWrapping" Value="Wrap"/><Setter Property="Margin" Value="0,8,0,0"/></Style>
    <Style x:Key="Card" TargetType="Border"><Setter Property="CornerRadius" Value="20"/><Setter Property="Background" Value="#0FFFFFFF"/><Setter Property="BorderBrush" Value="#1FFFFFFF"/><Setter Property="BorderThickness" Value="1"/><Setter Property="Padding" Value="26,22,26,24"/><Setter Property="Margin" Value="0,22,0,0"/></Style>
    <Style x:Key="CardTitle" TargetType="TextBlock"><Setter Property="Foreground" Value="{StaticResource Ink}"/><Setter Property="FontSize" Value="19"/><Setter Property="FontWeight" Value="SemiBold"/></Style>
    <Style x:Key="Section" TargetType="TextBlock"><Setter Property="Foreground" Value="{StaticResource Ink}"/><Setter Property="FontSize" Value="14"/><Setter Property="FontWeight" Value="SemiBold"/><Setter Property="Margin" Value="0,24,0,10"/></Style>
    <Style x:Key="Title" TargetType="TextBlock"><Setter Property="Foreground" Value="{StaticResource Ink}"/><Setter Property="FontSize" Value="30"/><Setter Property="FontWeight" Value="SemiBold"/></Style>

    <!-- a side menu entry -->
    <Style x:Key="Nav" TargetType="RadioButton">
      <Setter Property="Foreground" Value="{StaticResource Muted}"/><Setter Property="FontSize" Value="15"/><Setter Property="Cursor" Value="Hand"/><Setter Property="Margin" Value="0,2"/>
      <Setter Property="Template"><Setter.Value><ControlTemplate TargetType="RadioButton">
        <Border x:Name="b" CornerRadius="12" Padding="16,11" Background="Transparent">
          <Grid><Border x:Name="bar" Width="4" CornerRadius="2" HorizontalAlignment="Left" Margin="-8,2" Background="{StaticResource Accent}" Opacity="0"/>
            <StackPanel Orientation="Horizontal" Margin="6,0,0,0">
              <TextBlock x:Name="ico" FontFamily="Segoe MDL2 Assets" FontSize="16" Width="26" VerticalAlignment="Center" Foreground="{TemplateBinding Foreground}" Opacity="0.75"/>
              <ContentPresenter VerticalAlignment="Center" TextBlock.Foreground="{TemplateBinding Foreground}"/></StackPanel></Grid></Border>
        <ControlTemplate.Triggers>
          <Trigger Property="Tag" Value="play"><Setter TargetName="ico" Property="Text" Value="&#xE768;"/></Trigger>
          <Trigger Property="Tag" Value="online"><Setter TargetName="ico" Property="Text" Value="&#xE774;"/></Trigger>
          <Trigger Property="Tag" Value="pads"><Setter TargetName="ico" Property="Text" Value="&#xE7FC;"/></Trigger>
          <Trigger Property="Tag" Value="video"><Setter TargetName="ico" Property="Text" Value="&#xE7F4;"/></Trigger>
          <Trigger Property="Tag" Value="shaders"><Setter TargetName="ico" Property="Text" Value="&#xE790;"/></Trigger>
          <Trigger Property="Tag" Value="game"><Setter TargetName="ico" Property="Text" Value="&#xE713;"/></Trigger>
          <Trigger Property="IsMouseOver" Value="True"><Setter TargetName="b" Property="Background" Value="#12FFFFFF"/><Setter TargetName="ico" Property="Opacity" Value="1"/></Trigger>
          <Trigger Property="IsChecked" Value="True"><Setter TargetName="b" Property="Background" Value="#22FFFFFF"/><Setter TargetName="bar" Property="Opacity" Value="1"/><Setter TargetName="ico" Property="Opacity" Value="1"/><Setter Property="Foreground" Value="{StaticResource Ink}"/></Trigger>
        </ControlTemplate.Triggers></ControlTemplate></Setter.Value></Setter>
    </Style>

    <!-- one of several choices -->
    <Style x:Key="Chip" TargetType="RadioButton">
      <Setter Property="Foreground" Value="{StaticResource Ink}"/><Setter Property="FontSize" Value="14"/><Setter Property="Cursor" Value="Hand"/><Setter Property="Margin" Value="0,0,10,10"/>
      <Setter Property="Template"><Setter.Value><ControlTemplate TargetType="RadioButton">
        <Border x:Name="b" CornerRadius="12" Padding="16,10" Background="{StaticResource Veil}" BorderBrush="{StaticResource Edge}" BorderThickness="1">
          <ContentPresenter HorizontalAlignment="Center" TextBlock.Foreground="{TemplateBinding Foreground}"/></Border>
        <ControlTemplate.Triggers>
          <Trigger Property="IsMouseOver" Value="True"><Setter TargetName="b" Property="Background" Value="#22FFFFFF"/></Trigger>
          <Trigger Property="IsChecked" Value="True"><Setter TargetName="b" Property="Background" Value="{StaticResource Accent}"/><Setter TargetName="b" Property="BorderBrush" Value="#66FFFFFF"/></Trigger>
          <Trigger Property="IsEnabled" Value="False"><Setter Property="Opacity" Value="0.4"/></Trigger>
        </ControlTemplate.Triggers></ControlTemplate></Setter.Value></Setter>
    </Style>

    <!-- an on/off switch -->
    <Style x:Key="Switch" TargetType="CheckBox">
      <Setter Property="Foreground" Value="{StaticResource Ink}"/><Setter Property="FontSize" Value="14"/><Setter Property="Cursor" Value="Hand"/><Setter Property="Margin" Value="0,8,0,0"/>
      <Setter Property="Template"><Setter.Value><ControlTemplate TargetType="CheckBox">
        <StackPanel Orientation="Horizontal">
          <Border x:Name="track" Width="46" Height="26" CornerRadius="13" Background="#33FFFFFF" VerticalAlignment="Center">
            <Ellipse x:Name="knob" Width="20" Height="20" Fill="White" HorizontalAlignment="Left" Margin="3,0"/></Border>
          <TextBlock Text="{TemplateBinding Content}" Foreground="{TemplateBinding Foreground}" Margin="14,0,0,0" VerticalAlignment="Center" TextWrapping="Wrap" MaxWidth="560"/>
        </StackPanel>
        <ControlTemplate.Triggers>
          <Trigger Property="IsChecked" Value="True"><Setter TargetName="track" Property="Background" Value="{StaticResource Accent}"/><Setter TargetName="knob" Property="HorizontalAlignment" Value="Right"/></Trigger>
          <Trigger Property="IsEnabled" Value="False"><Setter Property="Opacity" Value="0.4"/></Trigger>
        </ControlTemplate.Triggers></ControlTemplate></Setter.Value></Setter>
    </Style>

    <Style x:Key="Field" TargetType="TextBox">
      <Setter Property="Foreground" Value="{StaticResource Ink}"/><Setter Property="CaretBrush" Value="White"/><Setter Property="FontSize" Value="14"/>
      <Setter Property="Template"><Setter.Value><ControlTemplate TargetType="TextBox">
        <Border x:Name="b" CornerRadius="12" Background="{StaticResource Veil}" BorderBrush="{StaticResource Edge}" BorderThickness="1" Padding="14,10">
          <ScrollViewer x:Name="PART_ContentHost"/></Border>
        <ControlTemplate.Triggers>
          <Trigger Property="IsKeyboardFocused" Value="True"><Setter TargetName="b" Property="BorderBrush" Value="#FF8E9BFF"/></Trigger>
          <Trigger Property="IsEnabled" Value="False"><Setter Property="Opacity" Value="0.4"/></Trigger>
        </ControlTemplate.Triggers></ControlTemplate></Setter.Value></Setter>
    </Style>

    <Style x:Key="PwField" TargetType="PasswordBox">
      <Setter Property="Foreground" Value="{StaticResource Ink}"/><Setter Property="CaretBrush" Value="White"/><Setter Property="FontSize" Value="14"/>
      <Setter Property="Template"><Setter.Value><ControlTemplate TargetType="PasswordBox">
        <Border x:Name="b" CornerRadius="12" Background="{StaticResource Veil}" BorderBrush="{StaticResource Edge}" BorderThickness="1" Padding="14,10">
          <ScrollViewer x:Name="PART_ContentHost"/></Border>
        <ControlTemplate.Triggers>
          <Trigger Property="IsKeyboardFocused" Value="True"><Setter TargetName="b" Property="BorderBrush" Value="#FF8E9BFF"/></Trigger>
        </ControlTemplate.Triggers></ControlTemplate></Setter.Value></Setter>
    </Style>
    <Style TargetType="ScrollBar">
      <Setter Property="Width" Value="10"/><Setter Property="Background" Value="Transparent"/>
      <Setter Property="Template"><Setter.Value><ControlTemplate TargetType="ScrollBar">
        <Track x:Name="PART_Track" IsDirectionReversed="True">
          <Track.DecreaseRepeatButton><RepeatButton Command="ScrollBar.PageUpCommand" Opacity="0" IsTabStop="False"/></Track.DecreaseRepeatButton>
          <Track.IncreaseRepeatButton><RepeatButton Command="ScrollBar.PageDownCommand" Opacity="0" IsTabStop="False"/></Track.IncreaseRepeatButton>
          <Track.Thumb><Thumb><Thumb.Template><ControlTemplate TargetType="Thumb">
            <Border Background="#33FFFFFF" CornerRadius="5" Margin="2,0"/>
          </ControlTemplate></Thumb.Template></Thumb></Track.Thumb>
        </Track>
      </ControlTemplate></Setter.Value></Setter>
    </Style>

    <Style x:Key="Soft" TargetType="Button">
      <Setter Property="Foreground" Value="{StaticResource Ink}"/><Setter Property="FontSize" Value="14"/><Setter Property="Cursor" Value="Hand"/><Setter Property="Margin" Value="0,0,10,10"/>
      <Setter Property="Template"><Setter.Value><ControlTemplate TargetType="Button">
        <Border x:Name="b" CornerRadius="12" Padding="18,11" Background="{StaticResource Veil}" BorderBrush="{StaticResource Edge}" BorderThickness="1">
          <ContentPresenter HorizontalAlignment="Center" TextBlock.Foreground="{TemplateBinding Foreground}"/></Border>
        <ControlTemplate.Triggers><Trigger Property="IsMouseOver" Value="True"><Setter TargetName="b" Property="Background" Value="#26FFFFFF"/></Trigger>
          <Trigger Property="IsEnabled" Value="False"><Setter Property="Opacity" Value="0.4"/></Trigger></ControlTemplate.Triggers>
      </ControlTemplate></Setter.Value></Setter>
    </Style>

    <!-- minimize and close -->
    <Style x:Key="WinBtn" TargetType="Button">
      <Setter Property="Foreground" Value="{StaticResource Muted}"/><Setter Property="FontFamily" Value="Segoe MDL2 Assets"/><Setter Property="FontSize" Value="11"/><Setter Property="Cursor" Value="Hand"/><Setter Property="Width" Value="34"/><Setter Property="Height" Value="34"/><Setter Property="Margin" Value="4,0,0,0"/>
      <Setter Property="Template"><Setter.Value><ControlTemplate TargetType="Button">
        <Border x:Name="b" CornerRadius="10" Background="Transparent">
          <ContentPresenter HorizontalAlignment="Center" VerticalAlignment="Center" TextBlock.Foreground="{TemplateBinding Foreground}"/></Border>
        <ControlTemplate.Triggers>
          <Trigger Property="IsMouseOver" Value="True"><Setter TargetName="b" Property="Background" Value="#22FFFFFF"/><Setter Property="Foreground" Value="{StaticResource Ink}"/></Trigger>
          <Trigger Property="IsPressed" Value="True"><Setter TargetName="b" Property="Background" Value="#33FFFFFF"/></Trigger>
        </ControlTemplate.Triggers></ControlTemplate></Setter.Value></Setter>
    </Style>
    <Style x:Key="CloseWin" TargetType="Button" BasedOn="{StaticResource WinBtn}">
      <Setter Property="Template"><Setter.Value><ControlTemplate TargetType="Button">
        <Border x:Name="b" CornerRadius="10" Background="Transparent">
          <ContentPresenter HorizontalAlignment="Center" VerticalAlignment="Center" TextBlock.Foreground="{TemplateBinding Foreground}"/></Border>
        <ControlTemplate.Triggers>
          <Trigger Property="IsMouseOver" Value="True"><Setter TargetName="b" Property="Background" Value="#CCE5484D"/><Setter Property="Foreground" Value="White"/></Trigger>
          <Trigger Property="IsPressed" Value="True"><Setter TargetName="b" Property="Background" Value="#FFC93A40"/></Trigger>
        </ControlTemplate.Triggers></ControlTemplate></Setter.Value></Setter>
    </Style>

    <Style x:Key="AccentBtn" TargetType="Button">
      <Setter Property="Foreground" Value="White"/><Setter Property="FontSize" Value="15"/><Setter Property="FontWeight" Value="SemiBold"/><Setter Property="Cursor" Value="Hand"/><Setter Property="Margin" Value="0,0,12,0"/>
      <Setter Property="Template"><Setter.Value><ControlTemplate TargetType="Button">
        <Border x:Name="b" CornerRadius="14" Background="{StaticResource Accent}" Padding="24,12" BorderBrush="#55FFFFFF" BorderThickness="1">
          <ContentPresenter HorizontalAlignment="Center" VerticalAlignment="Center" TextBlock.Foreground="White"/></Border>
        <ControlTemplate.Triggers>
          <Trigger Property="IsMouseOver" Value="True"><Setter TargetName="b" Property="Opacity" Value="0.88"/></Trigger>
          <Trigger Property="IsEnabled" Value="False"><Setter Property="Opacity" Value="0.4"/></Trigger>
        </ControlTemplate.Triggers></ControlTemplate></Setter.Value></Setter>
    </Style>
    <Style x:Key="PlayBtn" TargetType="Button">
      <Setter Property="Foreground" Value="White"/><Setter Property="FontSize" Value="20"/><Setter Property="FontWeight" Value="Bold"/><Setter Property="Cursor" Value="Hand"/>
      <Setter Property="Template"><Setter.Value><ControlTemplate TargetType="Button">
        <Grid x:Name="root" RenderTransformOrigin="0.5,0.5"><Grid.RenderTransform><ScaleTransform x:Name="sc" ScaleX="1" ScaleY="1"/></Grid.RenderTransform>
          <Border x:Name="glow" CornerRadius="32" Background="{StaticResource Accent}" Opacity="0.55"><Border.Effect><BlurEffect Radius="26"/></Border.Effect></Border>
          <Border x:Name="b" CornerRadius="32" Background="{StaticResource Accent}" Padding="56,18" BorderBrush="#55FFFFFF" BorderThickness="1"><ContentPresenter HorizontalAlignment="Center" VerticalAlignment="Center" TextBlock.Foreground="White"/></Border></Grid>
        <ControlTemplate.Triggers>
          <Trigger Property="IsMouseOver" Value="True"><Setter TargetName="glow" Property="Opacity" Value="0.9"/>
            <Trigger.EnterActions><BeginStoryboard><Storyboard>
              <DoubleAnimation Storyboard.TargetName="sc" Storyboard.TargetProperty="ScaleX" To="1.035" Duration="0:0:0.14"/>
              <DoubleAnimation Storyboard.TargetName="sc" Storyboard.TargetProperty="ScaleY" To="1.035" Duration="0:0:0.14"/></Storyboard></BeginStoryboard></Trigger.EnterActions>
            <Trigger.ExitActions><BeginStoryboard><Storyboard>
              <DoubleAnimation Storyboard.TargetName="sc" Storyboard.TargetProperty="ScaleX" To="1" Duration="0:0:0.18"/>
              <DoubleAnimation Storyboard.TargetName="sc" Storyboard.TargetProperty="ScaleY" To="1" Duration="0:0:0.18"/></Storyboard></BeginStoryboard></Trigger.ExitActions></Trigger>
          <Trigger Property="IsPressed" Value="True">
            <Trigger.EnterActions><BeginStoryboard><Storyboard>
              <DoubleAnimation Storyboard.TargetName="sc" Storyboard.TargetProperty="ScaleX" To="0.97" Duration="0:0:0.06"/>
              <DoubleAnimation Storyboard.TargetName="sc" Storyboard.TargetProperty="ScaleY" To="0.97" Duration="0:0:0.06"/></Storyboard></BeginStoryboard></Trigger.EnterActions></Trigger>
          <Trigger Property="IsEnabled" Value="False"><Setter Property="Opacity" Value="0.35"/></Trigger>
        </ControlTemplate.Triggers></ControlTemplate></Setter.Value></Setter>
    </Style>
  </Window.Resources>

  <Border x:Name="Shell" CornerRadius="26" BorderThickness="1" Margin="22">
    <Border.BorderBrush><LinearGradientBrush StartPoint="0,0" EndPoint="1,1"><GradientStop Color="#55FFFFFF" Offset="0"/><GradientStop Color="#1AFFFFFF" Offset="0.45"/><GradientStop Color="#33A66BFF" Offset="1"/></LinearGradientBrush></Border.BorderBrush>
    <Border.Background><LinearGradientBrush StartPoint="0,0" EndPoint="1,1">
      <GradientStop Color="#FF141935" Offset="0"/><GradientStop x:Name="Mid" Color="#FF0E1124" Offset="0.55"/><GradientStop Color="#FF080A15" Offset="1"/></LinearGradientBrush></Border.Background>
    <Border.Effect><DropShadowEffect BlurRadius="40" ShadowDepth="0" Opacity="0.65"/></Border.Effect>
    <Grid>
      <Ellipse x:Name="Aura" Width="560" Height="560" HorizontalAlignment="Right" VerticalAlignment="Top" Margin="0,-170,-120,0" IsHitTestVisible="False" Opacity="0.55">
        <Ellipse.Fill><RadialGradientBrush><GradientStop Color="#886D7DFF" Offset="0"/><GradientStop Color="#00A66BFF" Offset="1"/></RadialGradientBrush></Ellipse.Fill></Ellipse>
      <Grid>
        <Grid.ColumnDefinitions><ColumnDefinition Width="250"/><ColumnDefinition Width="*"/></Grid.ColumnDefinitions>

        <!-- side menu -->
        <Border Grid.Column="0" Background="#0DFFFFFF" CornerRadius="26,0,0,26" BorderBrush="#14FFFFFF" BorderThickness="0,0,1,0">
          <Grid Margin="22,26,22,22">
            <Grid.RowDefinitions><RowDefinition Height="Auto"/><RowDefinition Height="*"/><RowDefinition Height="Auto"/></Grid.RowDefinitions>
            <StackPanel><TextBlock Text="NativeSmash" FontSize="23" FontWeight="Bold" Foreground="{StaticResource Ink}"/>
              <TextBlock x:Name="Tagline" FontSize="12" Foreground="{StaticResource Muted}" Margin="0,4,0,28" TextWrapping="Wrap"/></StackPanel>
            <StackPanel Grid.Row="1">
              <RadioButton x:Name="NavPlay" Style="{StaticResource Nav}" GroupName="nav" Tag="play"/>
              <RadioButton x:Name="NavOnline" Style="{StaticResource Nav}" GroupName="nav" Tag="online"/>
              <RadioButton x:Name="NavPads" Style="{StaticResource Nav}" GroupName="nav" Tag="pads"/>
              <RadioButton x:Name="NavVideo" Style="{StaticResource Nav}" GroupName="nav" Tag="video"/>
              <RadioButton x:Name="NavShaders" Style="{StaticResource Nav}" GroupName="nav" Tag="shaders"/>
              <RadioButton x:Name="NavGame" Style="{StaticResource Nav}" GroupName="nav" Tag="game"/>
            </StackPanel>
            <TextBlock x:Name="Footer" Grid.Row="2" FontSize="12" Foreground="{StaticResource Muted}" TextWrapping="Wrap"/>
          </Grid>
        </Border>

        <!-- pages -->
        <!-- after a match the server arranged: how it went (the ranking's points) -->
        <Border x:Name="ResultPanel" Grid.ColumnSpan="2" Visibility="Collapsed" Panel.ZIndex="20" CornerRadius="26" Background="#E60B0D1C">
          <Border Style="{StaticResource Card}" Background="#FF171B36" HorizontalAlignment="Center" VerticalAlignment="Center" MaxWidth="560" Padding="34,30">
            <StackPanel>
              <TextBlock x:Name="ResultTitle" Style="{StaticResource CardTitle}" FontSize="24" TextWrapping="Wrap"/>
              <TextBlock x:Name="ResultHelp" Style="{StaticResource Help}" FontSize="14" Margin="0,10,0,0"/>
              <WrapPanel Margin="0,24,0,0">
                <Button x:Name="ResWinBtn" Style="{StaticResource AccentBtn}"/>
                <Button x:Name="ResLossBtn" Style="{StaticResource Soft}" Margin="0,0,12,0"/>
                <Button x:Name="ResNoneBtn" Style="{StaticResource Soft}" Margin="0"/>
              </WrapPanel>
              <TextBlock x:Name="ResultNote" Style="{StaticResource Help}" Foreground="#FFB9C2FF" Margin="0,16,0,0" TextWrapping="Wrap"/>
            </StackPanel>
          </Border>
        </Border>
        <Grid Grid.Column="1" x:Name="Pages" Margin="46,40,46,34">
          <StackPanel Orientation="Horizontal" HorizontalAlignment="Right" VerticalAlignment="Top" Margin="0,-22,-26,0" Panel.ZIndex="5">
            <Button x:Name="MinBtn" Style="{StaticResource WinBtn}" Content="&#xE921;" ToolTip="Minimizar"/>
            <Button x:Name="CloseBtn" Style="{StaticResource CloseWin}" Content="&#xE8BB;" ToolTip="Cerrar"/>
          </StackPanel>

          <StackPanel x:Name="PagePlay" VerticalAlignment="Center">
            <Border x:Name="UpdBanner" Visibility="Collapsed" CornerRadius="14" Background="#22A66BFF" BorderBrush="#55A66BFF" BorderThickness="1" Padding="16,6,6,0" HorizontalAlignment="Left" Margin="0,0,0,26">
              <StackPanel Orientation="Horizontal"><TextBlock x:Name="UpdBannerText" Foreground="{StaticResource Ink}" FontSize="14" VerticalAlignment="Center" Margin="0,0,14,6"/>
                <Button x:Name="UpdBannerBtn" Style="{StaticResource Soft}" Padding="0" Margin="0,0,0,6"/></StackPanel></Border>
            <TextBlock x:Name="PlayTitle" Style="{StaticResource Title}" FontSize="40"/>
            <TextBlock x:Name="PlayHelp" Style="{StaticResource Help}" FontSize="15" MaxWidth="560" HorizontalAlignment="Left"/>
            <TextBlock x:Name="VersionLabel" Style="{StaticResource Label}" Margin="0,34,0,8"/>
            <TextBlock x:Name="VersionValue" Foreground="{StaticResource Ink}" FontSize="18" FontWeight="SemiBold"/>
            <StackPanel Orientation="Horizontal" Margin="0,40,0,0">
              <Button x:Name="PlayBtn" Style="{StaticResource PlayBtn}"/>
              <Button x:Name="InstallBtn" Style="{StaticResource Soft}" Margin="18,0,0,0" VerticalAlignment="Center"/>
            </StackPanel>
            <CheckBox x:Name="DirectBox" Style="{StaticResource Switch}" Margin="0,34,0,0"/>
            <CheckBox x:Name="ReportBox" Style="{StaticResource Switch}" Margin="0,18,0,0"/>
            <TextBlock x:Name="ReportHelp" Style="{StaticResource Help}" MaxWidth="560" HorizontalAlignment="Left"/>
          </StackPanel>

          <ScrollViewer x:Name="PageOnline" Visibility="Collapsed" VerticalScrollBarVisibility="Auto" Padding="0,0,14,0">
          <StackPanel>
            <TextBlock x:Name="OnlineTitle" Style="{StaticResource Title}"/>
            <TextBlock x:Name="NameLabel" Style="{StaticResource Label}"/>
            <TextBox x:Name="NameBox" Style="{StaticResource Field}" MaxLength="32" Width="360" HorizontalAlignment="Left"/>

            <!-- Local Wireless: as it was -->
            <Border Style="{StaticResource Card}">
              <StackPanel>
                <TextBlock x:Name="LwTitle" Style="{StaticResource CardTitle}"/>
                <TextBlock x:Name="LwHelp" Style="{StaticResource Help}" MaxWidth="600" HorizontalAlignment="Left"/>
                <TextBlock x:Name="LobbyLabel" Style="{StaticResource Label}"/>
                <WrapPanel x:Name="LobbyChips"/>
                <TextBlock x:Name="CodeLabel" Style="{StaticResource Label}"/>
                <TextBox x:Name="CodeBox" Style="{StaticResource Field}" MaxLength="32" Width="360" HorizontalAlignment="Left"/>
                <TextBlock x:Name="ServerLabel" Style="{StaticResource Label}"/>
                <TextBox x:Name="ServerBox" Style="{StaticResource Field}" Width="360" HorizontalAlignment="Left"/>
                <CheckBox x:Name="DirectConn" Style="{StaticResource Switch}" Margin="0,22,0,0"/>
                <TextBlock x:Name="DirectHelp" Style="{StaticResource Help}" MaxWidth="560" HorizontalAlignment="Left"/>
                <TextBlock x:Name="DelayLabel" Style="{StaticResource Label}" Margin="0,22,0,8"/>
                <WrapPanel x:Name="DelayChips" MaxWidth="660" HorizontalAlignment="Left"/>
              </StackPanel>
            </Border>

            <!-- the VS netplay: mode, delay, quick match, players, invitations; a code by hand under Advanced -->
            <Border Style="{StaticResource Card}">
              <StackPanel>
                <Grid>
                  <TextBlock x:Name="VsCardTitle" Style="{StaticResource CardTitle}" VerticalAlignment="Center"/>
                  <Border HorizontalAlignment="Right" CornerRadius="11" Background="#1A3DDC97" BorderBrush="#553DDC97" BorderThickness="1" Padding="12,4">
                    <StackPanel Orientation="Horizontal"><Ellipse x:Name="OnlineDot" Width="8" Height="8" Fill="#FF3DDC97" VerticalAlignment="Center" Margin="0,0,8,0"/>
                      <TextBlock x:Name="OnlineCount" Foreground="{StaticResource Ink}" FontSize="12"/></StackPanel></Border>
                </Grid>
                <TextBlock x:Name="VsCardHelp" Style="{StaticResource Help}" MaxWidth="600" HorizontalAlignment="Left"/>
                <Border x:Name="AccountBox" Visibility="Collapsed" CornerRadius="14" Background="#0DFFFFFF" BorderBrush="{StaticResource Edge}" BorderThickness="1" Padding="18,14,18,16" Margin="0,16,0,0">
                  <StackPanel>
                    <StackPanel x:Name="SignedOutPanel">
                      <TextBlock x:Name="AccTitle" Foreground="{StaticResource Ink}" FontSize="14" FontWeight="SemiBold"/>
                      <TextBlock x:Name="AccHelp" Style="{StaticResource Help}" MaxWidth="560" HorizontalAlignment="Left" Margin="0,4,0,0"/>
                      <WrapPanel Margin="0,12,0,0">
                        <StackPanel Margin="0,0,12,0"><TextBlock x:Name="EmailLabel" Style="{StaticResource Label}" Margin="0,0,0,6"/>
                          <TextBox x:Name="EmailBox" Style="{StaticResource Field}" Width="250"/></StackPanel>
                        <StackPanel Margin="0,0,12,0"><TextBlock x:Name="PassLabel" Style="{StaticResource Label}" Margin="0,0,0,6"/>
                          <PasswordBox x:Name="PassBox" Style="{StaticResource PwField}" Width="200"/></StackPanel>
                      </WrapPanel>
                      <StackPanel Orientation="Horizontal" Margin="0,14,0,0">
                        <Button x:Name="SignInBtn" Style="{StaticResource AccentBtn}"/>
                        <Button x:Name="SignUpBtn" Style="{StaticResource Soft}" Margin="0"/>
                        <Button x:Name="ForgotBtn" Style="{StaticResource Soft}" Margin="10,0,0,0" Background="Transparent" BorderThickness="0"/>
                      </StackPanel>
                    </StackPanel>
                    <Grid x:Name="SignedInPanel" Visibility="Collapsed">
                      <StackPanel Orientation="Horizontal" VerticalAlignment="Center">
                        <Border x:Name="MeAvatar" Width="34" Height="34" CornerRadius="17" Background="{StaticResource Accent}"><TextBlock x:Name="MeInitial" Foreground="White" FontSize="15" FontWeight="SemiBold" HorizontalAlignment="Center" VerticalAlignment="Center"/></Border>
                        <TextBlock x:Name="MeText" Foreground="{StaticResource Ink}" FontSize="14" VerticalAlignment="Center" Margin="12,0,0,0"/>
                      </StackPanel>
                      <Button x:Name="SignOutBtn" Style="{StaticResource Soft}" HorizontalAlignment="Right" Margin="0"/>
                    </Grid>
                  </StackPanel>
                </Border>
                <WrapPanel Margin="0,10,0,0">
                  <TextBlock x:Name="NetStatus" Style="{StaticResource Help}" Foreground="#FFB9C2FF" MaxWidth="520" VerticalAlignment="Center" Margin="0,0,12,0" Visibility="Collapsed"/>
                  <Button x:Name="CancelInviteBtn" Style="{StaticResource Soft}" Visibility="Collapsed" Margin="0"/>
                </WrapPanel>
                <Grid Margin="0,4,0,0">
                  <Grid.ColumnDefinitions><ColumnDefinition Width="Auto"/><ColumnDefinition Width="36"/><ColumnDefinition Width="*"/></Grid.ColumnDefinitions>
                  <StackPanel>
                    <TextBlock x:Name="NetcodeLabel" Style="{StaticResource Label}"/>
                    <WrapPanel x:Name="NetcodeChips"/>
                  </StackPanel>
                  <StackPanel Grid.Column="2">
                    <TextBlock x:Name="VsDelayLabel" Style="{StaticResource Label}"/>
                    <WrapPanel x:Name="VsDelayChips"/>
                  </StackPanel>
                </Grid>
                <TextBlock x:Name="NetcodeHelp" Style="{StaticResource Help}" MaxWidth="600" HorizontalAlignment="Left" Margin="0,0,0,0"/>

                <StackPanel Orientation="Horizontal" Margin="0,22,0,0">
                  <Button x:Name="QuickBtn" Style="{StaticResource AccentBtn}"/>
                  <TextBlock x:Name="QuickHelp" Style="{StaticResource Help}" VerticalAlignment="Center" Margin="4,0,0,0" MaxWidth="380"/>
                </StackPanel>

                <StackPanel x:Name="InvitesBox" Visibility="Collapsed">
                  <TextBlock x:Name="InvitesLabel" Style="{StaticResource Section}"/>
                  <StackPanel x:Name="InvitesList"/>
                </StackPanel>

                <StackPanel x:Name="FriendsBox" Visibility="Collapsed">
                  <TextBlock x:Name="FriendsLabel" Style="{StaticResource Section}"/>
                  <StackPanel x:Name="FriendsList"/>
                  <TextBlock x:Name="FriendsEmpty" Style="{StaticResource Help}" MaxWidth="600" HorizontalAlignment="Left" Margin="0,0,0,8"/>
                  <StackPanel Orientation="Horizontal" Margin="0,4,0,0">
                    <TextBox x:Name="FriendBox" Style="{StaticResource Field}" Width="250" MaxLength="20"/>
                    <Button x:Name="FriendAddBtn" Style="{StaticResource Soft}" Margin="10,0,0,0"/>
                  </StackPanel>
                </StackPanel>

                <TextBlock x:Name="PlayersLabel" Style="{StaticResource Section}"/>
                <StackPanel x:Name="PlayersList"/>
                <TextBlock x:Name="PlayersEmpty" Style="{StaticResource Help}" MaxWidth="600" HorizontalAlignment="Left" Margin="0,0,0,0"/>

                <Button x:Name="AdvToggle" Style="{StaticResource Soft}" HorizontalAlignment="Left" Margin="0,24,0,0"/>
                <StackPanel x:Name="AdvPanel" Visibility="Collapsed">
                  <TextBlock x:Name="VsTitle" Style="{StaticResource Help}" MaxWidth="600" HorizontalAlignment="Left" Margin="0,4,0,10"/>
                  <WrapPanel x:Name="VsChips"/>
                  <TextBlock x:Name="VsMine" Style="{StaticResource Label}" Foreground="{StaticResource Ink}" FontSize="16" FontWeight="SemiBold" MaxWidth="560" HorizontalAlignment="Left"/>
                  <WrapPanel x:Name="VsCodeRow" Margin="0,6,0,0"><Button x:Name="VsCopy" Style="{StaticResource Soft}"/><Button x:Name="VsNewCode" Style="{StaticResource Soft}"/></WrapPanel>
                  <TextBlock x:Name="VsPeerLabel" Style="{StaticResource Label}"/>
                  <TextBox x:Name="VsPeerBox" Style="{StaticResource Field}" Width="360" HorizontalAlignment="Left"/>
                  <TextBlock x:Name="VsPlayersLabel" Style="{StaticResource Label}"/>
                  <WrapPanel x:Name="VsPlayersChips"/>
                  <WrapPanel x:Name="VsSaveRow" Margin="0,18,0,0"><Button x:Name="VsSaveMake" Style="{StaticResource Soft}"/><Button x:Name="VsSaveUse" Style="{StaticResource Soft}"/></WrapPanel>
                  <TextBlock x:Name="VsSaveState" Style="{StaticResource Help}" MaxWidth="560" HorizontalAlignment="Left"/>
                  <TextBlock x:Name="VsHelp" Style="{StaticResource Help}" MaxWidth="560" HorizontalAlignment="Left"/>
                </StackPanel>
              </StackPanel>
            </Border>
          </StackPanel>
          </ScrollViewer>

          <ScrollViewer x:Name="PagePads" Visibility="Collapsed" VerticalScrollBarVisibility="Auto" Padding="0,0,14,0">
          <StackPanel>
            <TextBlock x:Name="PadsTitle" Style="{StaticResource Title}"/>
            <WrapPanel x:Name="PadTabs" Margin="0,14,0,0"/>
            <StackPanel x:Name="KbSection">
              <TextBlock x:Name="KbLabel" Style="{StaticResource Label}"/>
              <WrapPanel x:Name="KbChips" MaxWidth="640" HorizontalAlignment="Left"/>
              <CheckBox x:Name="BackgroundBox" Style="{StaticResource Switch}" Margin="0,18,0,0"/>
              <TextBlock x:Name="KeysTitle" Style="{StaticResource Label}" Margin="0,26,0,2" Foreground="{StaticResource Ink}" FontSize="17" FontWeight="SemiBold"/>
              <TextBlock x:Name="KeysHelp" Style="{StaticResource Help}" MaxWidth="640" HorizontalAlignment="Left" Margin="0,0,0,12"/>
              <WrapPanel x:Name="KeyGrid" MaxWidth="660" HorizontalAlignment="Left"/>
              <TextBlock x:Name="KeysNote" Foreground="#FFFFB86B" FontSize="13" Margin="0,2,0,0"/>
              <WrapPanel Margin="0,14,0,0">
                <Button x:Name="ResetKeys" Style="{StaticResource Soft}"/>
                <Button x:Name="OpenPads" Style="{StaticResource Soft}"/>
              </WrapPanel>
            </StackPanel>
            <StackPanel x:Name="PadSection" Visibility="Collapsed">
              <TextBlock x:Name="PadPickLabel" Style="{StaticResource Label}"/>
              <WrapPanel x:Name="PadPickChips" MaxWidth="660" HorizontalAlignment="Left"/>
              <TextBlock x:Name="PadNone" Style="{StaticResource Help}" MaxWidth="600" HorizontalAlignment="Left"/>
              <StackPanel x:Name="PadBody" Visibility="Collapsed">
                <TextBlock x:Name="PadWhere" Style="{StaticResource Help}" MaxWidth="640" HorizontalAlignment="Left"/>
                <TextBlock x:Name="PadStyleLabel" Style="{StaticResource Label}"/>
                <WrapPanel x:Name="PadStyleChips" MaxWidth="660" HorizontalAlignment="Left"/>
                <StackPanel x:Name="PadLayoutBox">
                  <TextBlock x:Name="PadLayoutLabel" Style="{StaticResource Label}"/>
                  <WrapPanel x:Name="PadLayoutChips" MaxWidth="660" HorizontalAlignment="Left"/>
                </StackPanel>
                <TextBlock x:Name="PadDeadLabel" Style="{StaticResource Label}"/>
                <WrapPanel x:Name="PadDeadChips" MaxWidth="660" HorizontalAlignment="Left"/>
                <TextBlock x:Name="PadRumbleLabel" Style="{StaticResource Label}"/>
                <WrapPanel x:Name="PadRumbleChips" MaxWidth="660" HorizontalAlignment="Left"/>
                <TextBlock x:Name="PadTrigLabel" Style="{StaticResource Label}"/>
                <WrapPanel x:Name="PadTrigChips" MaxWidth="660" HorizontalAlignment="Left"/>
                <TextBlock x:Name="PadMapTitle" Style="{StaticResource Label}" Margin="0,26,0,2" Foreground="{StaticResource Ink}" FontSize="17" FontWeight="SemiBold"/>
                <TextBlock x:Name="PadMapHelp" Style="{StaticResource Help}" MaxWidth="640" HorizontalAlignment="Left" Margin="0,0,0,12"/>
                <WrapPanel x:Name="PadMapGrid" MaxWidth="660" HorizontalAlignment="Left"/>
                <TextBlock x:Name="PadMapNote" Foreground="#FFFFB86B" FontSize="13" Margin="0,2,0,0"/>
                <WrapPanel Margin="0,14,0,0">
                  <Button x:Name="PadLetters" Style="{StaticResource Soft}"/>
                  <Button x:Name="ResetPad" Style="{StaticResource Soft}"/>
                </WrapPanel>
              </StackPanel>
            </StackPanel>
            <TextBlock x:Name="PadsHelp" Style="{StaticResource Help}" MaxWidth="600" HorizontalAlignment="Left" Margin="0,8,0,0"/>
          </StackPanel>
          </ScrollViewer>

          <ScrollViewer x:Name="PageVideo" Visibility="Collapsed" VerticalScrollBarVisibility="Auto" Padding="0,0,14,0">
          <StackPanel>
            <TextBlock x:Name="VideoTitle" Style="{StaticResource Title}"/>
            <TextBlock x:Name="WindowLabel" Style="{StaticResource Label}"/>
            <WrapPanel x:Name="WindowChips"/>
            <TextBlock x:Name="SizeLabel" Style="{StaticResource Label}" Margin="0,8,0,8"/>
            <StackPanel Orientation="Horizontal"><TextBox x:Name="WidthBox" Style="{StaticResource Field}" Width="110" MaxLength="4"/>
              <TextBlock Text="×" Foreground="{StaticResource Muted}" FontSize="18" Margin="14,0" VerticalAlignment="Center"/>
              <TextBox x:Name="HeightBox" Style="{StaticResource Field}" Width="110" MaxLength="4"/></StackPanel>
            <TextBlock x:Name="PacingLabel" Style="{StaticResource Label}"/>
            <WrapPanel x:Name="PacingChips"/>
            <TextBlock x:Name="FpsHelp" Style="{StaticResource Help}" MaxWidth="640" HorizontalAlignment="Left" Margin="0,8,0,0"/>
            <CheckBox x:Name="VsyncBox" Style="{StaticResource Switch}" Margin="0,16,0,0"/>
            <TextBlock x:Name="VsyncHelp" Style="{StaticResource Help}" MaxWidth="640" HorizontalAlignment="Left" Margin="40,0,0,0"/>
            <CheckBox x:Name="FskipBox" Style="{StaticResource Switch}" Margin="0,16,0,0"/>
            <TextBlock x:Name="FskipHelp" Style="{StaticResource Help}" MaxWidth="640" HorizontalAlignment="Left" Margin="40,0,0,0"/>
            <TextBlock x:Name="GpuLabel" Style="{StaticResource Label}" Margin="0,18,0,8"/>
            <WrapPanel x:Name="GpuChips" MaxWidth="660" HorizontalAlignment="Left"/>
            <TextBlock x:Name="TexLabel" Style="{StaticResource Label}" Margin="0,8,0,8"/>
            <WrapPanel x:Name="TexChips" MaxWidth="660" HorizontalAlignment="Left"/>
            <TextBlock x:Name="TexHelp" Style="{StaticResource Help}" MaxWidth="620" HorizontalAlignment="Left" Margin="0,2,0,0"/>
            <TextBlock x:Name="UpLabel" Style="{StaticResource Label}" Margin="0,8,0,8"/>
            <WrapPanel x:Name="UpChips" MaxWidth="660" HorizontalAlignment="Left"/>
            <TextBlock x:Name="SharpLabel" Style="{StaticResource Label}" Margin="0,8,0,8"/>
            <WrapPanel x:Name="SharpChips" MaxWidth="660" HorizontalAlignment="Left"/>
            <TextBlock x:Name="UpHelp" Style="{StaticResource Help}" MaxWidth="620" HorizontalAlignment="Left" Margin="0,2,0,0"/>
            <CheckBox x:Name="CursorBox" Style="{StaticResource Switch}" Margin="0,22,0,0"/>
            <CheckBox x:Name="OverlayBox" Style="{StaticResource Switch}"/>
          </StackPanel>
          </ScrollViewer>

          <StackPanel x:Name="PageShaders" Visibility="Collapsed">
            <TextBlock x:Name="ShadersTitle" Style="{StaticResource Title}"/>
            <TextBlock x:Name="ShadersHelp" Style="{StaticResource Help}" MaxWidth="620" HorizontalAlignment="Left" FontSize="14"/>
            <Border CornerRadius="18" Background="{StaticResource Veil}" BorderBrush="{StaticResource Edge}" BorderThickness="1" Padding="26,22" Margin="0,26,0,0" HorizontalAlignment="Left">
              <StackPanel><TextBlock x:Name="ShaderNumber" Foreground="{StaticResource Ink}" FontSize="44" FontWeight="Light"/>
                <TextBlock x:Name="ShaderCaption" Foreground="{StaticResource Muted}" FontSize="14" Margin="0,2,0,0"/></StackPanel></Border>
            <Button x:Name="PrepareBtn" Style="{StaticResource Soft}" HorizontalAlignment="Left" Margin="0,26,0,0"/>
          </StackPanel>

          <StackPanel x:Name="PageGame" Visibility="Collapsed">
            <TextBlock x:Name="GameTitle" Style="{StaticResource Title}"/>
            <TextBlock x:Name="GameVersionLabel" Style="{StaticResource Label}"/>
            <WrapPanel x:Name="VersionChips" MaxWidth="660" HorizontalAlignment="Left"/>
            <TextBlock x:Name="NoVersions" Style="{StaticResource Help}"/>
            <TextBlock x:Name="ToolsLabel" Style="{StaticResource Label}" Margin="0,22,0,8"/>
            <WrapPanel MaxWidth="700" HorizontalAlignment="Left">
              <Button x:Name="GameInstall" Style="{StaticResource Soft}"/><Button x:Name="UnlockBtn" Style="{StaticResource Soft}"/>
              <Button x:Name="FolderBtn" Style="{StaticResource Soft}"/><Button x:Name="ReportsBtn" Style="{StaticResource Soft}"/><Button x:Name="LogBtn" Style="{StaticResource Soft}"/>
            </WrapPanel>
            <TextBlock x:Name="UpdTitle" Style="{StaticResource Label}" Margin="0,22,0,6"/>
            <TextBlock x:Name="UpdStatus" Style="{StaticResource Help}" MaxWidth="660" HorizontalAlignment="Left" Foreground="{StaticResource Ink}" Margin="0,0,0,2"/>
            <TextBlock x:Name="UpdDetail" Style="{StaticResource Help}" MaxWidth="660" HorizontalAlignment="Left" Margin="0,0,0,10"/>
            <WrapPanel><Button x:Name="UpdCheckBtn" Style="{StaticResource Soft}"/><Button x:Name="UpdNowBtn" Style="{StaticResource Soft}" Visibility="Collapsed"/></WrapPanel>
          </StackPanel>

          <!-- while the game starts -->
          <Grid x:Name="Starting" Visibility="Collapsed" Panel.ZIndex="9" Background="#E6080A15">
            <StackPanel VerticalAlignment="Center" HorizontalAlignment="Center">
              <TextBlock x:Name="StartingText" Foreground="{StaticResource Ink}" FontSize="26" FontWeight="SemiBold" HorizontalAlignment="Center"/>
              <ProgressBar x:Name="Bar" IsIndeterminate="True" Width="280" Height="4" Margin="0,26,0,0" Foreground="#FF8E9BFF" Background="#22FFFFFF" BorderThickness="0"/>
            </StackPanel>
          </Grid>
        </Grid>
      </Grid>
    </Grid>
  </Border>
</Window>
'@

$win = [Windows.Markup.XamlReader]::Parse($xaml)
# the window's own icon (taskbar and Alt+Tab)
try {
    $iconPng = Join-Path (Split-Path -Parent $PSScriptRoot) 'docs\assets\nativesmash-512.png'
    if (Test-Path -LiteralPath $iconPng) {
        $iconBmp = New-Object Windows.Media.Imaging.BitmapImage
        $iconBmp.BeginInit(); $iconBmp.UriSource = New-Object Uri($iconPng); $iconBmp.CacheOption = 'OnLoad'; $iconBmp.EndInit()
        $win.Icon = $iconBmp
    }
} catch { }
function Find([string]$Name) { return $win.FindName($Name) }
function Say($Name, [string]$Key) { $el = Find $Name; if ($el -is [Windows.Controls.TextBlock]) { $el.Text = T $Key } else { $el.Content = T $Key } }

foreach ($p in @(@('NavPlay', 'play'), @('NavOnline', 'online'), @('NavPads', 'pads'), @('NavVideo', 'video'), @('NavShaders', 'shaders'), @('NavGame', 'game'),
                 @('Tagline', 'tagline'), @('PlayTitle', 'ready'), @('PlayHelp', 'readyHelp'), @('VersionLabel', 'version'), @('PlayBtn', 'playBtn'),
                 @('InstallBtn', 'install'), @('DirectBox', 'direct'), @('OnlineTitle', 'onlineTitle'), @('NameLabel', 'name'), @('LobbyLabel', 'lobby'),
                 @('CodeLabel', 'code'), @('ServerLabel', 'server'), @('DirectConn', 'directConn'), @('DirectHelp', 'directHelp'), @('ReportBox', 'report'), @('ReportHelp', 'reportHelp'), @('NetcodeLabel', 'netcodeLabel'), @('LwTitle', 'lwTitle'), @('LwHelp', 'lwHelp'), @('VsCardTitle', 'vsCardTitle'), @('VsCardHelp', 'vsCardHelp'), @('QuickBtn', 'quick'), @('QuickHelp', 'quickHelp'), @('InvitesLabel', 'invitesLabel'), @('PlayersLabel', 'playersLabel'), @('PlayersEmpty', 'playersEmpty'), @('AccTitle', 'accTitle'), @('AccHelp', 'accHelp'), @('EmailLabel', 'emailLabel'), @('PassLabel', 'passLabel'), @('SignInBtn', 'signIn'), @('SignUpBtn', 'signUp'), @('SignOutBtn', 'signOut'), @('ForgotBtn', 'forgot'), @('CancelInviteBtn', 'cancelInvite'), @('FriendsLabel', 'friendsLabel'), @('FriendsEmpty', 'friendsEmpty'), @('FriendAddBtn', 'friendAdd'), @('ResultHelp', 'resultHelp'), @('ResWinBtn', 'resWin'), @('ResLossBtn', 'resLoss'), @('ResNoneBtn', 'resNone'), @('NetcodeHelp', 'netcodeHelp'), @('DelayLabel', 'delayLabel'), @('VsTitle', 'vsTitle'), @('VsPeerLabel', 'vsPeer'), @('VsDelayLabel', 'vsDelay'), @('VsPlayersLabel', 'vsPlayers'), @('VsHelp', 'vsHelp'), @('VsSaveMake', 'vsSaveMake'), @('VsSaveUse', 'vsSaveUse'), @('VsCopy', 'vsCopy'), @('VsNewCode', 'vsNewCode'), @('PadsTitle', 'padsTitle'),
                 @('KbLabel', 'kbPlays'), @('BackgroundBox', 'background'), @('PadsHelp', 'padsHelp'), @('OpenPads', 'openPads'), @('KeysTitle', 'keysTitle'), @('UpdTitle', 'updTitle'), @('UpdCheckBtn', 'updCheck'), @('UpdNowBtn', 'updNow'), @('KeysHelp', 'keysHelp'), @('ResetKeys', 'resetKeys'), @('PadPickLabel', 'padPick'), @('PadNone', 'padNone'), @('PadStyleLabel', 'padStyle'), @('PadLayoutLabel', 'padLayout'), @('PadDeadLabel', 'padDead'), @('PadRumbleLabel', 'padRumble'), @('PadTrigLabel', 'padTrig'), @('PadMapTitle', 'padMapTitle'), @('PadMapHelp', 'padMapHelp'), @('ResetPad', 'resetPad'), @('PadLetters', 'padLetters'), @('VideoTitle', 'videoTitle'),
                 @('WindowLabel', 'windowMode'), @('SizeLabel', 'size'), @('PacingLabel', 'pacing'), @('GpuLabel', 'gpu'), @('UpLabel', 'upLabel'), @('TexLabel', 'texLabel'), @('FskipBox', 'fskipBox'), @('FskipHelp', 'fskipHelp'), @('TexHelp', 'texHelp'), @('FpsHelp', 'fpsHelp'), @('VsyncBox', 'vsyncBox'), @('VsyncHelp', 'vsyncHelp'), @('SharpLabel', 'sharpLabel'), @('UpHelp', 'upHelp'), @('CursorBox', 'hideCursor'),
                 @('OverlayBox', 'overlay'), @('ShadersTitle', 'shadersTitle'), @('ShadersHelp', 'shadersHelp'), @('PrepareBtn', 'prepare'), @('GameTitle', 'gameTitle'),
                 @('GameVersionLabel', 'version'), @('NoVersions', 'noVersions'), @('ToolsLabel', 'tools'), @('GameInstall', 'install'), @('UnlockBtn', 'unlock'),
                 @('FolderBtn', 'openFolder'), @('ReportsBtn', 'openReports'), @('LogBtn', 'openLog'), @('StartingText', 'starting'))) { Say $p[0] $p[1] }

# ---- groups of chips ---------------------------------------------------------------------------
function New-Chips($Panel, $Items, [string]$Selected) {
    <# Fills a WrapPanel with one RadioButton per @{Value; Text}; the one whose Value is $Selected is checked. #>
    $Panel.Children.Clear()
    $group = [guid]::NewGuid().ToString()
    $match = $false
    foreach ($it in $Items) { if ([string]::Equals($it.Value, $Selected, [StringComparison]::OrdinalIgnoreCase)) { $match = $true } }
    $first = $true
    foreach ($it in $Items) {
        $rb = New-Object Windows.Controls.RadioButton
        $rb.Style = $win.FindResource('Chip')
        $rb.Content = $it.Text
        $rb.Tag = $it.Value
        $rb.GroupName = $group
        if ([string]::Equals($it.Value, $Selected, [StringComparison]::OrdinalIgnoreCase) -or (-not $match -and $first)) { $rb.IsChecked = $true }
        $first = $false
        [void]$Panel.Children.Add($rb)
    }
}
function Get-Chip($Panel) { foreach ($c in $Panel.Children) { if ($c.IsChecked) { return [string]$c.Tag } } return '' }

New-Chips (Find 'LobbyChips') @(
    @{ Value = 'public'; Text = (T 'lobPublic') }, @{ Value = 'private'; Text = (T 'lobPrivate') },
    @{ Value = 'custom'; Text = (T 'lobCustom') }, @{ Value = 'offline'; Text = (T 'lobOff') }) $netMode
New-Chips (Find 'KbChips') (@(1..8 | ForEach-Object { @{ Value = "$_"; Text = "$_" } }) + @(@{ Value = '0'; Text = (T 'nobody') })) ([string](Get-Number (Get-Setting $padsIni 'general' 'keyboard_player')))
New-Chips (Find 'WindowChips') @(
    @{ Value = 'windowed'; Text = (T 'winWindowed') }, @{ Value = 'borderless'; Text = (T 'winBorderless') },
    @{ Value = 'fullscreen'; Text = (T 'winFull') }) (Get-Setting $displayIni '' 'window')
New-Chips (Find 'TexChips') @(
    @{ Value = 'auto'; Text = (T 'texAuto') }, @{ Value = 'high'; Text = (T 'texHigh') },
    @{ Value = 'medium'; Text = (T 'texMed') }, @{ Value = 'low'; Text = (T 'texLow') }) (Get-Setting $displayIni '' 'texture_quality')
New-Chips (Find 'PacingChips') @(
    @{ Value = 'clock'; Text = (T 'pacClock') }, @{ Value = 'vrr'; Text = (T 'pacVrr') },
    @{ Value = 'display'; Text = (T 'pacDisplay') }) (Get-Setting $displayIni '' 'pacing')

$gpu = Get-Setting $displayIni '' 'gpu'
$gpuItems = @(@{ Value = 'auto'; Text = (T 'gpuAuto') })
try {
    foreach ($n in @(Get-CimInstance Win32_VideoController -ErrorAction Stop | ForEach-Object { $_.Name } | Where-Object { $_ } | Select-Object -Unique)) { $gpuItems += @{ Value = $n; Text = $n } }
} catch { }
$gpuItems += @{ Value = 'warp'; Text = 'WARP (Windows)' }
if (-not ($gpuItems | Where-Object { [string]::Equals($_.Value, $gpu, [StringComparison]::OrdinalIgnoreCase) })) { $gpuItems += @{ Value = $gpu; Text = $gpu } }
New-Chips (Find 'GpuChips') $gpuItems $gpu

# FSR 1: the modes are display.ini's upscale = fsr with a render_scale; the sharpness is its sharpness
$UpPresets = [ordered]@{ off = ''; quality = '0.77'; balanced = '0.67'; performance = '0.59'; ultra = '0.5' }
$script:upScaleWas = [string](Get-Setting $displayIni '' 'render_scale'); if (-not $script:upScaleWas) { $script:upScaleWas = '1' }
$script:upFsrWas = ([string](Get-Setting $displayIni '' 'upscale')).Trim().ToLower() -eq 'fsr'
$upNow = 'off'
if ($script:upFsrWas) {
    $x = 0.0; [void][double]::TryParse($script:upScaleWas, [Globalization.NumberStyles]::Float, [Globalization.CultureInfo]::InvariantCulture, [ref]$x)
    $best = 9.0
    foreach ($k in $UpPresets.Keys) { if ($k -eq 'off') { continue }; $d = [Math]::Abs([double]::Parse($UpPresets[$k], [Globalization.CultureInfo]::InvariantCulture) - $x); if ($d -lt $best) { $best = $d; $upNow = $k } }
}
New-Chips (Find 'UpChips') @(
    @{ Value = 'off'; Text = (T 'upOff') }, @{ Value = 'quality'; Text = (T 'upQuality') }, @{ Value = 'balanced'; Text = (T 'upBalanced') },
    @{ Value = 'performance'; Text = (T 'upPerf') }, @{ Value = 'ultra'; Text = (T 'upUltra') }) $upNow
$sh = [string](Get-Setting $displayIni '' 'sharpness'); if (-not $sh) { $sh = '0.8' }
$shNow = '0.8'; $shBest = 9.0; $shx = 0.8; [void][double]::TryParse($sh, [Globalization.NumberStyles]::Float, [Globalization.CultureInfo]::InvariantCulture, [ref]$shx)
foreach ($c in '0', '0.5', '0.8', '1') { $d = [Math]::Abs([double]::Parse($c, [Globalization.CultureInfo]::InvariantCulture) - $shx); if ($d -lt $shBest) { $shBest = $d; $shNow = $c } }
New-Chips (Find 'SharpChips') @(
    @{ Value = '0'; Text = (T 'shNone') }, @{ Value = '0.5'; Text = (T 'shSoft') }, @{ Value = '0.8'; Text = (T 'shNormal') }, @{ Value = '1'; Text = (T 'shSharp') }) $shNow

(Find 'NameBox').Text = [string](Get-LineValue $nameFile)
(Find 'CodeBox').Text = $lobbyCode
(Find 'ServerBox').Text = if ($netMode -eq 'custom') { $lobbyHost } else { '' }
(Find 'DirectConn').IsChecked = -not ((Get-LineValue $directFile) -match '^(?i)off')
(Find 'ReportBox').IsChecked = -not ((Get-LineValue $reportFile) -match '^(?i)off')
$delayNow = '3'; $delayText = ([string](Get-LineValue $delayFile)).Trim().ToLower()
if ($delayText -match '^auto') { $delayNow = 'auto' } elseif ($delayText -match '^\d+$') {
    $digits = $delayText.TrimStart('0')   # ([int] of a very long number throws; the game clamps such a number to 25 as well)
    $delayNow = if ($digits.Length -gt 2) { '25' } else { [string][Math]::Min(25, $(if ($digits) { [int]$digits } else { 0 })) }
}
$delayItems = @(@{ Value = 'auto'; Text = (T 'delayAuto') }); foreach ($n in 0..8) { $delayItems += @{ Value = [string]$n; Text = [string]$n } }
if ($delayNow -notin ($delayItems | ForEach-Object { $_.Value })) { $delayItems += @{ Value = $delayNow; Text = $delayNow } }
New-Chips (Find 'DelayChips') $delayItems $delayNow
(Find 'DelayChips').ToolTip = (T 'delayHelp')
$netcodeNow = if (([string](Get-LineValue $netcodeFile)).Trim() -match '^(?i)rollback') { 'rollback' } else { 'normal' }
New-Chips (Find 'NetcodeChips') @(@{ Value = 'normal'; Text = (T 'netcodeNormal') }, @{ Value = 'rollback'; Text = (T 'netcodeRollback') }) $netcodeNow

# ---- the VS netplay (vs_netplay.txt; the game gets it as SSBU_NETPLAY* variables when Play starts it) ----
$vsWords = @(([string](Get-LineValue $vsFile)).Trim() -split '\s+' | Where-Object { $_ })
$vsMode = if ($vsWords.Count -and $vsWords[0] -in @('host', 'join')) { $vsWords[0] } else { 'off' }
$vsPeer = if ($vsMode -eq 'join' -and $vsWords.Count -gt 1) { $vsWords[1] } else { '' }
function New-RoomCode { $abc = 'ABCDEFGHJKMNPQRSTUVWXYZ23456789'; -join (1..6 | ForEach-Object { $abc[(Get-Random -Maximum $abc.Length)] }) }
$script:vsCode = if ($vsMode -eq 'host' -and $vsWords.Count -gt 2 -and $vsWords[2] -match '^[A-Za-z0-9]{4,12}$') { $vsWords[2].ToUpper() } else { New-RoomCode }
$vsDelay = if ($vsMode -eq 'host' -and $vsWords.Count -gt 1) { $vsWords[1] } elseif ($vsMode -eq 'join' -and $vsWords.Count -gt 2) { $vsWords[2] } else { 'auto' }
if ($vsDelay -notmatch '^([0-8]|auto)$') { $vsDelay = 'auto' }
$vsPlayers = if ($vsMode -eq 'host' -and $vsWords.Count -gt 3 -and $vsWords[3] -match '^[2-4]$') { $vsWords[3] } else { '2' }
New-Chips (Find 'VsChips') @(@{ Value = 'off'; Text = (T 'vsOff') }, @{ Value = 'host'; Text = (T 'vsHost') }, @{ Value = 'join'; Text = (T 'vsJoin') }) $vsMode
New-Chips (Find 'VsDelayChips') (@(@{ Value = 'auto'; Text = (T 'vsAuto') }) + @(1..8 | ForEach-Object { @{ Value = "$_"; Text = "$_" } })) $vsDelay
(Find 'VsDelayChips').ToolTip = (T 'vsDelayHelp'); (Find 'VsDelayLabel').ToolTip = (T 'vsDelayHelp')
New-Chips (Find 'VsPlayersChips') @(2..4 | ForEach-Object { @{ Value = "$_"; Text = "$_" } }) $vsPlayers
(Find 'VsPeerBox').Text = $vsPeer
function Show-VsChoice {
    $m = Get-Chip (Find 'VsChips')
    $join = if ($m -eq 'join') { 'Visible' } else { 'Collapsed' }
    (Find 'VsPeerLabel').Visibility = $join; (Find 'VsPeerBox').Visibility = $join
    $on = if ($m -ne 'off') { 'Visible' } else { 'Collapsed' }
    (Find 'VsHelp').Visibility = $on
    (Find 'VsMine').Visibility = if ($m -eq 'host') { 'Visible' } else { 'Collapsed' }
    (Find 'VsCodeRow').Visibility = if ($m -eq 'host') { 'Visible' } else { 'Collapsed' }
    (Find 'VsPlayersLabel').Visibility = if ($m -eq 'host') { 'Visible' } else { 'Collapsed' }
    (Find 'VsPlayersChips').Visibility = if ($m -eq 'host') { 'Visible' } else { 'Collapsed' }
    (Find 'VsSaveRow').Visibility = $on; (Find 'VsSaveState').Visibility = $on
    (Find 'VsSaveMake').Visibility = if ($m -eq 'host') { 'Visible' } else { 'Collapsed' }
    (Find 'VsSaveUse').Visibility = if ($m -eq 'join') { 'Visible' } else { 'Collapsed' }
    if (-not $script:vsSaveNote) { (Find 'VsSaveState').Text = if (Test-NetplaySave) { T 'vsSaveReady' } else { T 'vsSaveNone' } }
    (Find 'VsMine').Text = (T 'vsCode') -f $script:vsCode
}
foreach ($c in (Find 'VsChips').Children) { $c.Add_Checked({ Show-VsChoice }) }

# ---- players online and invitations (NativeSmash's players server: on its way; until then the list is empty) ----
$script:players = @(); $script:invites = @(); $script:friends = @(); $script:onlineTotal = -1
function New-Avatar([string]$Name) {
    $hues = @('#FF7C6CFF', '#FFA66BFF', '#FF3DB6DC', '#FF3DDC97', '#FFE0A43D', '#FFE5617A')
    $b = New-Object Windows.Controls.Border
    $b.Width = 38; $b.Height = 38; $b.CornerRadius = 19
    $b.Background = (New-Object Windows.Media.BrushConverter).ConvertFromString($hues[[Math]::Abs($Name.GetHashCode()) % $hues.Count])
    $t = New-Object Windows.Controls.TextBlock
    $t.Text = if ($Name) { $Name.Substring(0, 1).ToUpper() } else { '?' }
    $t.Foreground = [Windows.Media.Brushes]::White; $t.FontSize = 16; $t.FontWeight = 'SemiBold'
    $t.HorizontalAlignment = 'Center'; $t.VerticalAlignment = 'Center'
    $b.Child = $t
    return $b
}
function New-PersonRow([string]$Name, [string]$Status, [string]$Ping, $Buttons) {
    $row = New-Object Windows.Controls.Border
    $row.CornerRadius = 14; $row.Padding = '12,10,10,10'; $row.Margin = '0,0,0,8'
    $row.Background = $win.FindResource('Veil'); $row.BorderBrush = $win.FindResource('Edge'); $row.BorderThickness = 1
    $g = New-Object Windows.Controls.Grid
    foreach ($w in @('Auto', '*', 'Auto')) { $cd = New-Object Windows.Controls.ColumnDefinition; $cd.Width = (New-Object Windows.GridLengthConverter).ConvertFromString($w); $g.ColumnDefinitions.Add($cd) }
    $av = New-Avatar $Name; [Windows.Controls.Grid]::SetColumn($av, 0); [void]$g.Children.Add($av)
    $info = New-Object Windows.Controls.StackPanel; $info.Margin = '14,0,0,0'; $info.VerticalAlignment = 'Center'
    $n = New-Object Windows.Controls.TextBlock; $n.Text = $Name; $n.Foreground = $win.FindResource('Ink'); $n.FontSize = 15; $n.FontWeight = 'SemiBold'
    $st = New-Object Windows.Controls.TextBlock; $st.Text = if ($Ping) { "$Status  ·  $Ping" } else { $Status }; $st.Foreground = $win.FindResource('Muted'); $st.FontSize = 12; $st.Margin = '0,2,0,0'
    [void]$info.Children.Add($n); [void]$info.Children.Add($st)
    [Windows.Controls.Grid]::SetColumn($info, 1); [void]$g.Children.Add($info)
    $bp = New-Object Windows.Controls.StackPanel; $bp.Orientation = 'Horizontal'; $bp.VerticalAlignment = 'Center'
    foreach ($bt in $Buttons) { $bt.Margin = '8,0,0,0'; [void]$bp.Children.Add($bt) }
    [Windows.Controls.Grid]::SetColumn($bp, 2); [void]$g.Children.Add($bp)
    $row.Child = $g
    return $row
}
function New-RowButton([string]$Text, [string]$Style, [scriptblock]$OnClick) {
    $b = New-Object Windows.Controls.Button
    $b.Content = $Text; $b.Style = $win.FindResource($Style)
    if ($Style -eq 'AccentBtn') { $b.Padding = '0'; $b.FontSize = 13 }
    $b.Add_Click($OnClick)
    return $b
}
function Show-Soon { [void][Windows.MessageBox]::Show($win, (T 'quickSoon'), 'NativeSmash', 'OK', 'Information') }
function Show-People {
    (Find 'OnlineCount').Text = if ($script:onlineTotal -ge 0) { (T 'onlineCount') -f $script:onlineTotal } else { T 'onlineSoon' }
    (Find 'OnlineDot').Opacity = if ($script:onlineTotal -ge 0) { 1 } else { 0.35 }
    $list = Find 'PlayersList'; $list.Children.Clear()
    foreach ($p in $script:players) {
        $status = switch ($p.State) { 'playing' { T 'stPlaying' } 'searching' { T 'stSearching' } default { T 'stLauncher' } }
        $btn = New-RowButton (T 'invite') 'Soft' { Send-Invite $this.Tag }
        $btn.Tag = $p
        $btn.Margin = '0'; $btn.IsEnabled = $p.State -ne 'playing'
        [void]$list.Children.Add((New-PersonRow $p.Name $status $p.Ping @($btn)))
    }
    (Find 'PlayersEmpty').Visibility = if ($script:players.Count) { 'Collapsed' } else { 'Visible' }
    $inv = Find 'InvitesList'; $inv.Children.Clear()
    foreach ($i in $script:invites) {
        $ok = New-RowButton (T 'accept') 'AccentBtn' { Answer-Invite $this.Tag $true }; $ok.Tag = $i
        $no = New-RowButton (T 'decline') 'Soft' { Answer-Invite $this.Tag $false }; $no.Margin = '0'; $no.Tag = $i
        [void]$inv.Children.Add((New-PersonRow $i.Name ((T 'inviteFrom') -f $i.Name) $i.Ping @($ok, $no)))
    }
    (Find 'InvitesBox').Visibility = if ($script:invites.Count) { 'Visible' } else { 'Collapsed' }
    $fl = Find 'FriendsList'; $fl.Children.Clear()
    foreach ($f in $script:friends) {
        $buttons = @()
        switch ($f.Relation) {
            'friend' {
                $status = if ($f.State) { switch ($f.State) { 'playing' { T 'stPlaying' } 'searching' { T 'stSearching' } default { T 'stLauncher' } } } else { T 'friendOffline' }
                $b1 = New-RowButton (T 'invite') 'Soft' { Send-Invite $this.Tag }; $b1.Tag = $f; $b1.IsEnabled = ($f.State -and $f.State -ne 'playing')
                $b2 = New-RowButton (T 'friendRemove') 'Soft' { Request-Players @{ Op = 'friendRemove'; Name = $this.Tag.Name } }; $b2.Tag = $f; $b2.Margin = '0'
                $buttons = @($b1, $b2)
            }
            'incoming' {
                $status = T 'friendIncoming'
                $b1 = New-RowButton (T 'accept') 'AccentBtn' { Request-Players @{ Op = 'friendAnswer'; Name = $this.Tag.Name; Accept = $true } }; $b1.Tag = $f
                $b2 = New-RowButton (T 'decline') 'Soft' { Request-Players @{ Op = 'friendAnswer'; Name = $this.Tag.Name; Accept = $false } }; $b2.Tag = $f; $b2.Margin = '0'
                $buttons = @($b1, $b2)
            }
            default {
                $status = T 'friendOutgoing'
                $b2 = New-RowButton (T 'friendRemove') 'Soft' { Request-Players @{ Op = 'friendRemove'; Name = $this.Tag.Name } }; $b2.Tag = $f; $b2.Margin = '0'
                $buttons = @($b2)
            }
        }
        [void]$fl.Children.Add((New-PersonRow $f.Name $status $(if ($null -ne $f.Rating) { "$($f.Rating) pts" } else { '' }) $buttons))
    }
    (Find 'FriendsEmpty').Visibility = if ($script:friends.Count) { 'Collapsed' } else { 'Visible' }
    (Find 'FriendsBox').Visibility = if ($NsNet -and $NsNet.SignedIn) { 'Visible' } else { 'Collapsed' }
}
function Show-Advanced([bool]$Open) {
    (Find 'AdvPanel').Visibility = if ($Open) { 'Visible' } else { 'Collapsed' }
    (Find 'AdvToggle').Content = if ($Open) { T 'advHide' } else { T 'advShow' }
}
(Find 'AdvToggle').Add_Click({ Show-Advanced ((Find 'AdvPanel').Visibility -ne 'Visible') })
# ---- the players server (tools\players_client.ps1): signing in, the list, invitations, quick match ----
. (Join-Path $PSScriptRoot 'players_client.ps1')
function Say-Net([string]$Text) { $s_ = Find 'NetStatus'; $s_.Text = $Text; $s_.Visibility = if ($Text) { 'Visible' } else { 'Collapsed' } }
function Show-Account {
    (Find 'AccountBox').Visibility = if ($NsNet.On) { 'Visible' } else { 'Collapsed' }
    (Find 'SignedOutPanel').Visibility = if ($NsNet.SignedIn) { 'Collapsed' } else { 'Visible' }
    (Find 'SignedInPanel').Visibility = if ($NsNet.SignedIn) { 'Visible' } else { 'Collapsed' }
    (Find 'MeText').Text = (T 'signedAs') -f $NsNet.Name
    (Find 'MeInitial').Text = if ($NsNet.Name) { $NsNet.Name.Substring(0, 1).ToUpper() } else { '?' }
    (Find 'QuickBtn').Content = if ($NsNet.State -eq 'searching') { T 'quickCancel' } else { T 'quick' }
}
function Send-Invite($P_) {
    if (-not $NsNet.On) { Show-Soon; return }
    if (-not $NsNet.SignedIn) { Say-Net (T 'needSignIn'); return }
    Request-Players @{ Op = 'invite'; To = $P_.Id; Name = $P_.Name; Netcode = (Get-Chip (Find 'NetcodeChips')) }
}
function Answer-Invite($I_, [bool]$Yes) {
    if (-not $NsNet.On) { Show-Soon; return }
    Request-Players @{ Op = 'answer'; Id = $I_.Id; Accept = $Yes; Name = $I_.Name }
}
function Start-FromServer($N) {
    # the room the server arranged: this launcher makes it (host) or joins it, with the invitation's netcode, and starts the game
    [void](Pick-Chip 'NetcodeChips' $(if ($N.Netcode -eq 'normal') { 'normal' } else { 'rollback' }))
    if ($N.Role -eq 'host') { [void](Pick-Chip 'VsChips' 'host'); $script:vsCode = $N.Code; [void](Pick-Chip 'VsPlayersChips' '2') }
    else { [void](Pick-Chip 'VsChips' 'join'); (Find 'VsPeerBox').Text = $N.Code }
    Show-VsChoice
    Say-Net ((T 'starting2') -f $N.With)
    (Find 'PlayBtn').RaiseEvent([Windows.RoutedEventArgs]::new([Windows.Controls.Primitives.ButtonBase]::ClickEvent))
}
function Pick-Chip([string]$Panel, [string]$Value) { foreach ($ch in (Find $Panel).Children) { if ([string]$ch.Tag -eq $Value) { $ch.IsChecked = $true; return $ch } } }
(Find 'QuickBtn').Add_Click({
    if (-not $NsNet.On) { Show-Soon; return }
    if (-not $NsNet.SignedIn) { Say-Net (T 'needSignIn'); return }
    if ($NsNet.State -eq 'searching') { Request-Players @{ Op = 'quickCancel' } } else { Request-Players @{ Op = 'quick'; Netcode = (Get-Chip (Find 'NetcodeChips')) } }
})
(Find 'SignInBtn').Add_Click({
    $mail = (Find 'EmailBox').Text.Trim(); $pw = (Find 'PassBox').Password
    if (-not $mail -or -not $pw) { Say-Net (T 'needBoth'); return }
    Say-Net (T 'signingIn')
    Request-Players @{ Op = 'signIn'; Email = $mail; Password = $pw }
    (Find 'PassBox').Password = ''
})
(Find 'SignUpBtn').Add_Click({ $u = if ($PlayersConfig -and $PlayersConfig.signup) { [string]$PlayersConfig.signup } else { '' }; if ($u -match '^https://') { Start-Process $u } })
(Find 'SignOutBtn').Add_Click({ Request-Players @{ Op = 'signOut' } })
(Find 'ForgotBtn').Add_Click({ $u = if ($PlayersConfig -and $PlayersConfig.signup) { [string]$PlayersConfig.signup } else { '' }; if ($u -match '^https://') { Start-Process ($u + '#olvide') } })
(Find 'CancelInviteBtn').Add_Click({ Request-Players @{ Op = 'cancelInvite' } })
(Find 'FriendAddBtn').Add_Click({ $nm = (Find 'FriendBox').Text.Trim(); if ($nm) { Request-Players @{ Op = 'friendAdd'; Name = $nm } } })
(Find 'FriendBox').Add_KeyDown({ param($s_, $e_) if ($e_.Key -eq 'Return') { $nm = (Find 'FriendBox').Text.Trim(); if ($nm) { Request-Players @{ Op = 'friendAdd'; Name = $nm } } } })
try {
    Add-Type -Namespace NativeSmash -Name Flash -MemberDefinition '[System.Runtime.InteropServices.DllImport("user32.dll")] public static extern bool FlashWindow(System.IntPtr hwnd, bool invert);' -ErrorAction Stop
} catch { }
function Ring-Invite([string]$From) {
    # a new invitation: a sound, the taskbar button flashing when the launcher is behind other windows, and the news in the status line
    try { [System.Media.SystemSounds]::Asterisk.Play() } catch { }
    try { if (-not $win.IsActive) { [void][NativeSmash.Flash]::FlashWindow((New-Object Windows.Interop.WindowInteropHelper $win).Handle, $true) } } catch { }
    Say-Net ((T 'invitedYou') -f $From)
}
function Show-Result($N) {
    # after a match the server arranged: the launcher, hidden while the game ran, comes back to ask how it went
    $script:resultMatch = $N.Id
    (Find 'ResultTitle').Text = (T 'resultTitle') -f $N.Opponent
    (Find 'ResultNote').Text = ''
    foreach ($b in 'ResWinBtn', 'ResLossBtn', 'ResNoneBtn') { (Find $b).IsEnabled = $true }
    (Find 'ResultPanel').Visibility = 'Visible'
    $win.BeginAnimation([Windows.Window]::OpacityProperty, $null); $win.Opacity = 1
    $win.Show(); $win.WindowState = 'Normal'; [void]$win.Activate()
}
foreach ($pair in @(@('ResWinBtn', 'win'), @('ResLossBtn', 'loss'), @('ResNoneBtn', 'none'))) {
    $btn_ = Find $pair[0]; $btn_.Tag = $pair[1]
    $btn_.Add_Click({
        foreach ($b in 'ResWinBtn', 'ResLossBtn', 'ResNoneBtn') { (Find $b).IsEnabled = $false }
        Request-Players @{ Op = 'report'; Id = $script:resultMatch; Result = [string]$this.Tag }
    })
}
# While the game runs the launcher stays signed in, hidden, as "playing": the others see it, and when the game closes it asks how the
# match went (if the server arranged one), then goes.
function Watch-Game {
    Request-Players @{ Op = 'state'; State = 'playing' }
    $script:gameWatch = New-Object Windows.Threading.DispatcherTimer
    $script:gameWatch.Interval = [TimeSpan]::FromSeconds(2)
    $script:gameWatch.Add_Tick({
        if (Get-Process -Name 'ssbu' -ErrorAction SilentlyContinue) { return }
        $script:gameWatch.Stop()
        $script:afterGame = $true
        Request-Players @{ Op = 'state'; State = 'launcher' }
        Request-Players @{ Op = 'openMatch' }
    })
    $script:gameWatch.Start()
}
$script:playersSeen = -1
$playersTimer = New-Object Windows.Threading.DispatcherTimer
$playersTimer.Interval = [TimeSpan]::FromMilliseconds(500)
$playersTimer.Add_Tick({
    if ($NsNet.Stamp -ne $script:playersSeen) {
        $script:playersSeen = $NsNet.Stamp
        $script:onlineTotal = $NsNet.Online
        $script:players = @($NsNet.List | ForEach-Object { @{ Id = $_.Id; Name = $_.Name; State = $_.State; Ping = $(if ($null -ne $_.Rating) { "$($_.Rating) pts" } else { '' }) } })
        $old = @($script:invites | ForEach-Object { $_.Id })
        $script:invites = @($NsNet.Invites | ForEach-Object { @{ Id = $_.Id; Name = $_.Name; Netcode = $_.Netcode; Ping = '' } })
        $script:friends = @($NsNet.Friends)
        Show-People
        $new = @($script:invites | Where-Object { $old -notcontains $_.Id })
        if ($new.Count) { Ring-Invite $new[0].Name }
    }
    if ($NsNet.State -eq 'searching' -and $script:searchSince) {
        $t = [DateTime]::Now - $script:searchSince
        Say-Net ((T 'searchingFor') -f ('{0}:{1:00}' -f [int][Math]::Floor($t.TotalMinutes), $t.Seconds), $NsNet.Searching)
    }
    $n = $null
    while ($NsNet.News.TryDequeue([ref]$n)) {
        switch ($n.Kind) {
            'signedIn' { Say-Net '' }
            'signedOut' { Say-Net '' }
            'searching' { $script:searchSince = [DateTime]::Now; Say-Net (T 'searching') }
            'quickStopped' { $script:searchSince = $null; Say-Net '' }
            'inviteSent' { Say-Net ((T 'inviteSent') -f $n.Name); (Find 'CancelInviteBtn').Visibility = 'Visible' }
            'inviteOver' { Say-Net ((T 'inviteOver') -f $n.Name); (Find 'CancelInviteBtn').Visibility = 'Collapsed' }
            'inviteCancelled' { Say-Net ((T 'inviteCancelled') -f $n.Name); (Find 'CancelInviteBtn').Visibility = 'Collapsed' }
            'friendSent' { Say-Net ((T 'friendSent') -f $n.Name); (Find 'FriendBox').Text = '' }
            'friendNow' { Say-Net ((T 'friendNow') -f $n.Name); (Find 'FriendBox').Text = '' }
            'openMatch' { Show-Result $n }
            'noMatch' { if ($script:afterGame) { $win.Close() } }
            'reported' {
                (Find 'ResultNote').Text = switch ($n.Result) { 'done' { T 'resDone' } 'disputed' { T 'resDisputed' } 'void' { T 'resVoid' } default { T 'resWaiting' } }
                $script:closeSoon = New-Object Windows.Threading.DispatcherTimer
                $script:closeSoon.Interval = [TimeSpan]::FromMilliseconds(2200)
                $script:closeSoon.Add_Tick({ $script:closeSoon.Stop(); $win.Close() })
                $script:closeSoon.Start()
            }
            'error' {
                $why = switch -Regex ([string]$n.Text) {
                    'Invalid login credentials' { T 'badLogin' }
                    'Email not confirmed' { T 'notConfirmed' }
                    'not online' { T 'notOnline' }
                    'no longer there' { T 'inviteGone' }
                    'too many' { T 'tooMany' }
                    'no such player' { T 'noSuchPlayer' }
                    'not yourself' { T 'noSuchPlayer' }
                    'Unable to connect|No es posible conectar|remote name|nombre remoto|timed out|tiempo de espera' { T 'noServer' }
                    default { (T 'netError') -f $n.Text }
                }
                Say-Net $why
            }
            'start' { $script:searchSince = $null; (Find 'CancelInviteBtn').Visibility = 'Collapsed'; Start-FromServer $n }
        }
        Show-Account
    }
})
$win.Add_Closed({ $playersTimer.Stop(); Stop-PlayersClient })
if (-not $env:NATIVESMASH_SHOTS) { Start-PlayersClient ([string](Get-LineValue (Join-Path $Root 'VERSION'))); $playersTimer.Start() }
Show-Account
Show-Advanced ($vsMode -ne 'off')
Show-People
(Find 'VsCopy').Add_Click({
    try { [Windows.Clipboard]::SetText($script:vsCode); (Find 'VsSaveState').Text = T 'vsCopied'; $script:vsSaveNote = $true } catch { }
})
(Find 'VsNewCode').Add_Click({ $script:vsCode = New-RoomCode; Show-VsChoice })
(Find 'VsSaveMake').Add_Click({
    try {
        $zip = Join-Path ([Environment]::GetFolderPath('Desktop')) 'savegame-netplay.zip'
        Export-NetplaySave $script:tree $zip
        $script:vsSaveNote = $true
        (Find 'VsSaveState').Text = (T 'vsSaveMade') -f $zip
    } catch { [void][Windows.MessageBox]::Show($win, "$(T 'vsSaveFail')$($_.Exception.Message)", 'NativeSmash', 'OK', 'Warning') }
})
(Find 'VsSaveUse').Add_Click({
    $dlg = New-Object Microsoft.Win32.OpenFileDialog
    $dlg.Filter = 'ZIP (*.zip)|*.zip'
    if (-not $dlg.ShowDialog($win)) { return }
    try {
        Import-NetplaySave $dlg.FileName
        $script:vsSaveNote = $false
        Show-VsChoice
    } catch { [void][Windows.MessageBox]::Show($win, "$(T 'vsSaveFail')$($_.Exception.Message)", 'NativeSmash', 'OK', 'Warning') }
})
Show-VsChoice
(Find 'BackgroundBox').IsChecked = (Get-Number (Get-Setting $padsIni 'general' 'background')) -ne 0
(Find 'VsyncBox').IsChecked = @('off', '0', 'no') -notcontains ([string](Get-Setting $displayIni '' 'vsync')).Trim().ToLower()
(Find 'FskipBox').IsChecked = @('on', '1', 'yes') -contains ([string](Get-Setting $displayIni '' 'frameskip')).Trim().ToLower()

# ---- the keyboard's keys (controllers.ini [keyboard]; the game's defaults when a key is not there) -----
$KeyActions = [ordered]@{
    a = 'Z'; b = 'X'; x = 'C'; y = 'V'; l = 'A'; r = 'S'; zl = 'Q'; zr = 'W'; plus = 'Return'; minus = 'Backspace'
    left = 'Left'; right = 'Right'; up = 'Up'; down = 'Down'; rs_left = 'J'; rs_right = 'L'; rs_up = 'I'; rs_down = 'K'
}
$KeyNames = @{   # WPF's Key -> the name controllers.ini spells
    Back = 'Backspace'; Return = 'Return'; Space = 'Space'; Tab = 'Tab'; Prior = 'PageUp'; Next = 'PageDown'; End = 'End'; Home = 'Home'
    Left = 'Left'; Up = 'Up'; Right = 'Right'; Down = 'Down'; Insert = 'Insert'; Delete = 'Delete'
    LeftShift = 'LShift'; RightShift = 'RShift'; LeftCtrl = 'LCtrl'; RightCtrl = 'RCtrl'
    OemSemicolon = 'Semicolon'; OemPlus = 'Equals'; OemComma = 'Comma'; OemMinus = 'Minus'; OemPeriod = 'Period'; OemQuestion = 'Slash'
    OemTilde = 'Grave'; OemOpenBrackets = 'LBracket'; OemPipe = 'Backslash'; OemCloseBrackets = 'RBracket'; OemQuotes = 'Quote'
}
function ConvertTo-KeyName($Key) {
    <# The name controllers.ini spells for a WPF key, '' for none, '!' for one that is the game's (Esc, F1 to F6). #>
    $n = [string]$Key
    if ($n -match '^[A-Z]$') { return $n }
    if ($n -match '^D([0-9])$') { return $Matches[1] }
    if ($n -match '^NumPad([0-9])$') { return "Numpad$($Matches[1])" }
    if ($n -match '^F([0-9]+)$') { if ([int]$Matches[1] -ge 7 -and [int]$Matches[1] -le 12 -and [int]$Matches[1] -ne 11) { return $n } else { return '!' } }
    if ($n -eq 'Escape') { return '!' }
    if ($KeyNames.ContainsKey($n)) { return $KeyNames[$n] }
    return ''
}
$KeyShow = @{ Return = 'Enter'; Backspace = '⌫'; Left = '←'; Right = '→'; Up = '↑'; Down = '↓'; Space = 'Espacio'; PageUp = 'Re Pág'; PageDown = 'Av Pág'
              LShift = 'Shift izq'; RShift = 'Shift der'; LCtrl = 'Ctrl izq'; RCtrl = 'Ctrl der'; Comma = ','; Period = '.'; Slash = '/'; Semicolon = ';'
              Quote = "'"; LBracket = '['; RBracket = ']'; Backslash = '\'; Grave = '`'; Minus = '-'; Equals = '=' }
if ($Lang -ne 'es') { $KeyShow.Space = 'Space'; $KeyShow.PageUp = 'PgUp'; $KeyShow.PageDown = 'PgDn'; $KeyShow.LShift = 'Left Shift'; $KeyShow.RShift = 'Right Shift'
                      $KeyShow.LCtrl = 'Left Ctrl'; $KeyShow.RCtrl = 'Right Ctrl' }
$script:KeyValue = @{}      # action -> the name now chosen
$script:KeyButton = @{}
$script:Capturing = $null
function Show-KeyName([string]$Name) { if ($Name -eq 'none' -or -not $Name) { return '—' } elseif ($KeyShow.ContainsKey($Name)) { return $KeyShow[$Name] } else { return $Name } }
foreach ($a in $KeyActions.Keys) {
    $cur = [string](Get-Setting $padsIni 'keyboard' $a)
    $script:KeyValue[$a] = if ($cur) { $cur } else { $KeyActions[$a] }
    $card = New-Object Windows.Controls.StackPanel
    $card.Width = 200; $card.Margin = '0,0,12,6'
    $lab = New-Object Windows.Controls.TextBlock
    $lab.Text = T "act_$a"; $lab.Foreground = $win.FindResource('Muted'); $lab.FontSize = 12; $lab.Margin = '2,0,0,3'
    $btn = New-Object Windows.Controls.Button
    $btn.Style = $win.FindResource('Soft'); $btn.Margin = '0'; $btn.Tag = $a
    $btn.Content = Show-KeyName $script:KeyValue[$a]
    $btn.HorizontalContentAlignment = 'Center'
    [void]$card.Children.Add($lab); [void]$card.Children.Add($btn)
    [void](Find 'KeyGrid').Children.Add($card)
    $script:KeyButton[$a] = $btn
    $btn.Add_Click({
        param($s, $e)
        if ($script:Capturing) { $script:KeyButton[$script:Capturing].Content = Show-KeyName $script:KeyValue[$script:Capturing] }
        $script:Capturing = [string]$s.Tag
        (Find 'KeysNote').Text = ''
        $s.Content = T 'pressKey'
        $s.Focus() | Out-Null
    })
}
(Find 'WidthBox').Text = [string](Get-Number (Get-Setting $displayIni '' 'width'))
(Find 'HeightBox').Text = [string](Get-Number (Get-Setting $displayIni '' 'height'))
(Find 'CursorBox').IsChecked = (Get-Number (Get-Setting $displayIni '' 'hide_cursor')) -ne 0
(Find 'OverlayBox').IsChecked = (Get-Number (Get-Setting $displayIni '' 'overlay')) -ne 0
(Find 'DirectBox').IsChecked = -not (Test-ShowLauncher)

# the lobby decides which online fields apply
$updateNet = {
    $m = Get-Chip (Find 'LobbyChips')
    (Find 'CodeBox').IsEnabled = $m -eq 'private' -or $m -eq 'custom'
    (Find 'ServerBox').IsEnabled = $m -eq 'custom'
    (Find 'NameBox').IsEnabled = $m -ne 'offline'
    (Find 'DirectConn').IsEnabled = $m -ne 'offline'
}
foreach ($c in (Find 'LobbyChips').Children) { $c.Add_Checked($updateNet) }
& $updateNet

# ---- the game: versions, status ----------------------------------------------------------------
function Update-GameState {
    $has = [bool]$script:tree
    $label = ($versions | Where-Object { $_.Tree -eq $script:tree } | Select-Object -First 1).Label
    (Find 'VersionValue').Text = if ($has) { $label } else { T 'notBuilt' }
    (Find 'PlayTitle').Text = if ($has) { T 'ready' } else { T 'notready' }
    (Find 'PlayHelp').Text = if ($has) { T 'readyHelp' } else { T 'notreadyHelp' }
    (Find 'PlayBtn').IsEnabled = $has
    (Find 'InstallBtn').Visibility = if ($has) { 'Collapsed' } else { 'Visible' }
    (Find 'NoVersions').Visibility = if ($versions.Count) { 'Collapsed' } else { 'Visible' }
    (Find 'Footer').Text = if ($has) { $label } else { '' }
}
New-Chips (Find 'VersionChips') @($versions | ForEach-Object { @{ Value = $_.Tree; Text = $_.Label } }) $script:tree
foreach ($c in (Find 'VersionChips').Children) { $c.Add_Checked({ param($s, $e) $script:tree = [string]$s.Tag; Update-GameState }) }
Update-GameState
(Find 'ShaderNumber').Text = '{0:N0}' -f $shaderTotal
(Find 'ShaderCaption').Text = "$(T 'shadersCount')  ·  $('{0:N0}' -f $shaderPlayed) $(T 'shadersPlayed')"

# ---- pages ---------------------------------------------------------------------------------------
$pageNames = @{ play = 'PagePlay'; online = 'PageOnline'; pads = 'PagePads'; video = 'PageVideo'; shaders = 'PageShaders'; game = 'PageGame' }
function Show-Page([string]$Name) {
    foreach ($k in $pageNames.Keys) {
        $el = Find $pageNames[$k]
        if ($k -eq $Name) {
            $el.Visibility = 'Visible'
            $el.Opacity = 0
            $tr = New-Object Windows.Media.TranslateTransform(0, 14)
            $el.RenderTransform = $tr
            $fade = New-Object Windows.Media.Animation.DoubleAnimation(0, 1, [TimeSpan]::FromMilliseconds(260))
            $slide = New-Object Windows.Media.Animation.DoubleAnimation(14, 0, [TimeSpan]::FromMilliseconds(320))
            $slide.EasingFunction = New-Object Windows.Media.Animation.CubicEase
            $el.BeginAnimation([Windows.UIElement]::OpacityProperty, $fade)
            $tr.BeginAnimation([Windows.Media.TranslateTransform]::YProperty, $slide)
        } else { $el.Visibility = 'Collapsed' }
    }
}
$navs = @('NavPlay', 'NavOnline', 'NavPads', 'NavVideo', 'NavShaders', 'NavGame')
foreach ($n in $navs) { (Find $n).Add_Checked({ param($s, $e) Show-Page ([string]$s.Tag) }) }
$startPage = if ($Page -eq 'controllers') { 'pads' } elseif ($pageNames.ContainsKey($Page)) { $Page } else { 'play' }
(Find ('Nav' + (Get-Culture).TextInfo.ToTitleCase($startPage).Replace('Pads', 'Pads'))).IsChecked = $true
foreach ($k in $pageNames.Keys) { if ($k -ne $startPage) { (Find $pageNames[$k]).Visibility = 'Collapsed' } else { (Find $pageNames[$k]).Visibility = 'Visible' } }

. (Join-Path $PSScriptRoot 'launcher_pads.ps1')   # the Controllers tab: the pads connected, each set up

# ---- reading the window back into the files' values -------------------------------------------
function Get-WindowValues {
    $onOff = { param($b) if ($b) { '1' } else { '0' } }
    $code = (Find 'CodeBox').Text -replace '\s', ''
    $mode = Get-Chip (Find 'LobbyChips')
    $server = switch ($mode) {
        'offline' { 'off' }
        'public' { $PublicLobby }
        'private' { "$PublicLobby $(if ($code) { $code } else { 'friends' })" }
        'custom' { $s = (Find 'ServerBox').Text.Trim(); if (-not $s) { $s = '127.0.0.1:39420' }; if ($code) { "$s $code" } else { $s } }
    }
    $w = [Math]::Min(7680, [Math]::Max(320, (Get-Number (Find 'WidthBox').Text)))
    $h = [Math]::Min(4320, [Math]::Max(180, (Get-Number (Find 'HeightBox').Text)))
    $result = [ordered]@{
        "$displayIni||window" = Get-Chip (Find 'WindowChips')
        "$displayIni||width" = [string]$w
        "$displayIni||height" = [string]$h
        "$displayIni||pacing" = Get-Chip (Find 'PacingChips')
        "$displayIni||texture_quality" = Get-Chip (Find 'TexChips')
        "$displayIni||gpu" = Get-Chip (Find 'GpuChips')
        "$displayIni||hide_cursor" = & $onOff (Find 'CursorBox').IsChecked
        "$displayIni||overlay" = & $onOff (Find 'OverlayBox').IsChecked
        "$padsIni|general|keyboard_player" = Get-Chip (Find 'KbChips')
        "$padsIni|general|background" = & $onOff (Find 'BackgroundBox').IsChecked
        "$launcherIni||show_launcher" = & $onOff (-not (Find 'DirectBox').IsChecked)
        "$serverFile|" = $server
        "$nameFile|" = (Find 'NameBox').Text.Trim()
        "$directFile|" = if ((Find 'DirectConn').IsChecked) { 'on' } else { 'off' }
        "$reportFile|" = if ((Find 'ReportBox').IsChecked) { 'on' } else { 'off' }
        "$delayFile|" = Get-Chip (Find 'DelayChips')
        "$netcodeFile|" = Get-Chip (Find 'NetcodeChips')
        "$vsFile|" = switch (Get-Chip (Find 'VsChips')) {
            'host' { "host $(Get-Chip (Find 'VsDelayChips')) $script:vsCode $(Get-Chip (Find 'VsPlayersChips'))" }
            'join' { "join $(((Find 'VsPeerBox').Text -replace '\s', '')) $(Get-Chip (Find 'VsDelayChips'))" }
            default { 'off' }
        }
    }
    foreach ($a in $KeyActions.Keys) { $result["$padsIni|keyboard|$a"] = [string]$script:KeyValue[$a] }
    foreach ($kv in (Get-PadValues).GetEnumerator()) { $result[$kv.Key] = $kv.Value }
    $result["$displayIni||vsync"] = if ((Find 'VsyncBox').IsChecked) { 'on' } else { 'off' }
    $result["$displayIni||frameskip"] = if ((Find 'FskipBox').IsChecked) { 'on' } else { 'off' }
    $up = Get-Chip (Find 'UpChips')
    $result["$displayIni||upscale"] = if ($up -eq 'off') { 'off' } else { 'fsr' }
    # Off gives back the full size a mode had taken; any other render_scale is the person's own
    $result["$displayIni||render_scale"] = if ($up -ne 'off') { $UpPresets[$up] } elseif ($script:upFsrWas) { '1' } else { $script:upScaleWas }
    $result["$displayIni||sharpness"] = Get-Chip (Find 'SharpChips')
    $result
}

function Save-WindowValues($Before, $Now) {
    <# Writes the values changed since the window opened (as tools\launcher.ps1 does). #>
    foreach ($k in $Now.Keys) {
        $v = $Now[$k]
        if ([string]::Equals([string]$Before[$k], [string]$v)) { continue }
        $file, $section, $key = $k -split '\|', 3
        $leaf = Split-Path -Leaf $file
        if ($DryRun) { Write-Host "would set $leaf`: $(if ($section) { "[$section] " })$(if ($key) { "$key = " })$v"; continue }
        if ($null -eq $key) {
            if ($v) { [void](Set-LineValue $file $v) }
            elseif (Test-Path -LiteralPath $file) {
                $t = Read-TextLines $file
                $i = Find-FirstLine $t
                if ($i -ge 0) { $t.Lines.RemoveAt($i); Write-TextLines $file $t }
            }
            continue
        }
        if ($file -eq $launcherIni -and -not (Test-Path -LiteralPath $file)) {
            [IO.File]::WriteAllText($file, "; launcher.ini: the window Play SSBU.bat opens (tools\launcher_wpf.ps1).`r`n" +
                "; show_launcher = 0: start the game directly; hold Shift while starting to see the window.`r`n", [Text.Encoding]::ASCII)
        }
        [void](Set-IniValue $file $section $key $v)
    }
}
$before = Get-WindowValues

# ---- buttons -----------------------------------------------------------------------------------
$win.Add_PreviewKeyDown({
    param($s, $e)
    if (-not $script:Capturing) { return }
    $e.Handled = $true
    $a = $script:Capturing
    $key = if ($e.Key -eq 'System') { $e.SystemKey } else { $e.Key }
    if ($key -eq 'Escape') { $script:Capturing = $null; $script:KeyButton[$a].Content = Show-KeyName $script:KeyValue[$a]; return }
    $name = ConvertTo-KeyName $key
    if ($name -eq '!') { (Find 'KeysNote').Text = T 'keyReserved'; return }
    if (-not $name) { return }
    foreach ($o in @($script:KeyValue.Keys)) {   # one key, one action: the other one gives it up
        if ($o -ne $a -and $script:KeyValue[$o] -eq $name) { $script:KeyValue[$o] = 'none'; $script:KeyButton[$o].Content = Show-KeyName 'none' }
    }
    $script:KeyValue[$a] = $name
    $script:KeyButton[$a].Content = Show-KeyName $name
    $script:Capturing = $null
})
(Find 'ResetKeys').Add_Click({
    $script:Capturing = $null
    foreach ($a in $KeyActions.Keys) { $script:KeyValue[$a] = $KeyActions[$a]; $script:KeyButton[$a].Content = Show-KeyName $KeyActions[$a] }
    (Find 'KeysNote').Text = ''
})
function Start-NativeSmashInstaller([string]$Bat) {
    # the installer has a window of its own (a bar, the steps; tools\progress_window.ps1): no console shows
    $env:NATIVESMASH_WINDOW = '1'
    Start-Process -FilePath $env:ComSpec -ArgumentList ('/c ""' + $Bat + '""') -WorkingDirectory $Root -WindowStyle Hidden
}
function Open-Path([string]$Path, [string]$Verb) {
    if (Test-Path -LiteralPath $Path) { Start-Process -FilePath $Path } else { Start-Process -FilePath 'explorer.exe' -ArgumentList "`"$Root`"" }
}
(Find 'CloseBtn').Add_Click({ $win.Close() })
(Find 'MinBtn').Add_Click({ $win.WindowState = [Windows.WindowState]::Minimized })
(Find 'Shell').Add_MouseLeftButtonDown({ param($s, $e) if ($e.OriginalSource -is [Windows.Controls.Border] -or $e.OriginalSource -is [Windows.Controls.Grid]) { try { $win.DragMove() } catch { } } })
$install = {
    $bat = Join-Path $Root 'Install NativeSmash.bat'
    if (Test-Path -LiteralPath $bat) { Start-NativeSmashInstaller $bat } else { Start-Process -FilePath 'explorer.exe' -ArgumentList "`"$Root`"" }
}
(Find 'InstallBtn').Add_Click($install); (Find 'GameInstall').Add_Click($install)
(Find 'OpenPads').Add_Click({ if (Test-Path -LiteralPath $padsIni) { Start-Process notepad.exe -ArgumentList "`"$padsIni`"" } })
(Find 'PrepareBtn').Add_Click({ $b = Join-Path $Root 'launchers\Prepare Shaders.bat'; if (Test-Path -LiteralPath $b) { Start-Process -FilePath $b -WorkingDirectory $Root } })
(Find 'UnlockBtn').Add_Click({ $b = Join-Path $Root 'launchers\Unlock All Fighters.bat'; if (Test-Path -LiteralPath $b) { Start-Process -FilePath $b -WorkingDirectory $Root } })
(Find 'FolderBtn').Add_Click({ Start-Process -FilePath 'explorer.exe' -ArgumentList "`"$Root`"" })
(Find 'ReportsBtn').Add_Click({ $d = Join-Path $Root 'reports'; if (Test-Path -LiteralPath $d) { Start-Process -FilePath 'explorer.exe' -ArgumentList "`"$d`"" } })
(Find 'LogBtn').Add_Click({ $t = if ($script:tree -and $script:tree -ne '.') { Join-Path $Root "$($script:tree)\build\play.log" } else { Join-Path $Root 'build\play.log' }; if (Test-Path -LiteralPath $t) { Start-Process notepad.exe -ArgumentList "`"$t`"" } })

# ---- updates: GitHub's newest commit of the project's branch (tools\project.json), as the installer's first step ------
$Project = $null
try { $Project = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'project.json') -Raw -Encoding UTF8 | ConvertFrom-Json } catch { }
function Get-LocalCommit {
    <# The commit this copy is: a git clone's HEAD, else the one GitHub's ZIP wrote in tools\source_version.txt. #>
    $c = $null
    try { if ((Test-Path -LiteralPath (Join-Path $Root '.git')) -and (Get-Command git -ErrorAction SilentlyContinue)) { $c = [string]((& git -C $Root rev-parse HEAD 2>$null) | Select-Object -First 1) } } catch { }
    if ($c -notmatch '^[0-9a-f]{40}$') {
        $c = $null
        $f = Join-Path $PSScriptRoot 'source_version.txt'
        if (Test-Path -LiteralPath $f) { $t = ([IO.File]::ReadAllText($f)).Trim() -split '\s+'; if ($t[0] -match '^[0-9a-f]{40}$') { $c = $t[0] } }
    }
    return $c
}
$script:upd = @{ State = 'idle'; Result = $null; Local = $null }
$script:updTimer = New-Object Windows.Threading.DispatcherTimer
$script:updTimer.Interval = [TimeSpan]::FromMilliseconds(400)
function Show-UpdateState {
    $r = $script:upd.Result
    $banner = Find 'UpdBanner'; $now = Find 'UpdNowBtn'
    $banner.Visibility = 'Collapsed'; $now.Visibility = 'Collapsed'
    (Find 'UpdCheckBtn').IsEnabled = ($script:upd.State -ne 'checking')
    if ($script:upd.State -eq 'checking') { (Find 'UpdStatus').Text = T 'updChecking'; (Find 'UpdDetail').Text = ''; return }
    if (-not $r) { (Find 'UpdStatus').Text = ''; (Find 'UpdDetail').Text = ''; return }
    if (-not $r.Ok) { (Find 'UpdStatus').Text = T 'updFail'; (Find 'UpdDetail').Text = ''; return }
    $short = { param($s) if ($s) { ([string]$s).Substring(0, 7) } else { '?' } }
    $detail = "$(T 'updThis'): $(if ($script:upd.Local) { & $short $script:upd.Local } else { '?' })   ·   $(T 'updLatest'): $(& $short $r.Remote) - $($r.Message)"
    (Find 'UpdDetail').Text = $detail
    switch ($r.Status) {
        'identical' { (Find 'UpdStatus').Text = T 'updUpToDate' }
        'ahead' {
            $text = "$(T 'updAvailable') ($((T 'updChanges') -f $r.Ahead))"
            (Find 'UpdStatus').Text = $text; $now.Visibility = 'Visible'
            (Find 'UpdBannerText').Text = $text; $banner.Visibility = 'Visible'
        }
        'behind' { (Find 'UpdStatus').Text = T 'updOwn' }
        'diverged' { (Find 'UpdStatus').Text = T 'updOwn' }
        default { (Find 'UpdStatus').Text = T 'updUnknown' }
    }
}
function Start-UpdateCheck {
    if (-not $Project -or $script:upd.State -eq 'checking') { return }
    $script:upd.Local = Get-LocalCommit
    $script:upd.State = 'checking'; $script:upd.Result = $null
    Show-UpdateState
    $ps = [powershell]::Create()
    [void]$ps.AddScript({
        param($repo, $branch, $local)
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        $h = @{ 'User-Agent' = 'NativeSmash-launcher'; 'Accept' = 'application/vnd.github+json' }
        try {
            $head = Invoke-RestMethod -Uri "https://api.github.com/repos/$repo/commits/$branch" -Headers $h -TimeoutSec 8
            $r = @{ Ok = $true; Remote = [string]$head.sha; Message = (([string]$head.commit.message) -split "`n")[0]; Status = 'unknown'; Ahead = 0 }
            if ($local) {
                if ($local -eq $head.sha) { $r.Status = 'identical' }
                else {
                    try { $cmp = Invoke-RestMethod -Uri "https://api.github.com/repos/$repo/compare/$local...$($head.sha)?per_page=1" -Headers $h -TimeoutSec 10; $r.Status = [string]$cmp.status; $r.Ahead = [int]$cmp.ahead_by } catch { }
                }
            }
            return $r
        } catch { return @{ Ok = $false; Error = $_.Exception.Message } }
    }).AddArgument($Project.repo).AddArgument($Project.branch).AddArgument($script:upd.Local)
    $script:updPs = $ps
    $script:updAsync = $ps.BeginInvoke()
    $script:updTimer.Start()
}
$script:updTimer.Add_Tick({
    if (-not $script:updAsync.IsCompleted) { return }
    $script:updTimer.Stop()
    try { $script:upd.Result = @($script:updPs.EndInvoke($script:updAsync))[0] } catch { $script:upd.Result = @{ Ok = $false } }
    try { $script:updPs.Dispose() } catch { }
    $script:upd.State = 'done'
    Show-UpdateState
})
function Invoke-Update {
    if ([Windows.MessageBox]::Show($win, (T 'updConfirm'), 'NativeSmash', 'YesNo', 'Question') -ne 'Yes') { return }
    $bat = @('Install NativeSmash.bat', 'Install NativeSmash.bat') | ForEach-Object { Join-Path $Root $_ } | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if ($bat) { $env:NATIVESMASH_UPDATE = '1'; Start-NativeSmashInstaller $bat; $win.Close() } else { Start-Process -FilePath 'explorer.exe' -ArgumentList "`"$Root`"" }
}
(Find 'UpdCheckBtn').Add_Click({ Start-UpdateCheck })
(Find 'UpdNowBtn').Add_Click({ Invoke-Update })
(Find 'UpdBannerBtn').Add_Click({ Invoke-Update })
(Find 'UpdBannerBtn').Content = T 'updNow'

# ---- Play: start the game, wait for its window, fade away ------------------------------------
$script:waited = 0
$script:fading = $false
$poll = New-Object Windows.Threading.DispatcherTimer
$poll.Interval = [TimeSpan]::FromMilliseconds(400)
$poll.Add_Tick({
    $script:waited += 0.4
    $game = Get-Process -Name 'ssbu' -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
    if ($game -and -not $script:fading) {
        $script:fading = $true
        $poll.Stop()
        # the game's window is up: let it settle for a moment, then melt this one away
        $script:hold = New-Object Windows.Threading.DispatcherTimer   # (script scope: its own tick finds it there)
        $script:hold.Interval = [TimeSpan]::FromMilliseconds(700)
        $script:hold.Add_Tick({
            $script:hold.Stop()
            $out = New-Object Windows.Media.Animation.DoubleAnimation(1, 0, [TimeSpan]::FromMilliseconds(900))
            $out.EasingFunction = New-Object Windows.Media.Animation.CubicEase
            $out.Add_Completed({ if ($NsNet.On -and $NsNet.SignedIn) { $win.Hide(); Watch-Game } else { $win.Close() } })
            $win.BeginAnimation([Windows.Window]::OpacityProperty, $out)
        })
        $script:hold.Start()
    } elseif ($script:waited -gt 240) {
        $poll.Stop()
        (Find 'Starting').Visibility = 'Collapsed'
        [void][Windows.MessageBox]::Show($win, (T 'noStart'), 'NativeSmash', 'OK', 'Warning')
    }
})
(Find 'PlayBtn').Add_Click({
    if ((Get-Chip (Find 'LobbyChips')) -eq 'custom' -and (Find 'ServerBox').Text.Trim() -and (Find 'ServerBox').Text.Trim() -notmatch '^\S+:\d+$') {
        [void][Windows.MessageBox]::Show($win, (T 'badServer'), 'NativeSmash', 'OK', 'Warning'); return
    }
    $vs = Get-Chip (Find 'VsChips')
    $vsPeerNow = (Find 'VsPeerBox').Text -replace '\s', ''
    if ($vs -eq 'join' -and -not $vsPeerNow) { [void][Windows.MessageBox]::Show($win, (T 'vsNoPeer'), 'NativeSmash', 'OK', 'Warning'); return }
    $vars = @{}
    if ($vs -in @('host', 'join')) {
        $vars = @{ SSBU_NETPLAY = $vs; SSBU_NETPLAY_DELAY = (Get-Chip (Find 'VsDelayChips')) }
        # A room code: both games meet in the online lobby (private, named after the code), no VPN or port needed. An address (a.b.c.d[:port]):
        # straight to it, as before (a LAN or a VPN).
        if ($vs -eq 'join' -and $vsPeerNow -match '^\d{1,3}(\.\d{1,3}){3}(:\d+)?$') { $vars.SSBU_NETPLAY_PEER = $vsPeerNow }
        else {
            $room = if ($vs -eq 'host') { $script:vsCode } else { $vsPeerNow.ToUpper() }
            $lobbyNow = if ($lobbyHost -and $lobbyHost -ne 'off' -and $lobbyHost -match ':\d+$') { $lobbyHost } else { $PublicLobby }
            $vars.SSBU_NETPLAY_ROOM = $room
            $vars.SSBU_NET_RELAY = $lobbyNow
            $vars.SSBU_NET_ROOM = "nsd-$($room.ToLower())"
            $me = (Find 'NameBox').Text.Trim()
            if ($me) { $vars.SSBU_NET_NAME = $me }
            if ($vs -eq 'host') { $vars.SSBU_NETPLAY_PLAYERS = Get-Chip (Find 'VsPlayersChips') }
        }
        try {
            # by room code: the host's own save, sent to the others by the game; to an address: the save ZIP, as before
            $data = if ($vars.SSBU_NETPLAY_ROOM) { Initialize-NetplayRoomData $script:tree -Host_:($vs -eq 'host') } else { Initialize-NetplayData $script:tree }
            if ($data) { $vars.SSBU_DATA_ROOT = $data }
        } catch {
            [void][Windows.MessageBox]::Show($win, "$(T 'vsSaveFail')$($_.Exception.Message)", 'NativeSmash', 'OK', 'Warning'); return
        }
    }
    try { Save-WindowValues $before (Get-WindowValues) } catch {
        [void][Windows.MessageBox]::Show($win, "$(T 'saveFail')$($_.Exception.Message)", 'NativeSmash', 'OK', 'Error'); return
    }
    $script:played = $script:tree
    (Find 'Starting').Visibility = 'Visible'
    Save-Version $script:tree
    Start-Game $script:tree $vars
    if ($DryRun) { $win.Close(); return }
    $script:waited = 0
    $poll.Start()
})

# ---- go ----------------------------------------------------------------------------------------
if ($Shot) {
    foreach ($k in $pageNames.Keys) { $el = Find $pageNames[$k]; $el.BeginAnimation([Windows.UIElement]::OpacityProperty, $null); $el.Opacity = 1; $el.RenderTransform = $null }   # (no animation to wait for)
    $content = $win.Content
    $content.Measure([Windows.Size]::new($win.Width, $win.Height))
    $content.Arrange([Windows.Rect]::new(0, 0, $win.Width, $win.Height))
    $content.UpdateLayout()
    $rtb = New-Object Windows.Media.Imaging.RenderTargetBitmap([int]$win.Width, [int]$win.Height, 96, 96, [Windows.Media.PixelFormats]::Pbgra32)
    $rtb.Render($content)
    $enc = New-Object Windows.Media.Imaging.PngBitmapEncoder
    $enc.Frames.Add([Windows.Media.Imaging.BitmapFrame]::Create($rtb))
    $fs = [IO.File]::Create($Shot); $enc.Save($fs); $fs.Close()
    Write-Host "wrote $Shot"
    exit 0
}
$win.Opacity = 0
$win.Add_ContentRendered({
    $in = New-Object Windows.Media.Animation.DoubleAnimation(0, 1, [TimeSpan]::FromMilliseconds(420))
    $win.BeginAnimation([Windows.Window]::OpacityProperty, $in)
    Start-UpdateCheck
    if ($ClickPlay) {
        $script:clickTimer = New-Object Windows.Threading.DispatcherTimer   # (not $auto: that is the -Auto switch)
        $script:clickTimer.Interval = [TimeSpan]::FromMilliseconds(1500)
        $script:clickTimer.Add_Tick({ $script:clickTimer.Stop(); (Find 'PlayBtn').RaiseEvent([Windows.RoutedEventArgs]::new([Windows.Controls.Primitives.ButtonBase]::ClickEvent)) })
        $script:clickTimer.Start()
    }
})
if ($env:NATIVESMASH_SHOTS) { $win.Add_ContentRendered({ . (Join-Path $PSScriptRoot $(if ($env:NATIVESMASH_SHOTS_KIND -in @('online', 'signin')) { "launcher_shots_$($env:NATIVESMASH_SHOTS_KIND).ps1" } else { 'launcher_shots.ps1' })) }) }   # (pictures for players: tools\launcher_shots.ps1)
[void]$win.ShowDialog()
exit 0
