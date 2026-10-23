package com.nativesmash.app;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.util.Locale;

/**
 * Where the game will run. The game's engine is not in this app yet (NativeSmash has no Android renderer: docs/ANDROID.md), so
 * this says so, honestly, instead of pretending; the launcher's settings are already saved for the engine to read.
 */
public class GameActivity extends Activity {
    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        boolean es = Locale.getDefault().getLanguage().equals("es");
        float dp = getResources().getDisplayMetrics().density;
        FrameLayout root = new FrameLayout(this);
        root.setBackground(new GradientDrawable(GradientDrawable.Orientation.TL_BR, new int[]{LauncherActivity.ACCENT_A, LauncherActivity.ACCENT_B}));
        LinearLayout box = new LinearLayout(this);
        box.setOrientation(LinearLayout.VERTICAL);
        box.setGravity(Gravity.CENTER);
        box.setPadding((int) (32 * dp), (int) (16 * dp), (int) (32 * dp), (int) (16 * dp));
        ImageView logo = new ImageView(this);
        logo.setImageResource(R.mipmap.ic_launcher);
        box.addView(logo, new LinearLayout.LayoutParams((int) (72 * dp), (int) (72 * dp)));
        TextView title = new TextView(this);
        title.setText(es ? "El motor del juego aún no está en esta versión" : "The game engine is not in this version yet");
        title.setTextSize(22);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        title.setTextColor(Color.WHITE);
        title.setGravity(Gravity.CENTER);
        title.setPadding(0, (int) (14 * dp), 0, (int) (6 * dp));
        box.addView(title);
        TextView body = new TextView(this);
        body.setText(es ? "Tus ajustes ya están guardados. Cuando el juego pueda dibujarse en Android, empezará aquí."
            : "Your settings are saved. When the game can be drawn on Android, it will start here.");
        body.setTextSize(15);
        body.setTextColor(0xE6FFFFFF);
        body.setGravity(Gravity.CENTER);
        box.addView(body);
        TextView back = new TextView(this);
        back.setText(es ? "Volver al launcher" : "Back to the launcher");
        back.setTextSize(16);
        back.setTypeface(Typeface.DEFAULT_BOLD);
        back.setTextColor(Color.WHITE);
        back.setPadding((int) (26 * dp), (int) (13 * dp), (int) (26 * dp), (int) (13 * dp));
        GradientDrawable pill = new GradientDrawable();
        pill.setColor(0x33000000);
        pill.setCornerRadius(40 * dp);
        back.setBackground(pill);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.topMargin = (int) (22 * dp);
        back.setOnClickListener(new View.OnClickListener() { public void onClick(View v) { finish(); } });
        box.addView(back, lp);
        root.addView(box, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        setContentView(root);
        getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_FULLSCREEN
            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        root.setAlpha(0f);
        root.animate().alpha(1f).setDuration(350).start();
    }

    @Override
    public void finish() {
        super.finish();
        overridePendingTransition(android.R.anim.fade_in, android.R.anim.fade_out);
    }
}
