package com.nativesmash.app;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.text.Editable;
import android.text.InputType;
import android.text.TextWatcher;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewAnimationUtils;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.Switch;
import android.widget.TextView;

import java.util.Locale;

/**
 * NativeSmash's launcher for phones: the screen resolution, the controls, the online settings and a Play button that fades into
 * the game. What is chosen is saved as it is chosen (Settings), so it is there the next time the app opens.
 */
public class LauncherActivity extends Activity {
    // the launcher's colours (the PC launcher's)
    static final int BG1 = 0xFF171A33, BG2 = 0xFF0B0D1D, CARD = 0x14FFFFFF, EDGE = 0x26FFFFFF, INK = 0xFFF2F4FB, MUTED = 0xFF9AA1BC;
    static final int ACCENT_A = 0xFF6D7DFF, ACCENT_B = 0xFFA66BFF, OK = 0xFF62E3A4, CHIP_OFF = 0x1AFFFFFF;

    private Settings s;
    private boolean es;
    private float dp;
    private View overlay;
    private LinearLayout padList;
    private LinearLayout touchBox, padBox;
    private EditText codeBox;
    private FrameLayout root;

    private String t(String spanish, String english) { return es ? spanish : english; }
    private int px(float v) { return Math.round(v * dp); }

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        es = Locale.getDefault().getLanguage().equals("es");
        dp = getResources().getDisplayMetrics().density;
        s = new Settings(this);
        Window w = getWindow();
        w.setStatusBarColor(BG2);
        w.setNavigationBarColor(BG2);
        w.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);

        root = new FrameLayout(this);
        root.setBackground(gradient(BG1, BG2, GradientDrawable.Orientation.TL_BR, 0));

        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        scroll.setVerticalScrollBarEnabled(false);
        LinearLayout col = new LinearLayout(this);
        col.setOrientation(LinearLayout.VERTICAL);
        col.setPadding(px(18), px(28), px(18), px(130));   // (room at the bottom for the Play button)
        scroll.addView(col, new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        root.addView(scroll, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        col.addView(header());
        col.addView(screenCard());
        col.addView(controlsCard());
        col.addView(onlineCard());
        col.addView(phoneCard());
        col.addView(spacer(8));
        TextView foot = text(t("NativeSmash es un proyecto de la comunidad. No incluye el juego: se construye con tu propia copia.",
            "NativeSmash is a community project. It does not include the game: it is built from your own copy."), 12, MUTED, false);
        foot.setGravity(Gravity.CENTER);
        col.addView(foot);

        root.addView(playBar(), new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.BOTTOM));
        setContentView(root);
        showControlsBox();
        // the first time, the screen fades in
        root.setAlpha(0f);
        root.animate().alpha(1f).setDuration(350).start();
    }

    @Override
    protected void onPause() {
        super.onPause();
        s.save(this);   // (what was chosen is kept when the app is closed, or sent to the background)
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (overlay != null) {   // back from the game: the cover goes away softly
            final View o = overlay;
            overlay = null;
            o.animate().alpha(0f).setDuration(400).withEndAction(new Runnable() { public void run() { root.removeView(o); } }).start();
        }
    }

    // ---- pieces ----
    private GradientDrawable gradient(int a, int b, GradientDrawable.Orientation o, float radiusDp) {
        GradientDrawable g = new GradientDrawable(o, new int[]{a, b});
        g.setCornerRadius(px(radiusDp));
        return g;
    }

    private GradientDrawable rounded(int fill, float radiusDp, int stroke) {
        GradientDrawable g = new GradientDrawable();
        g.setColor(fill);
        g.setCornerRadius(px(radiusDp));
        if (stroke != 0) g.setStroke(Math.max(1, px(1)), stroke);
        return g;
    }

    private TextView text(String value, float sp, int color, boolean bold) {
        TextView v = new TextView(this);
        v.setText(value);
        v.setTextSize(sp);
        v.setTextColor(color);
        if (bold) v.setTypeface(Typeface.DEFAULT_BOLD);
        return v;
    }

    private View spacer(float h) {
        View v = new View(this);
        v.setLayoutParams(new LinearLayout.LayoutParams(1, px(h)));
        return v;
    }

    private LinearLayout card(String title, String hint) {
        LinearLayout c = new LinearLayout(this);
        c.setOrientation(LinearLayout.VERTICAL);
        c.setBackground(rounded(CARD, 20, EDGE));
        c.setPadding(px(18), px(16), px(18), px(18));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.bottomMargin = px(14);
        c.setLayoutParams(lp);
        c.addView(text(title, 19, INK, true));
        if (hint != null) {
            TextView h = text(hint, 13, MUTED, false);
            h.setPadding(0, px(2), 0, px(8));
            c.addView(h);
        } else {
            c.addView(spacer(8));
        }
        return c;
    }

    private View header() {
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setPadding(px(4), 0, 0, px(18));
        ImageView logo = new ImageView(this);
        logo.setImageResource(R.mipmap.ic_launcher);
        row.addView(logo, new LinearLayout.LayoutParams(px(52), px(52)));
        LinearLayout names = new LinearLayout(this);
        names.setOrientation(LinearLayout.VERTICAL);
        names.setPadding(px(14), 0, 0, 0);
        names.addView(text("NativeSmash", 26, INK, true));
        names.addView(text(t("Super Smash Bros. Ultimate · en tu móvil", "Super Smash Bros. Ultimate · on your phone"), 13, MUTED, false));
        row.addView(names);
        return row;
    }

    /** A row of options, one picked; picking calls back with its index. */
    private LinearLayout chips(String[] labels, int selected, final OnPick pick) {
        final LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        final TextView[] all = new TextView[labels.length];
        for (int i = 0; i < labels.length; i++) {
            final int index = i;
            TextView c = text(labels[i], 14, INK, false);
            c.setGravity(Gravity.CENTER);
            c.setPadding(px(6), px(11), px(6), px(11));
            c.setSingleLine(true);
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
            lp.rightMargin = i < labels.length - 1 ? px(6) : 0;
            row.addView(c, lp);
            all[i] = c;
            c.setOnClickListener(new View.OnClickListener() {
                public void onClick(View v) {
                    for (int j = 0; j < all.length; j++) styleChip(all[j], j == index);
                    haptic();
                    pick.pick(index);
                }
            });
        }
        for (int j = 0; j < all.length; j++) styleChip(all[j], j == selected);
        return row;
    }

    private void styleChip(TextView c, boolean on) {
        if (on) {
            c.setBackground(gradient(ACCENT_A, ACCENT_B, GradientDrawable.Orientation.LEFT_RIGHT, 12));
            c.setTypeface(Typeface.DEFAULT_BOLD);
        } else {
            c.setBackground(rounded(CHIP_OFF, 12, EDGE));
            c.setTypeface(Typeface.DEFAULT);
        }
    }

    interface OnPick { void pick(int index); }
    interface OnValue { void value(int v); }
    interface OnFlag { void flag(boolean on); }

    private View slider(String label, int value, final OnValue change) {
        LinearLayout box = new LinearLayout(this);
        box.setOrientation(LinearLayout.VERTICAL);
        box.setPadding(0, px(10), 0, 0);
        final TextView name = text(label + ": " + value + " %", 14, INK, false);
        box.addView(name);
        SeekBar bar = new SeekBar(this);
        bar.setMax(100);
        bar.setProgress(value);
        bar.setProgressTintList(ColorStateList.valueOf(ACCENT_B));
        bar.setThumbTintList(ColorStateList.valueOf(ACCENT_B));
        final String base = label;
        bar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            public void onProgressChanged(SeekBar b, int p, boolean user) { name.setText(base + ": " + p + " %"); if (user) change.value(p); }
            public void onStartTrackingTouch(SeekBar b) { }
            public void onStopTrackingTouch(SeekBar b) { s.save(LauncherActivity.this); }
        });
        box.addView(bar);
        return box;
    }

    private View toggle(String label, boolean on, final OnFlag change) {
        Switch sw = new Switch(this);
        sw.setText(label);
        sw.setTextColor(INK);
        sw.setTextSize(14);
        sw.setChecked(on);
        sw.setPadding(0, px(12), 0, px(2));
        sw.setThumbTintList(ColorStateList.valueOf(on ? ACCENT_B : MUTED));
        sw.setTrackTintList(ColorStateList.valueOf(0x66A66BFF));
        sw.setOnCheckedChangeListener(new android.widget.CompoundButton.OnCheckedChangeListener() {
            public void onCheckedChanged(android.widget.CompoundButton b, boolean checked) {
                ((Switch) b).setThumbTintList(ColorStateList.valueOf(checked ? ACCENT_B : MUTED));
                haptic();
                change.flag(checked);
                s.save(LauncherActivity.this);
            }
        });
        return sw;
    }

    private void haptic() {
        if (!s.haptics) return;
        Vibrator v = (Vibrator) getSystemService(Context.VIBRATOR_SERVICE);
        if (v != null && v.hasVibrator()) v.vibrate(VibrationEffect.createPredefined(VibrationEffect.EFFECT_TICK));
    }

    // ---- the cards ----
    private View screenCard() {
        LinearLayout c = card(t("Pantalla", "Screen"), t("La resolución a la que se dibuja el juego. Más baja = más fluido; el escalado FSR la afina.",
            "The resolution the game is drawn at. Lower is smoother; FSR upscaling sharpens it."));
        String[] labels = {t("Auto", "Auto"), "540p", "720p", "900p", "1080p", t("Nativa", "Native")};
        c.addView(chips(labels, s.resolution, new OnPick() { public void pick(int i) { s.resolution = i; s.save(LauncherActivity.this); } }));
        TextView tex = text(t("Texturas: más bajas gastan menos memoria y dan más margen al teléfono.", "Textures: lower ones use less memory and give the phone more room."), 13, MUTED, false);
        tex.setPadding(0, px(10), 0, px(6));
        c.addView(tex);
        c.addView(chips(new String[]{t("Bajas", "Low"), t("Medias", "Medium"), t("Originales", "Original")}, s.textures, new OnPick() { public void pick(int i) { s.textures = i; s.save(LauncherActivity.this); } }));
        c.addView(toggle(t("Escalado FSR (afina la imagen)", "FSR upscaling (sharpens the image)"), s.fsr, new OnFlag() { public void flag(boolean on) { s.fsr = on; } }));
        c.addView(slider(t("Nitidez", "Sharpness"), s.sharpness, new OnValue() { public void value(int v) { s.sharpness = v; } }));
        return c;
    }

    private View controlsCard() {
        LinearLayout c = card(t("Controles", "Controls"), t("Táctil en pantalla, o un mando Bluetooth o USB.", "On-screen touch, or a Bluetooth or USB controller."));
        c.addView(chips(new String[]{t("Táctil", "Touch"), t("Mando", "Controller")}, s.touch ? 0 : 1, new OnPick() {
            public void pick(int i) { s.touch = i == 0; showControlsBox(); s.save(LauncherActivity.this); }
        }));
        touchBox = new LinearLayout(this);
        touchBox.setOrientation(LinearLayout.VERTICAL);
        touchBox.addView(slider(t("Tamaño de los botones", "Button size"), s.touchSize, new OnValue() { public void value(int v) { s.touchSize = v; } }));
        touchBox.addView(slider(t("Opacidad", "Opacity"), s.touchOpacity, new OnValue() { public void value(int v) { s.touchOpacity = v; } }));
        touchBox.addView(toggle(t("Vibrar al tocar", "Vibrate on touch"), s.haptics, new OnFlag() { public void flag(boolean on) { s.haptics = on; } }));
        touchBox.addView(toggle(t("Modo zurdo (cruceta a la derecha)", "Left-handed (D-pad on the right)"), s.leftHanded, new OnFlag() { public void flag(boolean on) { s.leftHanded = on; } }));
        c.addView(touchBox);

        padBox = new LinearLayout(this);
        padBox.setOrientation(LinearLayout.VERTICAL);
        TextView hint = text(t("Toca un botón de la lista y luego pulsa el botón del mando que quieras usar.", "Tap a button in the list, then press the button on your controller you want to use."), 13, MUTED, false);
        hint.setPadding(0, px(10), 0, px(6));
        padBox.addView(hint);
        padList = new LinearLayout(this);
        padList.setOrientation(LinearLayout.VERTICAL);
        padBox.addView(padList);
        fillPadList();
        padBox.addView(slider(t("Zona muerta del stick", "Stick dead zone"), s.deadzone, new OnValue() { public void value(int v) { s.deadzone = v; } }));
        padBox.addView(slider(t("Vibración del mando", "Controller rumble"), s.rumble, new OnValue() { public void value(int v) { s.rumble = v; } }));
        TextView reset = text(t("Restablecer botones", "Reset buttons"), 14, 0xFF8E9BFF, false);
        reset.setPadding(0, px(14), 0, px(4));
        reset.setOnClickListener(new View.OnClickListener() { public void onClick(View v) { s.resetKeys(); fillPadList(); s.save(LauncherActivity.this); } });
        padBox.addView(reset);
        c.addView(padBox);
        return c;
    }

    private void showControlsBox() {
        if (touchBox != null) touchBox.setVisibility(s.touch ? View.VISIBLE : View.GONE);
        if (padBox != null) padBox.setVisibility(s.touch ? View.GONE : View.VISIBLE);
    }

    private void fillPadList() {
        padList.removeAllViews();
        for (int i = 0; i < Settings.ACTIONS.length; i++) {
            final int index = i;
            LinearLayout row = new LinearLayout(this);
            row.setOrientation(LinearLayout.HORIZONTAL);
            row.setGravity(Gravity.CENTER_VERTICAL);
            row.setPadding(px(12), px(10), px(12), px(10));
            row.setBackground(rounded(CHIP_OFF, 12, 0));
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            lp.topMargin = px(6);
            row.setLayoutParams(lp);
            TextView action = text(Settings.ACTIONS[i], 15, INK, true);
            row.addView(action, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
            row.addView(text(Settings.keyName(s.keys[i]), 14, MUTED, false));
            row.setOnClickListener(new View.OnClickListener() { public void onClick(View v) { remap(index); } });
            padList.addView(row);
        }
    }

    private void remap(final int index) {
        final AlertDialog d = new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
            .setTitle(Settings.ACTIONS[index])
            .setMessage(t("Pulsa ahora el botón del mando que quieras usar.", "Now press the controller button you want to use."))
            .setNegativeButton(t("Cancelar", "Cancel"), null)
            .create();
        d.setOnKeyListener(new android.content.DialogInterface.OnKeyListener() {
            public boolean onKey(android.content.DialogInterface di, int keyCode, KeyEvent ev) {
                if (ev.getAction() != KeyEvent.ACTION_UP || keyCode == KeyEvent.KEYCODE_BACK) return false;
                s.keys[index] = keyCode;
                fillPadList();
                s.save(LauncherActivity.this);
                di.dismiss();
                return true;
            }
        });
        d.show();
    }

    private View onlineCard() {
        LinearLayout c = card(t("En línea", "Online"), t("Para jugar con otras personas en salas (Local Wireless por internet).", "To play with other people in rooms (Local Wireless over the internet)."));
        c.addView(text(t("Tu nombre", "Your name"), 14, INK, false));
        final EditText name = edit(s.name, t("Tu nombre en línea", "Your online name"), 24);
        name.addTextChangedListener(new TextWatcher() {
            public void beforeTextChanged(CharSequence a, int b, int c2, int d) { }
            public void onTextChanged(CharSequence a, int b, int c2, int d) { }
            public void afterTextChanged(Editable e) { s.name = e.toString().trim().isEmpty() ? s.name : e.toString().trim(); }
        });
        c.addView(name);
        c.addView(spacer(12));
        c.addView(chips(new String[]{t("Sala pública", "Public lobby"), t("Con código", "With a code"), t("Sin conexión", "Offline")}, s.lobbyMode, new OnPick() {
            public void pick(int i) { s.lobbyMode = i; codeBox.setVisibility(i == 1 ? View.VISIBLE : View.GONE); s.save(LauncherActivity.this); }
        }));
        codeBox = edit(s.lobbyCode, t("Código de tu grupo (solo tus amigos lo usan)", "Your group's code (only your friends use it)"), 32);
        codeBox.addTextChangedListener(new TextWatcher() {
            public void beforeTextChanged(CharSequence a, int b, int c2, int d) { }
            public void onTextChanged(CharSequence a, int b, int c2, int d) { }
            public void afterTextChanged(Editable e) { s.lobbyCode = e.toString(); }
        });
        codeBox.setVisibility(s.lobbyMode == 1 ? View.VISIBLE : View.GONE);
        LinearLayout.LayoutParams lp = (LinearLayout.LayoutParams) codeBox.getLayoutParams();
        lp.topMargin = px(10);
        c.addView(codeBox);
        c.addView(toggle(t("Conexiones directas (más rápido; los demás ven tu dirección de internet)", "Direct connections (faster; others see your internet address)"), s.direct, new OnFlag() { public void flag(boolean on) { s.direct = on; } }));
        return c;
    }

    private EditText edit(String value, String hint, int max) {
        EditText e = new EditText(this);
        e.setText(value);
        e.setHint(hint);
        e.setSingleLine(true);
        e.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        e.setFilters(new android.text.InputFilter[]{new android.text.InputFilter.LengthFilter(max)});
        e.setTextColor(INK);
        e.setHintTextColor(0xFF6B7294);
        e.setTextSize(15);
        e.setBackground(rounded(0x1AFFFFFF, 12, EDGE));
        e.setPadding(px(14), px(12), px(14), px(12));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.topMargin = px(6);
        e.setLayoutParams(lp);
        return e;
    }

    private View phoneCard() {
        LinearLayout c = card(t("Tu móvil", "Your phone"), t("Comprueba si la tarjeta gráfica de tu móvil tiene lo que el juego necesita (Vulkan).", "Check whether your phone's graphics card has what the game needs (Vulkan)."));
        TextView b = text(t("Comprobar compatibilidad", "Check compatibility"), 15, INK, true);
        b.setGravity(Gravity.CENTER);
        b.setPadding(px(10), px(13), px(10), px(13));
        b.setBackground(rounded(CHIP_OFF, 12, EDGE));
        b.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                String report;
                try { report = Compat.report(); } catch (Throwable e) { report = "The check could not run: " + e; }
                TextView body = text(report, 11, INK, false);
                body.setTypeface(Typeface.MONOSPACE);
                body.setTextIsSelectable(true);
                body.setPadding(px(16), px(8), px(16), px(8));
                ScrollView sv = new ScrollView(LauncherActivity.this);
                sv.addView(body);
                new AlertDialog.Builder(LauncherActivity.this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                    .setTitle(t("Compatibilidad", "Compatibility")).setView(sv).setPositiveButton("OK", null).show();
            }
        });
        c.addView(b);
        return c;
    }

    // ---- Play ----
    private View playBar() {
        LinearLayout outer = new LinearLayout(this);
        outer.setOrientation(LinearLayout.VERTICAL);
        View fade = new View(this);
        fade.setBackground(gradient(0x000B0D1D, 0xFF0B0D1D, GradientDrawable.Orientation.TOP_BOTTOM, 0));
        outer.addView(fade, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, px(26)));
        FrameLayout bar = new FrameLayout(this);
        bar.setBackgroundColor(0xFF0B0D1D);
        bar.setPadding(px(18), px(4), px(18), px(18));
        outer.addView(bar, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        final TextView play = text(t("JUGAR", "PLAY"), 20, Color.WHITE, true);
        play.setGravity(Gravity.CENTER);
        play.setLetterSpacing(0.12f);
        play.setBackground(gradient(ACCENT_A, ACCENT_B, GradientDrawable.Orientation.LEFT_RIGHT, 18));
        play.setElevation(px(8));
        play.setPadding(0, px(17), 0, px(17));
        play.setOnClickListener(new View.OnClickListener() { public void onClick(View v) { startGame(v); } });
        bar.addView(play, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        return outer;
    }

    /** Saves, then covers the screen with a circle that grows out of the button, and goes into the game under it. */
    private void startGame(View button) {
        if (overlay != null) return;
        s.save(this);
        haptic();
        final View cover = new View(this);
        cover.setBackground(gradient(ACCENT_A, ACCENT_B, GradientDrawable.Orientation.TL_BR, 0));
        root.addView(cover, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        overlay = cover;
        int[] where = new int[2], at = new int[2];
        button.getLocationInWindow(where);
        root.getLocationInWindow(at);
        int cx = where[0] - at[0] + button.getWidth() / 2, cy = where[1] - at[1] + button.getHeight() / 2;
        float end = (float) Math.hypot(root.getWidth(), root.getHeight());
        Animator reveal = ViewAnimationUtils.createCircularReveal(cover, cx, cy, 0f, end);
        reveal.setDuration(520);
        reveal.setInterpolator(new android.view.animation.AccelerateDecelerateInterpolator());
        reveal.addListener(new AnimatorListenerAdapter() {
            @Override public void onAnimationEnd(Animator a) {
                Class<?> target = GameActivity.class;
                try { target = Class.forName("com.nativesmash.app.EngineActivity"); } catch (ClassNotFoundException none) { }   /* (only in the --engine build) */
                startActivity(new Intent(LauncherActivity.this, target));
                overridePendingTransition(android.R.anim.fade_in, android.R.anim.fade_out);
            }
        });
        reveal.start();
    }
}
