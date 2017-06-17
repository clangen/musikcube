package io.casey.musikcube.remote;

import android.app.Dialog;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.Handler;
import android.support.design.widget.Snackbar;
import android.support.v4.app.DialogFragment;
import android.support.v4.content.ContextCompat;
import android.support.v7.app.AlertDialog;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.SeekBar;
import android.widget.TextView;

import java.util.HashMap;
import java.util.Map;

import io.casey.musikcube.remote.playback.PlaybackService;
import io.casey.musikcube.remote.playback.PlaybackState;
import io.casey.musikcube.remote.playback.RepeatMode;
import io.casey.musikcube.remote.ui.activity.AlbumBrowseActivity;
import io.casey.musikcube.remote.ui.activity.CategoryBrowseActivity;
import io.casey.musikcube.remote.ui.activity.PlayQueueActivity;
import io.casey.musikcube.remote.ui.activity.SettingsActivity;
import io.casey.musikcube.remote.ui.activity.TrackListActivity;
import io.casey.musikcube.remote.ui.activity.WebSocketActivityBase;
import io.casey.musikcube.remote.ui.fragment.InvalidPasswordDialogFragment;
import io.casey.musikcube.remote.ui.util.Views;
import io.casey.musikcube.remote.ui.view.MainMetadataView;
import io.casey.musikcube.remote.util.Duration;
import io.casey.musikcube.remote.websocket.Messages;
import io.casey.musikcube.remote.websocket.Prefs;
import io.casey.musikcube.remote.websocket.SocketMessage;
import io.casey.musikcube.remote.websocket.WebSocketService;

public class MainActivity extends WebSocketActivityBase {
    private static Map<RepeatMode, Integer> REPEAT_TO_STRING_ID;

    private WebSocketService wss = null;

    private Handler handler = new Handler();
    private SharedPreferences prefs;
    private PlaybackService playback;

    private View mainLayout;
    private MainMetadataView metadataView;
    private TextView playPause, currentTime, totalTime;
    private View connectedNotPlayingContainer, disconnectedButton, showOfflineButton;
    private View disconnectedContainer;
    private CheckBox shuffleCb, muteCb, repeatCb;
    private View disconnectedOverlay;
    private SeekBar seekbar;
    private int seekbarValue = -1;
    private int blink = 0;

    static {
        REPEAT_TO_STRING_ID = new HashMap<>();
        REPEAT_TO_STRING_ID.put(RepeatMode.None, R.string.button_repeat_off);
        REPEAT_TO_STRING_ID.put(RepeatMode.List, R.string.button_repeat_list);
        REPEAT_TO_STRING_ID.put(RepeatMode.Track, R.string.button_repeat_track);
    }

    public static Intent getStartIntent(final Context context) {
        return new Intent(context, MainActivity.class)
            .addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_CLEAR_TOP);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        this.prefs = this.getSharedPreferences(Prefs.NAME, Context.MODE_PRIVATE);
        this.wss = getWebSocketService();
        this.playback = getPlaybackService();

        setContentView(R.layout.activity_main);
        bindEventListeners();

        if (!this.wss.hasValidConnection()) {
            startActivity(SettingsActivity.getStartIntent(this));
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        metadataView.onPause();
        unbindCheckboxEventListeners();
        handler.removeCallbacks(updateTimeRunnable);
    }

    @Override
    protected void onResume() {
        super.onResume();
        this.playback = getPlaybackService();
        metadataView.onResume();
        bindCheckBoxEventListeners();
        rebindUi();
        scheduleUpdateTime(true);
    }

    @Override
    protected void onRestoreInstanceState(Bundle savedInstanceState) {
        super.onRestoreInstanceState(savedInstanceState);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.main_menu, menu);
        return true;
    }

    @Override
    public boolean onPrepareOptionsMenu(Menu menu) {
        boolean connected = wss.getState() == WebSocketService.State.Connected;
        boolean streaming = isStreamingSelected();

        menu.findItem(R.id.action_playlists).setEnabled(connected);
        menu.findItem(R.id.action_genres).setEnabled(connected);

        menu.findItem(R.id.action_remote_toggle).setIcon(
            streaming ? R.mipmap.ic_toolbar_streaming : R.mipmap.ic_toolbar_remote);

        return super.onPrepareOptionsMenu(menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.action_remote_toggle:
                togglePlaybackService();
                return true;

            case R.id.action_settings:
                startActivity(SettingsActivity.getStartIntent(this));
                return true;

            case R.id.action_genres:
                startActivity(CategoryBrowseActivity.getStartIntent(this, Messages.Category.GENRE));
                return true;

            case R.id.action_playlists:
                startActivity(CategoryBrowseActivity.getStartIntent(
                        this, Messages.Category.PLAYLISTS, CategoryBrowseActivity.DeepLink.TRACKS));
                return true;

            case R.id.action_offline_tracks:
                onOfflineTracksSelected();
                return true;
        }

        return super.onOptionsItemSelected(item);
    }

    @Override
    protected WebSocketService.Client getWebSocketServiceClient() {
        return this.serviceClient;
    }

    @Override
    protected PlaybackService.EventListener getPlaybackServiceEventListener() {
        return this.playbackEvents;
    }

    private void onOfflineTracksSelected() {
        if (isStreamingSelected()) {
            startActivity(TrackListActivity.getOfflineStartIntent(this));
        }
        else {
            final String tag = SwitchToOfflineTracksDialog.TAG;
            if (getSupportFragmentManager().findFragmentByTag(tag) == null) {
                SwitchToOfflineTracksDialog.newInstance().show(getSupportFragmentManager(), tag);
            }
        }
    }

    private void onConfirmSwitchToOfflineTracks() {
        togglePlaybackService();
        onOfflineTracksSelected();
    }

    private boolean isStreamingSelected() {
        return prefs.getBoolean(
            Prefs.Key.STREAMING_PLAYBACK,
            Prefs.Default.STREAMING_PLAYBACK);
    }

    private void togglePlaybackService() {
        final boolean streaming = isStreamingSelected();

        if (streaming) {
            playback.stop();
        }

        prefs.edit().putBoolean(Prefs.Key.STREAMING_PLAYBACK, !streaming).apply();

        final int messageId = streaming
            ? R.string.snackbar_remote_enabled
            : R.string.snackbar_streaming_enabled;

        showSnackbar(messageId);

        reloadPlaybackService();
        this.playback = getPlaybackService();

        supportInvalidateOptionsMenu();
        rebindUi();
    }

    private void showSnackbar(int stringId) {
        final Snackbar sb = Snackbar.make(mainLayout, stringId, Snackbar.LENGTH_LONG);
        final View sbView = sb.getView();
        sbView.setBackgroundColor(ContextCompat.getColor(this, R.color.color_primary));
        final TextView tv = (TextView) sbView.findViewById(android.support.design.R.id.snackbar_text);
        tv.setTextColor(ContextCompat.getColor(this, R.color.theme_foreground));
        sb.show();
    }

    private void bindCheckBoxEventListeners() {
        this.shuffleCb.setOnCheckedChangeListener(shuffleListener);
        this.muteCb.setOnCheckedChangeListener(muteListener);
        this.repeatCb.setOnCheckedChangeListener(repeatListener);
    }

    /* onRestoreInstanceState() calls setChecked(), which has the side effect of
    running these callbacks. this screws up state, especially for the repeat checkbox */
    private void unbindCheckboxEventListeners() {
        this.shuffleCb.setOnCheckedChangeListener(null);
        this.muteCb.setOnCheckedChangeListener(null);
        this.repeatCb.setOnCheckedChangeListener(null);
    }

    private void bindEventListeners() {
        this.mainLayout = findViewById(R.id.activity_main);
        this.metadataView = (MainMetadataView) findViewById(R.id.main_metadata_view);
        this.shuffleCb = (CheckBox) findViewById(R.id.check_shuffle);
        this.muteCb = (CheckBox) findViewById(R.id.check_mute);
        this.repeatCb = (CheckBox) findViewById(R.id.check_repeat);
        this.connectedNotPlayingContainer = findViewById(R.id.connected_not_playing);
        this.disconnectedButton = findViewById(R.id.disconnected_button);
        this.disconnectedContainer = findViewById(R.id.disconnected_container);
        this.disconnectedOverlay = findViewById(R.id.disconnected_overlay);
        this.showOfflineButton = findViewById(R.id.offline_tracks_button);
        this.playPause = (TextView) findViewById(R.id.button_play_pause);
        this.currentTime = (TextView) findViewById(R.id.current_time);
        this.totalTime = (TextView) findViewById(R.id.total_time);
        this.seekbar = (SeekBar) findViewById(R.id.seekbar);

        findViewById(R.id.button_prev).setOnClickListener((View view) -> playback.prev());

        findViewById(R.id.button_play_pause).setOnClickListener((View view) -> {
            if (playback.getPlaybackState() == PlaybackState.Stopped) {
                playback.playAll();
            }
            else {
                playback.pauseOrResume();
            }
        });

        findViewById(R.id.button_next).setOnClickListener((View view) -> playback.next());

        disconnectedButton.setOnClickListener((view) -> {
            wss.reconnect();
        });

        seekbar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                if (fromUser) {
                    seekbarValue = progress;
                    currentTime.setText(Duration.format(seekbarValue));
                }
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {

            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                if (seekbarValue != -1) {
                    playback.seekTo((double) seekbarValue);
                    seekbarValue = -1;
                }
            }
        });

        findViewById(R.id.button_artists).setOnClickListener((View view) -> {
            startActivity(CategoryBrowseActivity.getStartIntent(this, Messages.Category.ALBUM_ARTIST));
        });

        findViewById(R.id.button_tracks).setOnClickListener((View view) -> {
            startActivity(TrackListActivity.getStartIntent(MainActivity.this));
        });

        findViewById(R.id.button_albums).setOnClickListener((View view) -> {
            startActivity(AlbumBrowseActivity.getStartIntent(MainActivity.this));
        });

        findViewById(R.id.button_play_queue).setOnClickListener((view) -> navigateToPlayQueue());

        findViewById(R.id.metadata_container).setOnClickListener((view) -> {
            if (playback.getQueueCount() > 0) {
                navigateToPlayQueue();
            }
        });

        disconnectedOverlay.setOnClickListener(view -> {
            /* swallow input so user can't click on things while disconnected */
        });

        showOfflineButton.setOnClickListener(view -> onOfflineTracksSelected());
    }

    private void rebindUi() {
        if (this.playback == null) {
            throw new IllegalStateException();
        }

        final PlaybackState playbackState = playback.getPlaybackState();
        final boolean streaming = prefs.getBoolean(Prefs.Key.STREAMING_PLAYBACK, Prefs.Default.STREAMING_PLAYBACK);
        final boolean connected = (wss.getState() == WebSocketService.State.Connected);
        final boolean stopped = (playbackState == PlaybackState.Stopped);
        final boolean playing = (playbackState == PlaybackState.Playing);
        final boolean buffering = (playbackState == PlaybackState.Buffering);
        final boolean showMetadataView = !stopped && /*connected &&*/ playback.getQueueCount() > 0;

        /* bottom section: transport controls */
        this.playPause.setText(playing || buffering ? R.string.button_pause : R.string.button_play);

        this.connectedNotPlayingContainer.setVisibility((connected && stopped) ? View.VISIBLE : View.GONE);
        this.disconnectedOverlay.setVisibility(connected || !stopped ? View.GONE : View.VISIBLE);

        final RepeatMode repeatMode = playback.getRepeatMode();
        final boolean repeatChecked = (repeatMode != RepeatMode.None);
        repeatCb.setText(REPEAT_TO_STRING_ID.get(repeatMode));
        Views.setCheckWithoutEvent(repeatCb, repeatChecked, this.repeatListener);

        this.shuffleCb.setText(streaming ? R.string.button_random : R.string.button_shuffle);
        Views.setCheckWithoutEvent(this.shuffleCb, playback.isShuffled(), this.shuffleListener);

        Views.setCheckWithoutEvent(this.muteCb, playback.isMuted(), this.muteListener);

        /* middle section: connected, disconnected, and metadata views */
        connectedNotPlayingContainer.setVisibility(View.GONE);
        disconnectedContainer.setVisibility(View.GONE);

        if (!showMetadataView) {
            metadataView.hide();

            if (!connected) {
                disconnectedContainer.setVisibility(View.VISIBLE);
            }
            else if (stopped) {
                connectedNotPlayingContainer.setVisibility(View.VISIBLE);
            }
        }
        else {
            metadataView.refresh();
        }
    }

    private void clearUi() {
        metadataView.clear();
        rebindUi();
    }

    private void navigateToPlayQueue() {
        startActivity(PlayQueueActivity.getStartIntent(MainActivity.this, playback.getQueuePosition()));
    }

    private void scheduleUpdateTime(boolean immediate) {
        handler.removeCallbacks(updateTimeRunnable);
        handler.postDelayed(updateTimeRunnable, immediate ? 0 : 1000);
    }

    private Runnable updateTimeRunnable = () -> {
        final double duration = playback.getDuration();
        final double current = (seekbarValue == -1) ? playback.getCurrentTime() : seekbarValue;

        currentTime.setText(Duration.format(current));
        totalTime.setText(Duration.format(duration));

        seekbar.setMax((int) duration);
        seekbar.setProgress((int) current);
        seekbar.setSecondaryProgress((int) playback.getBufferedTime());

        int currentTimeColor = R.color.theme_foreground;
        if (playback.getPlaybackState() == PlaybackState.Paused) {
            currentTimeColor = ++blink % 2 == 0
                ? R.color.theme_foreground
                : R.color.theme_blink_foreground;
        }

        currentTime.setTextColor(ContextCompat.getColor(this, currentTimeColor));

        scheduleUpdateTime(false);
    };

    private CheckBox.OnCheckedChangeListener muteListener =
        (CompoundButton compoundButton, boolean b) -> {
            if (b != playback.isMuted()) {
                playback.toggleMute();
            }
        };

    private CheckBox.OnCheckedChangeListener shuffleListener =
        (CompoundButton compoundButton, boolean b) -> {
            if (b != playback.isShuffled()) {
                playback.toggleShuffle();
            }
        };

    final CheckBox.OnCheckedChangeListener repeatListener = new CompoundButton.OnCheckedChangeListener() {
        @Override
        public void onCheckedChanged(CompoundButton compoundButton, boolean b) {
            final RepeatMode currentMode = playback.getRepeatMode();

            RepeatMode newMode = RepeatMode.None;

            if (currentMode == RepeatMode.None) {
                newMode = RepeatMode.List;
            }
            else if (currentMode == RepeatMode.List) {
                newMode = RepeatMode.Track;
            }

            final boolean checked = (newMode != RepeatMode.None);
            compoundButton.setText(REPEAT_TO_STRING_ID.get(newMode));
            Views.setCheckWithoutEvent(repeatCb, checked, this);

            playback.toggleRepeatMode();
        }
    };

    private PlaybackService.EventListener playbackEvents = () -> rebindUi();

    private WebSocketService.Client serviceClient = new WebSocketService.Client() {
        @Override
        public void onStateChanged(WebSocketService.State newState, WebSocketService.State oldState) {
            if (newState == WebSocketService.State.Connected) {
                rebindUi();
            }
            else if (newState == WebSocketService.State.Disconnected) {
                clearUi();
            }
        }

        @Override
        public void onMessageReceived(SocketMessage message) {
        }

        @Override
        public void onInvalidPassword() {
            final String tag = InvalidPasswordDialogFragment.TAG;
            if (getSupportFragmentManager().findFragmentByTag(tag) == null) {
                InvalidPasswordDialogFragment.newInstance().show(getSupportFragmentManager(), tag);
            }
        }
    };

    public static class SwitchToOfflineTracksDialog extends DialogFragment {
        public static final String TAG = "switch_to_offline_tracks_dialog";

        public static SwitchToOfflineTracksDialog newInstance() {
            return new SwitchToOfflineTracksDialog();
        }

        @Override
        public Dialog onCreateDialog(Bundle savedInstanceState) {
            final AlertDialog dlg = new AlertDialog.Builder(getActivity())
                    .setTitle(R.string.main_switch_to_streaming_title)
                    .setMessage(R.string.main_switch_to_streaming_message)
                    .setNegativeButton(R.string.button_no, null)
                    .setPositiveButton(R.string.button_yes, (dialog, which) -> {
                        ((MainActivity) getActivity()).onConfirmSwitchToOfflineTracks();
                    })
                    .create();

            dlg.setCancelable(false);
            return dlg;
        }
    }
}
