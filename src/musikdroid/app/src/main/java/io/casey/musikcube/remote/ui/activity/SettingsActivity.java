package io.casey.musikcube.remote.ui.activity;

import android.app.Dialog;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v4.app.DialogFragment;
import android.support.v7.app.AlertDialog;
import android.support.v7.app.AppCompatActivity;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.Spinner;

import java.util.Locale;

import io.casey.musikcube.remote.R;
import io.casey.musikcube.remote.playback.MediaPlayerWrapper;
import io.casey.musikcube.remote.playback.PlaybackServiceFactory;
import io.casey.musikcube.remote.playback.StreamProxy;
import io.casey.musikcube.remote.ui.util.Views;
import io.casey.musikcube.remote.websocket.Prefs;
import io.casey.musikcube.remote.websocket.WebSocketService;

public class SettingsActivity extends AppCompatActivity {
    private EditText addressText, portText, httpPortText, passwordText;
    private CheckBox albumArtCheckbox, messageCompressionCheckbox, softwareVolume;
    private CheckBox sslCheckbox, certCheckbox;
    private Spinner playbackModeSpinner, bitrateSpinner, cacheSpinner;
    private SharedPreferences prefs;
    private boolean wasStreaming;

    public static Intent getStartIntent(final Context context) {
        return new Intent(context, SettingsActivity.class);
    }

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        prefs = this.getSharedPreferences(Prefs.NAME, MODE_PRIVATE);
        setContentView(R.layout.activity_settings);
        setTitle(R.string.settings_title);
        wasStreaming = isStreamingEnabled();
        bindEventListeners();
        rebindUi();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.settings_menu, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == android.R.id.home) {
            finish();
            return true;
        }
        else if (item.getItemId() == R.id.action_save) {
            save();
            return true;
        }

        return super.onOptionsItemSelected(item);
    }

    private void rebindUi() {
        Views.setTextAndMoveCursorToEnd(this.addressText, prefs.getString(Prefs.Key.ADDRESS, Prefs.Default.ADDRESS));
        Views.setTextAndMoveCursorToEnd(this.portText, String.format(Locale.ENGLISH, "%d", prefs.getInt(Prefs.Key.MAIN_PORT, Prefs.Default.MAIN_PORT)));
        Views.setTextAndMoveCursorToEnd(this.httpPortText, String.format(Locale.ENGLISH, "%d", prefs.getInt(Prefs.Key.AUDIO_PORT, Prefs.Default.AUDIO_PORT)));
        Views.setTextAndMoveCursorToEnd(this.passwordText, prefs.getString(Prefs.Key.PASSWORD, Prefs.Default.PASSWORD));

        final ArrayAdapter<CharSequence> playbackModes = ArrayAdapter.createFromResource(
            this, R.array.streaming_mode_array, android.R.layout.simple_spinner_item);

        playbackModes.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        playbackModeSpinner.setAdapter(playbackModes);
        playbackModeSpinner.setSelection(isStreamingEnabled() ? 1 : 0);

        final ArrayAdapter<CharSequence> bitrates = ArrayAdapter.createFromResource(
            this, R.array.transcode_bitrate_array, android.R.layout.simple_spinner_item);

        bitrates.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        bitrateSpinner.setAdapter(bitrates);
        bitrateSpinner.setSelection(prefs.getInt(Prefs.Key.TRANSCODER_BITRATE_INDEX, Prefs.Default.TRANSCODER_BITRATE_INDEX));

        final ArrayAdapter<CharSequence> cacheSizes = ArrayAdapter.createFromResource(
            this, R.array.disk_cache_array, android.R.layout.simple_spinner_item);

        cacheSizes.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        cacheSpinner.setAdapter(cacheSizes);
        cacheSpinner.setSelection(prefs.getInt(Prefs.Key.DISK_CACHE_SIZE_INDEX, Prefs.Default.DISK_CACHE_SIZE_INDEX));

        this.albumArtCheckbox.setChecked(this.prefs.getBoolean(Prefs.Key.ALBUM_ART_ENABLED, Prefs.Default.ALBUM_ART_ENABLED));
        this.messageCompressionCheckbox.setChecked(this.prefs.getBoolean(Prefs.Key.MESSAGE_COMPRESSION_ENABLED, Prefs.Default.MESSAGE_COMPRESSION_ENABLED));
        this.softwareVolume.setChecked(this.prefs.getBoolean(Prefs.Key.SOFTWARE_VOLUME, Prefs.Default.SOFTWARE_VOLUME));

        Views.setCheckWithoutEvent(
            this.sslCheckbox,
            this.prefs.getBoolean(
                Prefs.Key.SSL_ENABLED,
                Prefs.Default.SSL_ENABLED),
            sslCheckChanged);

        Views.setCheckWithoutEvent(
            this.certCheckbox,
            this.prefs.getBoolean(
                Prefs.Key.CERT_VALIDATION_DISABLED,
                Prefs.Default.CERT_VALIDATION_DISABLED),
            certValidationChanged);

        Views.enableUpNavigation(this);
    }

    private boolean isStreamingEnabled() {
        return this.prefs.getBoolean(Prefs.Key.STREAMING_PLAYBACK, Prefs.Default.STREAMING_PLAYBACK);
    }

    private boolean isStreamingSelected() {
        return this.playbackModeSpinner.getSelectedItemPosition() == 1;
    }

    private void onDisableSslFromDialog() {
        Views.setCheckWithoutEvent(this.sslCheckbox, false, sslCheckChanged);
    }

    private void onDisableCertValidationFromDialog() {
        Views.setCheckWithoutEvent(this.certCheckbox, false, certValidationChanged);
    }

    private CheckBox.OnCheckedChangeListener sslCheckChanged = (button, value) -> {
        if (value) {
            if (getSupportFragmentManager().findFragmentByTag(SslAlertDialog.TAG) == null) {
                SslAlertDialog.newInstance().show(getSupportFragmentManager(), SslAlertDialog.TAG);
            }
        }
    };

    private CheckBox.OnCheckedChangeListener certValidationChanged = (button, value) -> {
        if (value) {
            if (getSupportFragmentManager().findFragmentByTag(DisableCertValidationAlertDialog.TAG) == null) {
                DisableCertValidationAlertDialog.newInstance().show(
                    getSupportFragmentManager(), DisableCertValidationAlertDialog.TAG);
            }
        }
    };

    private void bindEventListeners() {
        this.addressText = (EditText) this.findViewById(R.id.address);
        this.portText = (EditText) this.findViewById(R.id.port);
        this.httpPortText = (EditText) this.findViewById(R.id.http_port);
        this.passwordText = (EditText) this.findViewById(R.id.password);
        this.albumArtCheckbox = (CheckBox) findViewById(R.id.album_art_checkbox);
        this.messageCompressionCheckbox = (CheckBox) findViewById(R.id.message_compression);
        this.softwareVolume = (CheckBox) findViewById(R.id.software_volume);
        this.playbackModeSpinner = (Spinner) findViewById(R.id.playback_mode_spinner);
        this.bitrateSpinner = (Spinner) findViewById(R.id.transcoder_bitrate_spinner);
        this.cacheSpinner = (Spinner) findViewById(R.id.streaming_disk_cache_spinner);
        this.sslCheckbox = (CheckBox) findViewById(R.id.ssl_checkbox);
        this.certCheckbox = (CheckBox) findViewById(R.id.cert_validation);

        this.playbackModeSpinner.setOnItemSelectedListener(new Spinner.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> adapterView, View view, int selectedIndex, long l) {
                final boolean streaming = (selectedIndex == 1);
                bitrateSpinner.setEnabled(streaming);
                cacheSpinner.setEnabled(streaming);
            }

            @Override
            public void onNothingSelected(AdapterView<?> adapterView) {

            }
        });
   }

    private void save() {
        final String addr = addressText.getText().toString();
        final String port = portText.getText().toString();
        final String httpPort = httpPortText.getText().toString();
        final String password = passwordText.getText().toString();

        prefs.edit()
            .putString(Prefs.Key.ADDRESS, addr)
            .putInt(Prefs.Key.MAIN_PORT, (port.length() > 0) ? Integer.valueOf(port) : 0)
            .putInt(Prefs.Key.AUDIO_PORT, (httpPort.length() > 0) ? Integer.valueOf(httpPort) : 0)
            .putString(Prefs.Key.PASSWORD, password)
            .putBoolean(Prefs.Key.ALBUM_ART_ENABLED, albumArtCheckbox.isChecked())
            .putBoolean(Prefs.Key.MESSAGE_COMPRESSION_ENABLED, messageCompressionCheckbox.isChecked())
            .putBoolean(Prefs.Key.STREAMING_PLAYBACK, isStreamingSelected())
            .putBoolean(Prefs.Key.SOFTWARE_VOLUME, softwareVolume.isChecked())
            .putBoolean(Prefs.Key.SSL_ENABLED, sslCheckbox.isChecked())
            .putBoolean(Prefs.Key.CERT_VALIDATION_DISABLED, certCheckbox.isChecked())
            .putInt(Prefs.Key.TRANSCODER_BITRATE_INDEX, bitrateSpinner.getSelectedItemPosition())
            .putInt(Prefs.Key.DISK_CACHE_SIZE_INDEX, cacheSpinner.getSelectedItemPosition())
            .apply();

        if (!softwareVolume.isChecked()) {
            MediaPlayerWrapper.setGlobalVolume(1.0f);
        }

        if (wasStreaming && !isStreamingEnabled()) {
            PlaybackServiceFactory.streaming(this).stop();
        }

        StreamProxy.reload();
        WebSocketService.getInstance(this).disconnect();

        finish();
    }

    public static class SslAlertDialog extends DialogFragment {
        private static final String LEARN_MORE_URL = "https://github.com/clangen/musikcube/wiki/ssl-server-setup";
        public static final String TAG = "ssl_alert_dialog_tag";

        public static SslAlertDialog newInstance() {
            return new SslAlertDialog();
        }

        @Override
        public Dialog onCreateDialog(Bundle savedInstanceState) {
            final AlertDialog dlg = new AlertDialog.Builder(getActivity())
                .setTitle(R.string.settings_ssl_dialog_title)
                .setMessage(R.string.settings_ssl_dialog_message)
                .setPositiveButton(R.string.button_enable, null)
                .setNegativeButton(R.string.button_disable, (dialog, which) -> {
                    ((SettingsActivity) getActivity()).onDisableSslFromDialog();
                })
                .setNeutralButton(R.string.button_learn_more, (dialog, which) -> {
                    try {
                        final Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(LEARN_MORE_URL));
                        startActivity(intent);
                    }
                    catch (Exception ex) {
                    }
                })
                .create();

            dlg.setCancelable(false);
            return dlg;
        }
    }

    public static class DisableCertValidationAlertDialog extends DialogFragment {
        public static final String TAG = "disable_cert_verify_dialog";

        public static DisableCertValidationAlertDialog newInstance() {
            return new DisableCertValidationAlertDialog();
        }

        @Override
        public Dialog onCreateDialog(Bundle savedInstanceState) {
            final AlertDialog dlg = new AlertDialog.Builder(getActivity())
                .setTitle(R.string.settings_disable_cert_validation_title)
                .setMessage(R.string.settings_disable_cert_validation_message)
                .setPositiveButton(R.string.button_enable, null)
                .setNegativeButton(R.string.button_disable, (dialog, which) -> {
                    ((SettingsActivity) getActivity()).onDisableCertValidationFromDialog();
                })
                .create();

            dlg.setCancelable(false);
            return dlg;
        }
    }
}
