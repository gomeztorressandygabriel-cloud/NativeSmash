package com.nativesmash.vulkancheck;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.Typeface;
import android.os.Bundle;
import android.widget.ScrollView;
import android.widget.TextView;

/** Shows what this phone's Vulkan driver offers, against what NativeSmash's graphics (DXVK) needs. */
public class MainActivity extends Activity {
    static { System.loadLibrary("vkcheck"); }
    static native String report();

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        String text;
        try { text = report(); } catch (Throwable t) { text = "The check failed: " + t; }
        TextView view = new TextView(this);
        view.setText(text);
        view.setTextColor(Color.rgb(0xE6, 0xE9, 0xF5));
        view.setTypeface(Typeface.MONOSPACE);
        view.setTextSize(12);
        view.setPadding(32, 96, 32, 48);
        view.setTextIsSelectable(true);
        ScrollView scroll = new ScrollView(this);
        scroll.setBackgroundColor(Color.rgb(0x0B, 0x0D, 0x1D));
        scroll.addView(view);
        setContentView(scroll);
    }
}
