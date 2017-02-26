package io.casey.musikcube.remote;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v7.app.AppCompatActivity;
import android.view.View;
import android.widget.CheckBox;
import android.widget.EditText;

import java.util.Locale;

public class SettingsActivity extends AppCompatActivity {
    private EditText addressText, portText, passwordText;
    private CheckBox albumArtCheckbox, messageCompressionCheckbox;
    private SharedPreferences prefs;

    public static Intent getStartIntent(final Context context) {
        return new Intent(context, SettingsActivity.class);
    }

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        prefs = this.getSharedPreferences("prefs", MODE_PRIVATE);
        setContentView(R.layout.activity_settings);
        setTitle(R.string.settings_title);
        bindEventListeners();
        rebindUi();
    }

    private void rebindUi() {
        Views.setTextAndMoveCursorToEnd(this.addressText, prefs.getString("address", "192.168.1.100"));
        Views.setTextAndMoveCursorToEnd(this.portText, String.format(Locale.ENGLISH, "%d", prefs.getInt("port", 7905)));
        Views.setTextAndMoveCursorToEnd(this.passwordText, prefs.getString("password", ""));
    }

    private void bindEventListeners() {
        this.addressText = (EditText) this.findViewById(R.id.address);
        this.portText = (EditText) this.findViewById(R.id.port);
        this.passwordText = (EditText) this.findViewById(R.id.password);
        this.albumArtCheckbox = (CheckBox) findViewById(R.id.album_art_checkbox);
        this.messageCompressionCheckbox = (CheckBox) findViewById(R.id.message_compression);

        this.albumArtCheckbox.setChecked(this.prefs.getBoolean("album_art_enabled", true));
        this.messageCompressionCheckbox.setChecked(this.prefs.getBoolean("message_compression_enabled", true));

        this.findViewById(R.id.button_connect).setOnClickListener((View v) -> {
            final String addr = addressText.getText().toString();
            final String port = portText.getText().toString();
            final String password = passwordText.getText().toString();

            prefs.edit()
                .putString("address", addr)
                .putInt("port", (port.length() > 0) ? Integer.valueOf(port) : 0)
                .putString("password", password)
                .putBoolean("album_art_enabled", albumArtCheckbox.isChecked())
                .putBoolean("message_compression_enabled", messageCompressionCheckbox.isChecked())
                .apply();

            WebSocketService.getInstance(this).disconnect();

            finish();
        });
    }
}
