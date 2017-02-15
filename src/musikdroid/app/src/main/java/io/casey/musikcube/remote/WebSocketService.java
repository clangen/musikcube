package io.casey.musikcube.remote;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;

import com.neovisionaries.ws.client.WebSocket;
import com.neovisionaries.ws.client.WebSocketAdapter;
import com.neovisionaries.ws.client.WebSocketFactory;
import com.neovisionaries.ws.client.WebSocketFrame;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.atomic.AtomicLong;

import static android.content.Context.CONNECTIVITY_SERVICE;

public class WebSocketService {
    private static final int AUTO_RECONNECT_INTERVAL_MILLIS = 2000;
    private static final int CALLBACK_TIMEOUT_MILLIS = 30000;
    private static final int CONNECTION_TIMEOUT_MILLIS = 5000;
    private static final int PING_INTERVAL_MILLIS = 3500;
    private static final int AUTO_CONNECT_FAILSAFE_DELAY_MILLIS = 2000;
    private static final int AUTO_DISCONNECT_DELAY_MILLIS = 5000;

    private static final int MESSAGE_BASE = 0xcafedead;
    private static final int MESSAGE_CONNECT_THREAD_FINISHED = MESSAGE_BASE + 0;
    private static final int MESSAGE_MESSAGE_RECEIVED = MESSAGE_BASE + 1;
    private static final int MESSAGE_REMOVE_OLD_CALLBACKS = MESSAGE_BASE + 2;
    private static final int MESSAGE_AUTO_RECONNECT = MESSAGE_BASE + 3;
    private static final int MESSAGE_SCHEDULE_PING = MESSAGE_BASE + 4;
    private static final int MESSAGE_PING_EXPIRED = MESSAGE_BASE + 5;

    public interface Client {
        void onStateChanged(State newState, State oldState);
        void onMessageReceived(SocketMessage message);
    }

    public interface MessageResultCallback {
        void onMessageResult(final SocketMessage response);
    }

    private interface Predicate1<T> {
        boolean check(T value);
    }

    public enum State {
        Connecting,
        Connected,
        Disconnected
    }

    private Handler handler = new Handler(Looper.getMainLooper(), new Handler.Callback() {
        @Override
        public boolean handleMessage(Message message) {
            if (message.what == MESSAGE_CONNECT_THREAD_FINISHED) {
                if (message.obj == null) {
                    disconnect(true); /* auto-reconnect */
                }
                else {
                    setSocket((WebSocket) message.obj);
                    setState(State.Connected);
                    ping();
                }
                return true;
            }
            else if (message.what == MESSAGE_MESSAGE_RECEIVED) {
                if (clients != null) {
                    final SocketMessage msg = (SocketMessage) message.obj;

                    boolean dispatched = false;

                    /* registered callback for THIS message */
                    final MessageResultDescriptor mdr = messageCallbacks.remove(msg.getId());
                    if (mdr != null && mdr.callback != null) {
                        mdr.callback.onMessageResult(msg);
                        dispatched = true;
                    }

                    if (!dispatched) {
                        /* service-level callback */
                        for (Client client : clients) {
                            client.onMessageReceived(msg);
                        }
                    }
                }
                return true;
            }
            else if (message.what == MESSAGE_REMOVE_OLD_CALLBACKS) {
                removeExpiredCallbacks();
                handler.sendEmptyMessageDelayed(MESSAGE_REMOVE_OLD_CALLBACKS, CALLBACK_TIMEOUT_MILLIS);
            }
            else if (message.what == MESSAGE_AUTO_RECONNECT) {
                if (getState() == State.Disconnected && autoReconnect) {
                    reconnect();
                }
            }
            else if (message.what == MESSAGE_SCHEDULE_PING) {
                ping();
            }
            else if (message.what == MESSAGE_PING_EXPIRED) {
                // Toast.makeText(context, "recovering...", Toast.LENGTH_LONG).show();
                removeInternalCallbacks();
                boolean reconnect = (getState() == State.Connected) || autoReconnect;
                disconnect(reconnect);
            }

            return false;
        }
    });

    private static class MessageResultDescriptor {
        long id;
        long enqueueTime;
        Client client;
        MessageResultCallback callback;
    }

    private static WebSocketService INSTANCE;
    private static AtomicLong NEXT_ID = new AtomicLong(0);

    private Context context;
    private SharedPreferences prefs;
    private WebSocket socket = null;
    private State state = State.Disconnected;
    private Set<Client> clients = new HashSet<>();
    private Map<String, MessageResultDescriptor> messageCallbacks = new HashMap<>();
    private boolean autoReconnect = false;
    private NetworkChangedReceiver networkChanged = new NetworkChangedReceiver();
    private ConnectThread thread;

    public static synchronized WebSocketService getInstance(final Context context) {
        if (INSTANCE == null) {
            INSTANCE = new WebSocketService(context);
        }

        return INSTANCE;
    }

    private WebSocketService(final Context context) {
        this.context = context.getApplicationContext();
        this.prefs = this.context.getSharedPreferences("prefs", Context.MODE_PRIVATE);
        handler.sendEmptyMessageDelayed(MESSAGE_REMOVE_OLD_CALLBACKS, CALLBACK_TIMEOUT_MILLIS);
    }

    public void addClient(Client client) {
        Preconditions.throwIfNotOnMainThread();

        if (!this.clients.contains(client)) {
            this.clients.add(client);

            if (this.clients.size() == 1) {
                registerReceiverAndScheduleFailsafe();
                handler.removeCallbacks(autoDisconnectRunnable);
            }

            client.onStateChanged(getState(), getState());
        }
    }

    public void removeClient(Client client) {
        Preconditions.throwIfNotOnMainThread();

        if (this.clients.remove(client)) {
            removeCallbacksForClient(client);

            if (this.clients.size() == 0) {
                unregisterReceiverAndCancelFailsafe();
                handler.postDelayed(autoDisconnectRunnable, AUTO_DISCONNECT_DELAY_MILLIS);
            }
        }
    }

    public void reconnect() {
        Preconditions.throwIfNotOnMainThread();
        autoReconnect = true;
        connectIfNotConnected();
    }

    public void disconnect() {
        disconnect(false); /* don't auto-reconnect */
    }

    public State getState() {
        return state;
    }

    public void cancelMessage(final long id) {
        Preconditions.throwIfNotOnMainThread();
        removeCallbacks((MessageResultDescriptor mrd) -> mrd.id == id);
    }

    private void ping() {
        if (state == State.Connected) {
            //Log.i("WebSocketService", "ping");
            removeInternalCallbacks();
            handler.removeMessages(MESSAGE_PING_EXPIRED);
            handler.sendEmptyMessageDelayed(MESSAGE_PING_EXPIRED, PING_INTERVAL_MILLIS);

            final SocketMessage ping = SocketMessage.Builder
                .request(Messages.Request.Ping).build();

            send(ping, INTERNAL_CLIENT, (SocketMessage response) -> {
                //Log.i("WebSocketService", "pong");
                handler.removeMessages(MESSAGE_PING_EXPIRED);
                handler.sendEmptyMessageDelayed(MESSAGE_SCHEDULE_PING, PING_INTERVAL_MILLIS);
            });
        }
    }

    public void cancelMessages(final Client client) {
        Preconditions.throwIfNotOnMainThread();
        removeCallbacks((MessageResultDescriptor mrd) -> mrd.client == client);
    }

    public long send(final SocketMessage message) {
        return send(message, null, null);
    }

    public long send(final SocketMessage message, Client client, MessageResultCallback callback) {
        Preconditions.throwIfNotOnMainThread();

        if (this.socket != null) {
            /* it seems that sometimes the socket dies, but the onDisconnected() event is not
            raised. unclear if this is our bug or a bug in the library. disconnect and trigger
            a reconnect until we can find a better root cause. this is very difficult to repro */
            if (!this.socket.isOpen()) {
                this.disconnect(true);
            }
            else {
                long id = NEXT_ID.incrementAndGet();

                if (callback != null) {
                    if (!clients.contains(client) && client != INTERNAL_CLIENT) {
                        throw new IllegalArgumentException("client is not registered");
                    }

                    final MessageResultDescriptor mrd = new MessageResultDescriptor();
                    mrd.id = id;
                    mrd.enqueueTime = System.currentTimeMillis();
                    mrd.client = client;
                    mrd.callback = callback;
                    messageCallbacks.put(message.getId(), mrd);
                }

                this.socket.sendText(message.toString());
                return id;
            }
        }

        return -1;
    }

    public boolean hasValidConnection() {
        final String addr = prefs.getString("address", "");
        final int port = prefs.getInt("port", -1);
        return (addr.length() > 0 && port >= 0);
    }

    private void disconnect(boolean autoReconnect) {
        Preconditions.throwIfNotOnMainThread();

        synchronized (this) {
            if (this.thread != null) {
                this.thread.interrupt();
                this.thread = null;
            }
        }

        this.autoReconnect = autoReconnect;

        if (this.socket != null) {
            this.socket.removeListener(webSocketAdapter);
            this.socket.disconnect();
            this.socket = null;
        }

        this.messageCallbacks.clear();
        setState(State.Disconnected);

        if (autoReconnect) {
            this.handler.sendEmptyMessageDelayed(
                MESSAGE_AUTO_RECONNECT,
                AUTO_RECONNECT_INTERVAL_MILLIS);
        }
        else {
            this.handler.removeMessages(MESSAGE_AUTO_RECONNECT);
        }
    }

    private void removeInternalCallbacks() {
        removeCallbacks((MessageResultDescriptor mrd) -> mrd.client == INTERNAL_CLIENT);
    }

    private void removeExpiredCallbacks() {
        final long now = System.currentTimeMillis();

        removeCallbacks((MessageResultDescriptor value) ->
            now - value.enqueueTime > CALLBACK_TIMEOUT_MILLIS);
    }

    private void removeCallbacksForClient(final Client client) {
        removeCallbacks((MessageResultDescriptor value) -> value == client);
    }

    private void removeCallbacks(Predicate1<MessageResultDescriptor> predicate) {
        final Iterator<Map.Entry<String, MessageResultDescriptor>> it
                = messageCallbacks.entrySet().iterator();

        while (it.hasNext()) {
            final Map.Entry<String, MessageResultDescriptor> entry = it.next();
            if (predicate.check(entry.getValue())) {
                it.remove();
            }
        }
    }

    private void connectIfNotConnected() {
        if (state != State.Connected || !socket.isOpen()) {
            disconnect(autoReconnect);
            handler.removeMessages(MESSAGE_AUTO_RECONNECT);
            setState(State.Connecting);
            thread = new ConnectThread();
            thread.start();
        }
    }

    private void setSocket(WebSocket socket) {
        if (this.socket != socket) {
            if (this.socket != null) {
                this.socket.removeListener(webSocketAdapter);
            }

            this.socket = socket;
            this.socket.addListener(webSocketAdapter);
        }
    }

    private void setState(State state) {
        Preconditions.throwIfNotOnMainThread();

        if (this.state != state) {
            State old = this.state;
            this.state = state;

            for (Client client : this.clients) {
                client.onStateChanged(state, old);
            }
        }
    }

    private void registerReceiverAndScheduleFailsafe() {
        unregisterReceiverAndCancelFailsafe();

        /* generally raises a CONNECTIVITY_ACTION event immediately,
        even if already connected. */
        final IntentFilter filter = new IntentFilter();
        filter.addAction(ConnectivityManager.CONNECTIVITY_ACTION);
        context.registerReceiver(networkChanged, filter);

        /* however, CONNECTIVITY_ACTION doesn't ALWAYS seem to be raised,
        so we schedule a failsafe just in case */
        this.handler.postDelayed(autoReconnectFailsafeRunnable, AUTO_CONNECT_FAILSAFE_DELAY_MILLIS);
    }

    private void unregisterReceiverAndCancelFailsafe() {
        handler.removeCallbacks(autoReconnectFailsafeRunnable);

        try {
            context.unregisterReceiver(networkChanged);
        }
        catch (Exception ex) {
            /* om nom nom */
        }
    }

    private Runnable autoReconnectFailsafeRunnable = () -> {
        if (getState() != WebSocketService.State.Connected) {
            reconnect();
        }
    };

    private Runnable autoDisconnectRunnable = () -> disconnect();

    private WebSocketAdapter webSocketAdapter = new WebSocketAdapter() {
        @Override
        public void onTextFrame(WebSocket websocket, WebSocketFrame frame) throws Exception {
            final SocketMessage message = SocketMessage.create(frame.getPayloadText());
            if (message != null) {
                handler.sendMessage(Message.obtain(handler, MESSAGE_MESSAGE_RECEIVED, message));
            }
        }

        @Override
        public void onDisconnected(WebSocket websocket,
                                   WebSocketFrame serverCloseFrame,
                                   WebSocketFrame clientCloseFrame,
                                   boolean closedByServer) throws Exception {
            handler.sendMessage(Message.obtain(handler, MESSAGE_CONNECT_THREAD_FINISHED, null));
        }
    };

    private class ConnectThread extends Thread {
        @Override
        public void run() {
            WebSocket socket;

            try {
                final WebSocketFactory factory = new WebSocketFactory();

                final String host = String.format(
                    Locale.ENGLISH,
                    "ws://%s:%d",
                    prefs.getString("address", "192.168.1.100"),
                    prefs.getInt("port", 9002));

                socket = factory.createSocket(host, CONNECTION_TIMEOUT_MILLIS);
                socket.connect();
                socket.setPingInterval(PING_INTERVAL_MILLIS);
            }
            catch (Exception ex) {
                socket = null;
            }

            synchronized (WebSocketService.this) {
                if (!isInterrupted()) {
                    handler.sendMessage(Message.obtain(
                        handler, MESSAGE_CONNECT_THREAD_FINISHED, socket));
                }

                if (thread == this) {
                    thread = null;
                }
            }
        }
    }

    private class NetworkChangedReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            final ConnectivityManager cm = (ConnectivityManager)
                    context.getSystemService(CONNECTIVITY_SERVICE);

            final NetworkInfo info = cm.getActiveNetworkInfo();

            if (info != null && info.isConnected()) {
                if (getState() == WebSocketService.State.Disconnected) {
                    disconnect();
                    reconnect();
                }
            }
        }
    }

    private static Client INTERNAL_CLIENT = new Client() {
        public void onStateChanged(State newState, State oldState) { }
        public void onMessageReceived(SocketMessage message) { }
    };
}
