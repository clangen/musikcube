package io.casey.musikcube.remote;

import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v7.app.AppCompatActivity;
import android.view.KeyEvent;
import android.view.MenuItem;

import com.uacf.taskrunner.LifecycleDelegate;
import com.uacf.taskrunner.Runner;
import com.uacf.taskrunner.Task;

public abstract class WebSocketActivityBase extends AppCompatActivity implements Runner.TaskCallbacks {
    private WebSocketService wss;
    private boolean paused = true;
    private LifecycleDelegate runnerDelegate;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        this.runnerDelegate = new LifecycleDelegate(this, this, getClass(), null);
        this.runnerDelegate.onCreate(savedInstanceState);
        this.wss = WebSocketService.getInstance(this);
    }

    @Override
    protected void onPause() {
        super.onPause();
        this.runnerDelegate.onPause();
        this.wss.removeClient(getWebSocketServiceClient());
        this.paused = true;
    }

    @Override
    protected void onResume() {
        super.onResume();
        this.runnerDelegate.onResume();
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
        if (keyCode == KeyEvent.KEYCODE_VOLUME_DOWN) {
            wss.send(SocketMessage.Builder
                .request(Messages.Request.SetVolume)
                .addOption(Messages.Key.RELATIVE, Messages.Value.DOWN)
                 .build());
            return true;
        }
        else if (keyCode == KeyEvent.KEYCODE_VOLUME_UP) {
            wss.send(SocketMessage.Builder
                .request(Messages.Request.SetVolume)
                .addOption(Messages.Key.RELATIVE, Messages.Value.UP)
                .build());

            return true;
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

    protected abstract WebSocketService.Client getWebSocketServiceClient();
}
