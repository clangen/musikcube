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
import android.view.MenuItem;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.Spinner;

import java.util.Locale;

import io.casey.musikcube.remote.R;
import io.casey.musikcube.remote.playback.MediaPlayerWrapper;
import io.casey.musikcube.remote.playback.PlaybackServiceFactory;
import io.casey.musikcube.remote.ui.util.Views;
import io.casey.musikcube.remote.websocket.WebSocketService;

public class SettingsActivity extends AppCompatActivity {
    private EditText addressText, portText, httpPortText, passwordText;
    private CheckBox albumArtCheckbox, messageCompressionCheckbox, softwareVolume;
    private CheckBox sslCheckbox, certCheckbox;
    private Spinner playbackModeSpinner, bitrateSpinner;
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

        final ArrayAdapter<CharSequence> bitrates = ArrayAdapter.createFromResource(
            this, R.array.transcode_bitrate_array, android.R.layout.simple_spinner_item);

        bitrates.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        bitrateSpinner.setAdapter(bitrates);
        bitrateSpinner.setSelection(prefs.getInt("transcoder_bitrate_index", 0));

        this.albumArtCheckbox.setChecked(this.prefs.getBoolean("album_art_enabled", true));
        this.messageCompressionCheckbox.setChecked(this.prefs.getBoolean("message_compression_enabled", true));
        this.softwareVolume.setChecked(this.prefs.getBoolean("software_volume", false));
        this.certCheckbox.setChecked(this.prefs.getBoolean("cert_validation_disabled", false));

        Views.setCheckWithoutEvent(
            this.sslCheckbox,
            this.prefs.getBoolean("ssl_enabled", false),
            sslCheckChanged);

        Views.setCheckWithoutEvent(
            this.certCheckbox,
            this.prefs.getBoolean("cert_validation_disabled", false),
            certValidationChanged);

        Views.enableUpNavigation(this);
    }

    private boolean isStreamingEnabled() {
        return this.prefs.getBoolean("streaming_playback", false);
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
        this.sslCheckbox = (CheckBox) findViewById(R.id.ssl_checkbox);
        this.certCheckbox = (CheckBox) findViewById(R.id.cert_validation);

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
                .putBoolean("ssl_enabled", sslCheckbox.isChecked())
                .putBoolean("cert_validation_disabled", certCheckbox.isChecked())
                .putInt("transcoder_bitrate_index", bitrateSpinner.getSelectedItemPosition())
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
