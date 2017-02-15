package io.casey.musikcube.remote;

import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

public class TransportFragment extends Fragment {
    public static final String TAG = "TransportFragment";

    public static TransportFragment newInstance() {
        return new TransportFragment();
    }

    private WebSocketService wss;
    private View rootView;
    private TextView title, playPause;
    private TransportModel transportModel = new TransportModel();
    private OnModelChangedListener modelChangedListener;

    public interface OnModelChangedListener {
        void onChanged(TransportFragment fragment);
    }

    @Nullable
    @Override
    public View onCreateView(LayoutInflater inflater,
                             @Nullable ViewGroup container,
                             @Nullable Bundle savedInstanceState)
    {
        this.rootView = inflater.inflate(R.layout.fragment_transport, container, false);
        bindEventHandlers();
        rebindUi();
        return this.rootView;
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        this.wss = WebSocketService.getInstance(getActivity());
    }

    @Override
    public void onPause() {
        super.onPause();
        this.wss.removeClient(socketClient);

    }

    @Override
    public void onResume() {
        super.onResume();
        this.wss.addClient(socketClient);
    }

    public TransportModel getModel() {
        return transportModel;
    }

    public void setModelChangedListener(OnModelChangedListener modelChangedListener) {
        this.modelChangedListener = modelChangedListener;
    }

    private void bindEventHandlers() {
        this.title = (TextView) this.rootView.findViewById(R.id.track_title);

        this.title.setOnClickListener((View view) -> {
            if (transportModel.getPlaybackState() != TransportModel.PlaybackState.Stopped) {
                final Intent intent = PlayQueueActivity
                    .getStartIntent(getActivity(), transportModel.getQueuePosition())
                    .addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP);

                startActivity(intent);
            }
        });

        this.title.setOnLongClickListener((View view) -> {
            startActivity(MainActivity.getStartIntent(getActivity()));
            return true;
        });

        this.rootView.findViewById(R.id.button_prev).setOnClickListener((View view) -> {
            wss.send(SocketMessage.Builder.request(
                Messages.Request.Previous).build());
        });

        this.playPause = (TextView) this.rootView.findViewById(R.id.button_play_pause);

        this.playPause.setOnClickListener((View view) -> {
            if (transportModel.getPlaybackState() == TransportModel.PlaybackState.Stopped) {
                wss.send(SocketMessage.Builder.request(
                    Messages.Request.PlayAllTracks).build());
            }
            else {
                wss.send(SocketMessage.Builder.request(
                    Messages.Request.PauseOrResume).build());
            }
        });

        this.rootView.findViewById(R.id.button_next).setOnClickListener((View view) -> {
            wss.send(SocketMessage.Builder.request(
                Messages.Request.Next).build());
        });
    }

    private void rebindUi() {
        TransportModel.PlaybackState state = transportModel.getPlaybackState();

        final boolean playing = (state == TransportModel.PlaybackState.Playing);
        this.playPause.setText(playing ? R.string.button_pause : R.string.button_play);

        if (state == TransportModel.PlaybackState.Stopped) {
            title.setTextColor(getActivity().getResources().getColor(R.color.theme_disabled_foreground));
            title.setText(R.string.transport_not_playing);
        }
        else {
            title.setTextColor(getActivity().getResources().getColor(R.color.theme_green));
            title.setText(transportModel.getTrackValueString(TransportModel.Key.TITLE, "(unknown title)"));
        }
    }

    private WebSocketService.Client socketClient = new WebSocketService.Client() {
        @Override
        public void onStateChanged(WebSocketService.State newState, WebSocketService.State oldState) {
            if (newState == WebSocketService.State.Connected) {
                wss.send(SocketMessage.Builder.request(
                    Messages.Request.GetPlaybackOverview.toString()).build());
            }
        }

        @Override
        public void onMessageReceived(SocketMessage message) {
            if (transportModel.canHandle(message)) {
                if (transportModel.update(message)) {
                    rebindUi();

                    if (modelChangedListener != null) {
                        modelChangedListener.onChanged(TransportFragment.this);
                    }
                }
            }
        }
    };
}
