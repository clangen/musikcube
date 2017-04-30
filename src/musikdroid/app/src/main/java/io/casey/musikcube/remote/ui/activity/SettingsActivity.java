package io.casey.musikcube.remote.ui.activity;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v7.app.AppCompatActivity;
import android.view.MenuItem;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.Spinner;

import java.util.Locale;

import io.casey.musikcube.remote.R;
import io.casey.musikcube.remote.playback.MediaPlayerWrapper;
import io.casey.musikcube.remote.playback.PlaybackService;
import io.casey.musikcube.remote.playback.PlaybackServiceFactory;
import io.casey.musikcube.remote.ui.util.Views;
import io.casey.musikcube.remote.websocket.WebSocketService;

public class SettingsActivity extends AppCompatActivity {
    private EditText addressText, portText, httpPortText, passwordText;
    private CheckBox albumArtCheckbox, messageCompressionCheckbox, softwareVolume;
    private Spinner playbackModeSpinner;
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

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == android.R.id.home) {
            finish();
            return true;
        }

        return super.onOptionsItemSelected(item);
    }

    private void rebindUi() {
        Views.setTextAndMoveCursorToEnd(this.addressText, prefs.getString("address", "192.168.1.100"));
        Views.setTextAndMoveCursorToEnd(this.portText, String.format(Locale.ENGLISH, "%d", prefs.getInt("port", 7905)));
        Views.setTextAndMoveCursorToEnd(this.httpPortText, String.format(Locale.ENGLISH, "%d", prefs.getInt("http_port", 7906)));
        Views.setTextAndMoveCursorToEnd(this.passwordText, prefs.getString("password", ""));

        final ArrayAdapter<CharSequence> playbackModes = ArrayAdapter.createFromResource(
            this, R.array.streaming_mode_array, android.R.layout.simple_spinner_item);

        playbackModes.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        playbackModeSpinner.setAdapter(playbackModes);

        playbackModeSpinner.setSelection(isStreamingEnabled() ? 1 : 0);

        this.albumArtCheckbox.setChecked(this.prefs.getBoolean("album_art_enabled", true));
        this.messageCompressionCheckbox.setChecked(this.prefs.getBoolean("message_compression_enabled", true));
        this.softwareVolume.setChecked(this.prefs.getBoolean("software_volume", false));

        Views.enableUpNavigation(this);
    }

    private boolean isStreamingEnabled() {
        return this.prefs.getBoolean("streaming_playback", false);
    }

    private boolean isStreamingSelected() {
        return this.playbackModeSpinner.getSelectedItemPosition() == 1;
    }

    private void bindEventListeners() {
        this.addressText = (EditText) this.findViewById(R.id.address);
        this.portText = (EditText) this.findViewById(R.id.port);
        this.httpPortText = (EditText) this.findViewById(R.id.http_port);
        this.passwordText = (EditText) this.findViewById(R.id.password);
        this.albumArtCheckbox = (CheckBox) findViewById(R.id.album_art_checkbox);
        this.messageCompressionCheckbox = (CheckBox) findViewById(R.id.message_compression);
        this.softwareVolume = (CheckBox) findViewById(R.id.software_volume);
        this.playbackModeSpinner = (Spinner) findViewById(R.id.playback_mode_spinner);

        final boolean wasStreaming = isStreamingEnabled();

        this.findViewById(R.id.button_connect).setOnClickListener((View v) -> {
            final String addr = addressText.getText().toString();
            final String port = portText.getText().toString();
            final String httpPort = httpPortText.getText().toString();
            final String password = passwordText.getText().toString();

            prefs.edit()
                .putString("address", addr)
                .putInt("port", (port.length() > 0) ? Integer.valueOf(port) : 0)
                .putInt("http_port", (httpPort.length() > 0) ? Integer.valueOf(httpPort) : 0)
                .putString("password", password)
                .putBoolean("album_art_enabled", albumArtCheckbox.isChecked())
                .putBoolean("message_compression_enabled", messageCompressionCheckbox.isChecked())
                .putBoolean("streaming_playback", isStreamingSelected())
                .putBoolean("software_volume", softwareVolume.isChecked())
                .apply();

            if (!softwareVolume.isChecked()) {
                MediaPlayerWrapper.setGlobalVolume(1.0f);
            }

            if (wasStreaming && !isStreamingEnabled()) {
                PlaybackServiceFactory.streaming(this).stop();
            }

            WebSocketService.getInstance(this).disconnect();

            finish();
        });
    }
}
