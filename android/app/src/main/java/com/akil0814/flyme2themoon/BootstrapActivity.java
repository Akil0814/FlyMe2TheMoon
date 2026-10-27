package com.akil0814.flyme2themoon;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.os.Bundle;
import android.view.Gravity;
import android.widget.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import org.json.*;

public final class BootstrapActivity extends Activity {
    private TextView status;
    private ProgressBar progress;
    private Button retry;
    private volatile boolean working;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setGravity(Gravity.CENTER);
        layout.setPadding(64, 24, 64, 24);
        TextView title = new TextView(this);
        title.setText("FlyMe2TheMoon"); title.setTextSize(28); title.setTextColor(Color.WHITE);
        layout.addView(title);
        status = new TextView(this); status.setTextColor(Color.WHITE); status.setTextSize(16);
        layout.addView(status);
        progress = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        layout.addView(progress, new LinearLayout.LayoutParams(640, 36));
        retry = new Button(this); retry.setText("Retry / 重试"); retry.setVisibility(android.view.View.GONE);
        retry.setOnClickListener(view -> prepare()); layout.addView(retry);
        Button exit = new Button(this); exit.setText("Exit / 退出"); exit.setOnClickListener(view -> finish()); layout.addView(exit);
        setContentView(layout);
        prepare();
    }
    private void prepare() {
        if (working) return;
        working = true;
        retry.setVisibility(android.view.View.GONE);
        status.setText("Preparing game resources / 正在准备游戏资源…");
        progress.setProgress(0);
        new Thread(() -> {
            try {
                String json;
                try (InputStream stream = getAssets().open("game-assets.json"); ByteArrayOutputStream output = new ByteArrayOutputStream()) {
                    byte[] buffer = new byte[8192]; int count;
                    while ((count = stream.read(buffer)) != -1) output.write(buffer, 0, count);
                    json = new String(output.toByteArray(), StandardCharsets.UTF_8);
                }
                JSONObject object = new JSONObject(json);
                JSONArray entries = object.getJSONArray("files");
                List<AssetInstaller.Entry> files = new ArrayList<>();
                for (int i=0; i<entries.length(); ++i) {
                    JSONObject entry = entries.getJSONObject(i);
                    files.add(new AssetInstaller.Entry(entry.getString("path"), entry.getLong("size"), entry.getString("sha256")));
                }
                File root = AssetInstaller.install(new File(getFilesDir(), "content"),
                    new AssetInstaller.Manifest(object.getString("hash"), object.getLong("totalBytes"), files),
                    () -> getAssets().open("game-assets.zip"), value -> runOnUiThread(() -> progress.setProgress(value)));
                runOnUiThread(() -> {
                    working = false;
                    if (isFinishing() || isDestroyed()) return;
                    startActivity(new Intent(this, GameActivity.class).putExtra("contentRoot", root.getAbsolutePath()));
                    finish();
                });
            } catch (Exception error) {
                android.util.Log.e("FlyBootstrap", "Resource preparation failed", error);
                runOnUiThread(() -> {
                    working = false;
                    if (isFinishing() || isDestroyed()) return;
                    status.setText("Resource preparation failed / 资源准备失败\n" + error.getMessage());
                    retry.setVisibility(android.view.View.VISIBLE);
                });
            }
        }, "GameAssetInstaller").start();
    }
}
