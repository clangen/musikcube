package io.casey.musikcube.remote;

import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v7.app.AppCompatActivity;
import android.view.KeyEvent;

public abstract class WebSocketActivityBase extends AppCompatActivity {
    private WebSocketService wss;
    private boolean paused = true;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        this.wss = WebSocketService.getInstance(this);
    }

    @Override
    protected void onPause() {
        super.onPause();
        this.wss.removeClient(getWebSocketServiceClient());
        this.paused = true;
    }

    @Override
    protected void onResume() {
        super.onResume();
        this.wss.addClient(getWebSocketServiceClient());
        this.paused = false;
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

    protected final boolean isPaused() {
        return this.paused;
    }

    protected final WebSocketService getWebSocketService() {
        return this.wss;
    }

    protected abstract WebSocketService.Client getWebSocketServiceClient();
}
