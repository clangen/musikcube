package io.casey.musikcube.remote.ui.activity;

import android.content.Context;
import android.content.SharedPreferences;
import android.media.AudioManager;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v7.app.AppCompatActivity;
import android.view.KeyEvent;
import android.view.MenuItem;

import com.uacf.taskrunner.LifecycleDelegate;
import com.uacf.taskrunner.Runner;
import com.uacf.taskrunner.Task;

import io.casey.musikcube.remote.playback.PlaybackService;
import io.casey.musikcube.remote.playback.PlaybackServiceFactory;
import io.casey.musikcube.remote.websocket.WebSocketService;

public abstract class WebSocketActivityBase extends AppCompatActivity implements Runner.TaskCallbacks {
    private WebSocketService wss;
    private boolean paused = true;
    private LifecycleDelegate runnerDelegate;
    private PlaybackService playback;
    private SharedPreferences prefs;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setVolumeControlStream(AudioManager.STREAM_MUSIC);
        this.runnerDelegate = new LifecycleDelegate(this, this, getClass(), null);
        this.runnerDelegate.onCreate(savedInstanceState);
        this.wss = WebSocketService.getInstance(this);
        this.playback = PlaybackServiceFactory.instance(this);
        this.prefs = getSharedPreferences("prefs", Context.MODE_PRIVATE);
    }

    @Override
    protected void onPause() {
        super.onPause();
        this.runnerDelegate.onPause();
        this.wss.removeClient(getWebSocketServiceClient());
        this.playback.disconnect(getPlaybackServiceEventListener());
        this.paused = true;
    }

    @Override
    protected void onResume() {
        super.onResume();
        this.runnerDelegate.onResume();
        this.playback = PlaybackServiceFactory.instance(this);
        this.playback.connect(getPlaybackServiceEventListener());
        this.wss.addClient(getWebSocketServiceClient());
        this.paused = false;
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        this.runnerDelegate.onDestroy();
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        this.runnerDelegate.onSaveInstanceState(outState);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        boolean streaming = prefs.getBoolean("streaming_playback", false);

        /* if we're not streaming we want the hardware buttons to go out to the system */
        if (!streaming) {
            if (keyCode == KeyEvent.KEYCODE_VOLUME_DOWN) {
                playback.volumeDown();
                return true;
            }
            else if (keyCode == KeyEvent.KEYCODE_VOLUME_UP) {
                playback.volumeUp();
                return true;
            }
        }

        return super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == android.R.id.home) {
            finish();
            return true;
        }

        return super.onOptionsItemSelected(item);
    }

    @Override
    public void onTaskCompleted(String s, long l, Task task, Object o) {

    }

    @Override
    public void onTaskError(String s, long l, Task task, Throwable throwable) {

    }

    protected final Runner runner() {
        return this.runnerDelegate.runner();
    }

    protected final boolean isPaused() {
        return this.paused;
    }

    protected final WebSocketService getWebSocketService() {
        return this.wss;
    }

    protected final PlaybackService getPlaybackService() {
        return this.playback;
    }

    protected abstract WebSocketService.Client getWebSocketServiceClient();
    protected abstract PlaybackService.EventListener getPlaybackServiceEventListener();
}
