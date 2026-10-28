# Cuentas de NativeSmash: lo que se configura a mano

El servidor de jugadores está en Supabase (proyecto **NativeSmash**). Casi todo ya está hecho; estas dos cosas necesitan cuentas o claves secretas que solo puedes poner tú.

## 1. Un servicio de correo propio (para que lleguen todos los correos)

Supabase, con su correo de serie, solo envía **unos pocos correos por hora** (confirmar la cuenta, contraseña olvidada). Si se apunta mucha gente a la vez, algunos no recibirán el correo. Con un servicio propio, como **Resend** (gratis hasta 3.000 correos al mes), se acaba el problema.

1. Crea una cuenta en <https://resend.com> e inicia sesión.
2. En **Domains**, añade un dominio tuyo y sigue los pasos para verificarlo. Sin dominio propio, Resend solo deja enviar a tu propio correo, así que hace falta uno; se compra por unos pocos euros al año.
3. En **API Keys**, crea una clave. **No me la pases a mí**: es secreta.
4. En Supabase: **Authentication → Emails → SMTP Settings** → activa **Enable Custom SMTP** y rellena:
   - **Sender email**: `no-responder@tudominio.com`
   - **Sender name**: `NativeSmash`
   - **Host**: `smtp.resend.com` · **Port**: `465`
   - **Username**: `resend` · **Password**: la clave de Resend
5. Guarda. Después, en **Authentication → Rate Limits**, sube **Emails sent per hour** (por ejemplo, a 100).

## 2. Entrar con Discord (opcional)

1. En <https://discord.com/developers/applications> → **New Application** → nombre `NativeSmash`.
2. En **OAuth2**, copia el **Client ID** y pulsa **Reset Secret** para ver el **Client Secret**. **No me lo pases a mí.**
3. En **OAuth2 → Redirects**, añade: `https://vsaogrjmlwzwvdtffyow.supabase.co/auth/v1/callback`
4. En Supabase: **Authentication → Sign In / Providers → Discord** → actívalo y pega el Client ID y el Client Secret. Guarda.
5. Avísame y añado el botón **Entrar con Discord** a la web (y la forma de entrar desde el launcher).

Con Google es igual, en <https://console.cloud.google.com> (**APIs y servicios → Credenciales → ID de cliente de OAuth**), con la misma dirección de redirección.

## 3. Los correos en español

Los textos están en `server/supabase/emails/`. En Supabase: **Authentication → Emails → Templates**. En cada plantilla, pon el asunto y pega el archivo:

| Plantilla de Supabase | Asunto | Archivo |
| :--- | :--- | :--- |
| Confirm signup | Confirma tu cuenta de NativeSmash | `confirmar.html` |
| Reset Password | Pon una contraseña nueva en NativeSmash | `contrasena.html` |
| Change Email Address | Confirma tu correo nuevo en NativeSmash | `cambio-correo.html` |
| Magic Link | Entra en NativeSmash | `enlace-magico.html` |

(Si me abres Supabase en el navegador con tu sesión iniciada, lo pego yo.)
