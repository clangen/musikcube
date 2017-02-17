package io.casey.musikcube.remote;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.json.JSONObject;

public class PlayQueueActivity extends WebSocketActivityBase {
    private static String EXTRA_PLAYING_INDEX = "extra_playing_index";

    public static Intent getStartIntent(final Context context, int playingIndex) {
        return new Intent(context, PlayQueueActivity.class)
            .putExtra(EXTRA_PLAYING_INDEX, playingIndex);
    }

    private WebSocketService wss;
    private TrackListScrollCache<JSONObject> tracks;
    private TransportModel transportModel = new TransportModel();
    private Adapter adapter;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        this.wss = getWebSocketService();

        setContentView(R.layout.recycler_view_activity);
        setTitle(R.string.play_queue_title);

        this.adapter = new Adapter();
        final RecyclerView recyclerView = (RecyclerView) findViewById(R.id.recycler_view);

        Views.setupDefaultRecyclerView(this, recyclerView, adapter);

        this.tracks = new TrackListScrollCache<>(
            recyclerView, adapter, this.wss, this.queryFactory, (JSONObject obj) -> obj);

        this.tracks.setInitialPosition(
            getIntent().getIntExtra(EXTRA_PLAYING_INDEX, -1));

        Views.addTransportFragment(this);
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

    private void updatePlaybackModel(final SocketMessage message) {
        transportModel.update(message);
        adapter.notifyDataSetChanged();
    }

    private final TrackListScrollCache.QueryFactory queryFactory
        = new TrackListScrollCache.QueryFactory() {
            @Override
            public SocketMessage getRequeryMessage() {
                return SocketMessage.Builder
                    .request(Messages.Request.QueryPlayQueueTracks)
                    .addOption(Messages.Key.COUNT_ONLY, true)
                    .build();
            }

            @Override
            public SocketMessage getPageAroundMessage(int offset, int limit) {
                return SocketMessage.Builder
                    .request(Messages.Request.QueryPlayQueueTracks)
                    .addOption(Messages.Key.OFFSET, offset)
                    .addOption(Messages.Key.LIMIT, limit)
                    .build();
            }
        };

    private final View.OnClickListener onItemClickListener = new View.OnClickListener() {
        @Override
        public void onClick(View v) {
            if (v.getTag() instanceof Integer) {
                final int index = (Integer) v.getTag();

                wss.send(SocketMessage
                    .Builder.request(Messages.Request.PlayAtIndex)
                    .addOption(Messages.Key.INDEX, index)
                    .build());
            }
        }
    };

    private final WebSocketService.Client webSocketClient = new WebSocketService.Client() {
        @Override
        public void onStateChanged(WebSocketService.State newState, WebSocketService.State oldState) {
            if (newState == WebSocketService.State.Connected) {
                final SocketMessage overview = SocketMessage.Builder
                    .request(Messages.Request.GetPlaybackOverview).build();

                wss.send(overview, this, (SocketMessage response) -> updatePlaybackModel(response));

                tracks.requery();
            }
        }

        @Override
        public void onMessageReceived(SocketMessage broadcast) {
            if (Messages.Broadcast.PlaybackOverviewChanged.is(broadcast.getName())) {
                updatePlaybackModel(broadcast);
            }
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
                long playingId = transportModel.getTrackValueLong(Messages.Key.ID, -1);
                long entryId = entry.optLong(Messages.Key.ID, -1);

                if (entryId != -1 && playingId == entryId) {
                    titleColor = R.color.theme_green;
                    subtitleColor = R.color.theme_yellow;
                }

                title.setText(entry.optString(Messages.Key.TITLE, "-"));
                subtitle.setText(entry.optString(Messages.Key.ALBUM_ARTIST, "-"));
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
