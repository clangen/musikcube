package io.casey.musikcube.remote;

import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v7.app.AppCompatActivity;
import android.view.KeyEvent;

public abstract class WebSocketActivityBase extends AppCompatActivity {
    private WebSocketService wss;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        this.wss = WebSocketService.getInstance(this);
    }

    @Override
    protected void onPause() {
        super.onPause();
        this.wss.removeClient(getWebSocketServiceClient());
    }

    @Override
    protected void onResume() {
        super.onResume();
        this.wss.addClient(getWebSocketServiceClient());
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_VOLUME_DOWN) {
            wss.send(SocketMessage.Builder
                .request(Messages.Request.SetVolume)
                .addOption(TransportModel.Key.VOLUME, -0.05)
                .addOption(Messages.Key.RELATIVE, true)
                .build());
            return true;
        }
        else if (keyCode == KeyEvent.KEYCODE_VOLUME_UP) {
            wss.send(SocketMessage.Builder
                .request(Messages.Request.SetVolume)
                .addOption(TransportModel.Key.VOLUME, 0.05)
                .addOption(Messages.Key.RELATIVE, true)
                .build());

            return true;
        }

        return super.onKeyDown(keyCode, event);
    }

    protected final WebSocketService getWebSocketService() {
        return this.wss;
    }

    protected abstract WebSocketService.Client getWebSocketServiceClient();
}
