package io.casey.musikcube.remote.ui.fragment;

import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import io.casey.musikcube.remote.MainActivity;
import io.casey.musikcube.remote.R;
import io.casey.musikcube.remote.playback.Metadata;
import io.casey.musikcube.remote.playback.PlaybackService;
import io.casey.musikcube.remote.playback.PlaybackServiceFactory;
import io.casey.musikcube.remote.playback.PlaybackState;
import io.casey.musikcube.remote.ui.activity.PlayQueueActivity;

public class TransportFragment extends Fragment {
    public static final String TAG = "TransportFragment";

    public static TransportFragment newInstance() {
        return new TransportFragment();
    }

    private View rootView;
    private TextView title, playPause;
    private PlaybackService playback;
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
        this.playback = PlaybackServiceFactory.instance(getActivity());
    }

    @Override
    public void onPause() {
        super.onPause();
        this.playback.disconnect(this.playbackListener);
    }

    @Override
    public void onResume() {
        super.onResume();
        this.playback.connect(this.playbackListener);
    }

    public PlaybackService getPlaybackService() {
        return playback;
    }

    public void setModelChangedListener(OnModelChangedListener modelChangedListener) {
        this.modelChangedListener = modelChangedListener;
    }

    private void bindEventHandlers() {
        this.title = (TextView) this.rootView.findViewById(R.id.track_title);

        this.title.setOnClickListener((View view) -> {
            if (playback.getPlaybackState() != PlaybackState.Stopped) {
                final Intent intent = PlayQueueActivity
                    .getStartIntent(getActivity(), playback.getQueuePosition())
                    .addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP);

                startActivity(intent);
            }
        });

        this.title.setOnLongClickListener((View view) -> {
            startActivity(MainActivity.getStartIntent(getActivity()));
            return true;
        });

        this.rootView.findViewById(R.id.button_prev).setOnClickListener((View view) -> playback.prev());

        this.playPause = (TextView) this.rootView.findViewById(R.id.button_play_pause);

        this.playPause.setOnClickListener((View view) -> {
            if (playback.getPlaybackState() == PlaybackState.Stopped) {
                playback.playAll();
            }
            else {
                playback.pauseOrResume();
            }
        });

        this.rootView.findViewById(R.id.button_next).setOnClickListener((View view) -> playback.next());
    }

    private void rebindUi() {
        PlaybackState state = playback.getPlaybackState();

        final boolean playing = (state == PlaybackState.Playing);
        this.playPause.setText(playing ? R.string.button_pause : R.string.button_play);

        if (state == PlaybackState.Stopped) {
            title.setTextColor(getActivity().getResources().getColor(R.color.theme_disabled_foreground));
            title.setText(R.string.transport_not_playing);
        }
        else {
            title.setTextColor(getActivity().getResources().getColor(R.color.theme_green));
            title.setText(playback.getTrackString(Metadata.Track.TITLE, "(unknown title)"));
        }
    }

    private PlaybackService.EventListener playbackListener = new PlaybackService.EventListener() {
        @Override
        public void onStateUpdated() {
            rebindUi();

            if (modelChangedListener != null) {
                modelChangedListener.onChanged(TransportFragment.this);
            }
        }
    };
}
