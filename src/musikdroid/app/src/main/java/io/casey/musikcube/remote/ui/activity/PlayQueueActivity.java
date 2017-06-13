package io.casey.musikcube.remote.ui.activity;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import com.pluscubed.recyclerfastscroll.RecyclerFastScroller;

import org.json.JSONObject;

import io.casey.musikcube.remote.R;
import io.casey.musikcube.remote.playback.Metadata;
import io.casey.musikcube.remote.playback.PlaybackService;
import io.casey.musikcube.remote.ui.model.TrackListSlidingWindow;
import io.casey.musikcube.remote.ui.util.Views;
import io.casey.musikcube.remote.websocket.Messages;
import io.casey.musikcube.remote.websocket.SocketMessage;
import io.casey.musikcube.remote.websocket.WebSocketService;

import static io.casey.musikcube.remote.ui.model.TrackListSlidingWindow.QueryFactory;

public class PlayQueueActivity extends WebSocketActivityBase {
    private static String EXTRA_PLAYING_INDEX = "extra_playing_index";

    public static Intent getStartIntent(final Context context, int playingIndex) {
        return new Intent(context, PlayQueueActivity.class)
            .putExtra(EXTRA_PLAYING_INDEX, playingIndex);
    }

    private WebSocketService wss;
    private TrackListSlidingWindow<JSONObject> tracks;
    private PlaybackService playback;
    private Adapter adapter;
    private boolean offlineQueue;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        this.wss = getWebSocketService();
        this.playback = getPlaybackService();

        setContentView(R.layout.recycler_view_activity);

        Views.setTitle(this, R.string.play_queue_title);
        Views.enableUpNavigation(this);

        this.adapter = new Adapter();

        final RecyclerFastScroller fastScroller = (RecyclerFastScroller) findViewById(R.id.fast_scroller);
        final RecyclerView recyclerView = (RecyclerView) findViewById(R.id.recycler_view);
        Views.setupDefaultRecyclerView(this, recyclerView, fastScroller, adapter);

        final QueryFactory queryFactory = this.playback.getPlaylistQueryFactory();

        offlineQueue = Messages.Category.OFFLINE.equals(
            queryFactory.getRequeryMessage().getStringOption(Messages.Key.CATEGORY));

        this.tracks = new TrackListSlidingWindow<>(
            recyclerView,
            fastScroller,
            this.wss,
            queryFactory,
            (JSONObject obj) -> obj);

        this.tracks.setInitialPosition(
            getIntent().getIntExtra(EXTRA_PLAYING_INDEX, -1));

        Views.addTransportFragment(this);
        Views.enableUpNavigation(this);
    }

    @Override
    protected void onPause() {
        super.onPause();
        this.tracks.pause();
    }

    @Override
    protected void onResume() {
        this.tracks.resume(); /* needs to happen before */
        super.onResume();
    }

    @Override
    protected WebSocketService.Client getWebSocketServiceClient() {
        return webSocketClient;
    }

    @Override
    protected PlaybackService.EventListener getPlaybackServiceEventListener() {
        return this.playbackEvents;
    }

    private final View.OnClickListener onItemClickListener = new View.OnClickListener() {
        @Override
        public void onClick(View v) {
            if (v.getTag() instanceof Integer) {
                final int index = (Integer) v.getTag();
                playback.playAt(index);
            }
        }
    };

    private final PlaybackService.EventListener playbackEvents = new PlaybackService.EventListener() {
        @Override
        public void onStateUpdated() {
            adapter.notifyDataSetChanged();
        }
    };

    private final WebSocketService.Client webSocketClient = new WebSocketService.Client() {
        @Override
        public void onStateChanged(WebSocketService.State newState, WebSocketService.State oldState) {
            if (newState == WebSocketService.State.Connected || offlineQueue) {
                tracks.requery();
            }
        }

        @Override
        public void onMessageReceived(SocketMessage broadcast) {
        }

        @Override
        public void onInvalidPassword() {
        }
    };

    private class ViewHolder extends RecyclerView.ViewHolder {
        private final TextView title;
        private final TextView subtitle;
        private final TextView trackNum;

        ViewHolder(View itemView) {
            super(itemView);
            title = (TextView) itemView.findViewById(R.id.title);
            subtitle = (TextView) itemView.findViewById(R.id.subtitle);
            trackNum = (TextView) itemView.findViewById(R.id.track_num);
        }

        void bind(JSONObject entry, int position) {
            trackNum.setText(String.valueOf(position + 1));
            itemView.setTag(position);
            int titleColor = R.color.theme_foreground;
            int subtitleColor = R.color.theme_disabled_foreground;

            if (entry == null) {
                title.setText("-");
                subtitle.setText("-");
            }
            else {
                final String entryExternalId = entry
                    .optString(Metadata.Track.EXTERNAL_ID, "");

                final String playingExternalId = playback
                    .getTrackString(Metadata.Track.EXTERNAL_ID, "");

                if (entryExternalId.equals(playingExternalId)) {
                    titleColor = R.color.theme_green;
                    subtitleColor = R.color.theme_yellow;
                }

                title.setText(entry.optString(Metadata.Track.TITLE, "-"));
                subtitle.setText(entry.optString(Metadata.Track.ALBUM_ARTIST, "-"));
            }

            title.setTextColor(getResources().getColor(titleColor));
            subtitle.setTextColor(getResources().getColor(subtitleColor));
        }
    }

    private class Adapter extends RecyclerView.Adapter<ViewHolder> {
        @Override
        public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
            final LayoutInflater inflater = LayoutInflater.from(parent.getContext());
            final View view = inflater.inflate(R.layout.play_queue_row, parent, false);
            view.setOnClickListener(onItemClickListener);
            return new ViewHolder(view);
        }

        @Override
        public void onBindViewHolder(ViewHolder holder, int position) {
            holder.bind(tracks.getTrack(position), position);
        }

        @Override
        public int getItemCount() {
            return (tracks == null) ? 0 : tracks.getCount();
        }
    }
}
