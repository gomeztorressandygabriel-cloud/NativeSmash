package com.nativesmash.app;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.DisplayMetrics;
import android.view.KeyEvent;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.Random;

/**
 * What the player chose in the launcher. Kept in SharedPreferences (so it is there next time the app opens), and written, on
 * every change, as the same files the PC version reads (display.ini, online_name.txt, online_server.txt, controllers.ini) into
 * the app's folder, for the game itself to read when it starts.
 */
final class Settings {
    static final String LOBBY = "smash.recomps.wtf:39421";
    /** The render heights offered: 0 is "auto" (by the phone's screen), -1 is the screen's own (native). */
    static final int[] HEIGHTS = {0, 540, 720, 900, 1080, -1};
    /** The console buttons, in the order they are listed and written. */
    static final String[] ACTIONS = {"A", "B", "X", "Y", "L", "R", "ZL", "ZR", "PLUS", "MINUS", "LSTICK", "RSTICK", "UP", "DOWN", "LEFT", "RIGHT"};
    /** Android key codes for those on a pad laid out as on a Switch (the bottom face button is the console's B). */
    static final int[] DEFAULT_KEYS = {
        KeyEvent.KEYCODE_BUTTON_B, KeyEvent.KEYCODE_BUTTON_A, KeyEvent.KEYCODE_BUTTON_Y, KeyEvent.KEYCODE_BUTTON_X,
        KeyEvent.KEYCODE_BUTTON_L1, KeyEvent.KEYCODE_BUTTON_R1, KeyEvent.KEYCODE_BUTTON_L2, KeyEvent.KEYCODE_BUTTON_R2,
        KeyEvent.KEYCODE_BUTTON_START, KeyEvent.KEYCODE_BUTTON_SELECT, KeyEvent.KEYCODE_BUTTON_THUMBL, KeyEvent.KEYCODE_BUTTON_THUMBR,
        KeyEvent.KEYCODE_DPAD_UP, KeyEvent.KEYCODE_DPAD_DOWN, KeyEvent.KEYCODE_DPAD_LEFT, KeyEvent.KEYCODE_DPAD_RIGHT};

    int resolution = 0;          // index into HEIGHTS
    boolean fsr = true;
    int sharpness = 80;          // percent
    boolean touch = true;        // touch controls; else a pad
    int touchSize = 60;          // percent of the slider
    int touchOpacity = 70;
    boolean haptics = true;
    boolean leftHanded = false;
    int[] keys = DEFAULT_KEYS.clone();
    int deadzone = 10;
    int rumble = 100;
    String name = "";
    int lobbyMode = 0;           // 0 public lobby, 1 private (a code), 2 offline
    String lobbyCode = "";
    boolean direct = true;

    private final SharedPreferences prefs;

    Settings(Context c) {
        prefs = c.getSharedPreferences("nativesmash", Context.MODE_PRIVATE);
        resolution = prefs.getInt("resolution", resolution);
        fsr = prefs.getBoolean("fsr", fsr);
        sharpness = prefs.getInt("sharpness", sharpness);
        touch = prefs.getBoolean("touch", touch);
        touchSize = prefs.getInt("touchSize", touchSize);
        touchOpacity = prefs.getInt("touchOpacity", touchOpacity);
        haptics = prefs.getBoolean("haptics", haptics);
        leftHanded = prefs.getBoolean("leftHanded", leftHanded);
        deadzone = prefs.getInt("deadzone", deadzone);
        rumble = prefs.getInt("rumble", rumble);
        for (int i = 0; i < keys.length; i++) keys[i] = prefs.getInt("key_" + ACTIONS[i], keys[i]);
        name = prefs.getString("name", "");
        if (name.isEmpty()) name = "Player" + (1000 + new Random().nextInt(9000));
        lobbyMode = prefs.getInt("lobbyMode", lobbyMode);
        lobbyCode = prefs.getString("lobbyCode", lobbyCode);
        direct = prefs.getBoolean("direct", direct);
    }

    void save(Context c) {
        SharedPreferences.Editor e = prefs.edit();
        e.putInt("resolution", resolution).putBoolean("fsr", fsr).putInt("sharpness", sharpness)
         .putBoolean("touch", touch).putInt("touchSize", touchSize).putInt("touchOpacity", touchOpacity)
         .putBoolean("haptics", haptics).putBoolean("leftHanded", leftHanded)
         .putInt("deadzone", deadzone).putInt("rumble", rumble)
         .putString("name", name).putInt("lobbyMode", lobbyMode).putString("lobbyCode", lobbyCode).putBoolean("direct", direct);
        for (int i = 0; i < keys.length; i++) e.putInt("key_" + ACTIONS[i], keys[i]);
        e.apply();
        try { writeFiles(c); } catch (IOException ignored) { }
    }

    void resetKeys() { keys = DEFAULT_KEYS.clone(); }

    static String keyName(int code) { return KeyEvent.keyCodeToString(code).replace("KEYCODE_", ""); }

    /** The render height for this phone: what the player picked, or by the screen when it is "auto". */
    int renderHeight(Context c) {
        DisplayMetrics m = c.getResources().getDisplayMetrics();
        int native_ = Math.min(m.widthPixels, m.heightPixels);
        int h = HEIGHTS[resolution];
        if (h == -1) return native_;
        if (h == 0) return Math.min(native_, native_ > 1200 ? 720 : 540);   // auto: 720p on big screens, 540p otherwise (FSR makes up the rest)
        return h;
    }

    private void write(File dir, String name, String text) throws IOException {
        try (FileOutputStream f = new FileOutputStream(new File(dir, name))) { f.write(text.getBytes(StandardCharsets.UTF_8)); }
    }

    void writeFiles(Context c) throws IOException {
        File dir = c.getExternalFilesDir(null);
        if (dir == null) dir = c.getFilesDir();
        // the folders the game's data goes in, made by the app itself: ones made by `adb push` belong to the shell, and the game could not enter them
        for (String sub : new String[]{"dump/modules", "dump/fonts", "dump/nro", "save", "cache", "reports"}) new File(dir, sub).mkdirs();
        DisplayMetrics m = c.getResources().getDisplayMetrics();
        int nativeH = Math.min(m.widthPixels, m.heightPixels), nativeW = Math.max(m.widthPixels, m.heightPixels);
        double scale = Math.min(1.0, renderHeight(c) / (double) nativeH);
        write(dir, "display.ini", "; display.ini - written by the NativeSmash launcher\n"
            + "window = borderless\nwidth = " + nativeW + "\nheight = " + nativeH + "\n"
            + "upscale = " + (fsr && scale < 0.999 ? "fsr" : "bilinear") + "\n"
            + "render_scale = " + String.format(java.util.Locale.US, "%.2f", scale) + "\n"
            + "sharpness = " + String.format(java.util.Locale.US, "%.2f", sharpness / 100.0) + "\n"
            + "vsync = display\n");
        write(dir, "online_name.txt", name + "\n");
        String server = lobbyMode == 2 ? "off" : LOBBY + (lobbyMode == 1 && !lobbyCode.trim().isEmpty() ? " " + lobbyCode.trim() : "");
        write(dir, "online_server.txt", "# written by the NativeSmash launcher\n" + server + "\n");
        write(dir, "online_direct.txt", direct ? "on\n" : "off\n");
        StringBuilder pad = new StringBuilder("; controllers.ini - written by the NativeSmash launcher\n[general]\nkeyboard_player = 0\n\n[device default]\nlayout = positional\n"
            + "deadzone = " + deadzone + "\nrumble = " + rumble + "\n");
        for (int i = 0; i < ACTIONS.length; i++) pad.append(ACTIONS[i]).append(" = ").append(keyName(keys[i])).append('\n');
        write(dir, "controllers.ini", pad.toString());
        write(dir, "touch.ini", "; the on-screen controls\nenabled = " + (touch ? 1 : 0) + "\nsize = " + touchSize + "\nopacity = " + touchOpacity
            + "\nhaptics = " + (haptics ? 1 : 0) + "\nleft_handed = " + (leftHanded ? 1 : 0) + "\n");
    }
}
