package io.casey.musikcube.remote;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.json.JSONObject;

import static io.casey.musikcube.remote.TrackListScrollCache.QueryFactory;

public class TrackListActivity extends WebSocketActivityBase implements Filterable {
    public static String EXTRA_TITLE = "extra_title";
    private static String EXTRA_CATEGORY_TYPE = "extra_category_type";
    private static String EXTRA_SELECTED_ID = "extra_selected_id";

    public static Intent getStartIntent(final Context context, final String type, final long id) {
        return new Intent(context, TrackListActivity.class)
            .putExtra(EXTRA_CATEGORY_TYPE, type)
            .putExtra(EXTRA_SELECTED_ID, id);
    }

    public static Intent getStartIntent(final Context context,
                                        final String type,
                                        final long id,
                                        final String categoryValue) {
        final Intent intent = getStartIntent(context, type, id);

        if (Strings.notEmpty(categoryValue)) {
            intent.putExtra(
                TrackListActivity.EXTRA_TITLE,
                context.getString(R.string.songs_from_category, categoryValue));
        }

        return intent;
    }

    public static Intent getStartIntent(final Context context) {
        return new Intent(context, TrackListActivity.class);
    }

    private TrackListScrollCache<JSONObject> tracks;
    private TransportFragment transport;
    private String categoryType;
    private long categoryId;
    private String lastFilter = "";

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        categoryType = getIntent().getStringExtra(EXTRA_CATEGORY_TYPE);
        categoryId = getIntent().getLongExtra(EXTRA_SELECTED_ID, 0);

        setContentView(R.layout.recycler_view_activity);
        setTitle(R.string.songs_title);

        final String title = getIntent().getStringExtra(EXTRA_TITLE);
        if (Strings.notEmpty(title)) {
            setTitle(title);
        }

        final QueryFactory queryFactory =
            createCategoryQueryFactory(categoryType, categoryId);

        final Adapter adapter = new Adapter();
        final RecyclerView recyclerView = (RecyclerView) findViewById(R.id.recycler_view);
        Views.setupDefaultRecyclerView(this, recyclerView, adapter);

        tracks = new TrackListScrollCache<>(
            recyclerView, adapter, getWebSocketService(), queryFactory, (JSONObject track) -> track);

        transport = Views.addTransportFragment(this,
            (TransportFragment fragment) -> adapter.notifyDataSetChanged());
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        if (!Messages.Category.PLAYLISTS.equals(categoryType)) {
            Views.initSearchMenu(this, menu, this);
        }
        return true;
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
        return socketServiceClient;
    }

    @Override
    public void setFilter(String filter) {
        this.lastFilter = filter;
        this.filterDebouncer.call();
    }

    private WebSocketService.Client socketServiceClient = new WebSocketService.Client() {
        @Override
        public void onStateChanged(WebSocketService.State newState, WebSocketService.State oldState) {
            if (newState == WebSocketService.State.Connected) {
                filterDebouncer.cancel();
                tracks.requery();
            }
        }

        @Override
        public void onMessageReceived(SocketMessage message) {
        }

        @Override
        public void onInvalidPassword() {
        }
    };

    private final Debouncer<String> filterDebouncer = new Debouncer<String>(350) {
        @Override
        protected void onDebounced(String caller) {
            if (!isPaused()) {
                tracks.requery();
            }
        }
    };

    private View.OnClickListener onItemClickListener = (View view) -> {
        int index = (Integer) view.getTag();
        SocketMessage request;

        if (isValidCategory(categoryType, categoryId)) {
            request = SocketMessage.Builder
                .request(Messages.Request.PlayTracksByCategory)
                .addOption(Messages.Key.CATEGORY, categoryType)
                .addOption(Messages.Key.ID, categoryId)
                .addOption(Messages.Key.INDEX, index)
                .addOption(Messages.Key.FILTER, lastFilter)
                .build();
        }
        else {
            request = SocketMessage.Builder
                .request(Messages.Request.PlayAllTracks)
                .addOption(Messages.Key.INDEX, index)
                .addOption(Messages.Key.FILTER, lastFilter)
                .build();
        }

        getWebSocketService().send(request, socketServiceClient, (SocketMessage response) -> {
            setResult(Navigation.ResponseCode.PLAYBACK_STARTED);
            finish();
        });
    };

    private class ViewHolder extends RecyclerView.ViewHolder {
        private final TextView title;
        private final TextView subtitle;

        ViewHolder(View itemView) {
            super(itemView);
            title = (TextView) itemView.findViewById(R.id.title);
            subtitle = (TextView) itemView.findViewById(R.id.subtitle);
        }

        void bind(JSONObject entry, int position) {
            itemView.setTag(position);

            /* TODO: this colorizing logic is copied from PlayQueueActivity. can we generalize
            it cleanly somehow? is it worth it? */
            int titleColor = R.color.theme_foreground;
            int subtitleColor = R.color.theme_disabled_foreground;

            if (entry != null) {
                long playingId = transport.getModel().getTrackValueLong(Messages.Key.ID, -1);
                long entryId = entry.optLong(Messages.Key.ID, -1);

                if (entryId != -1 && playingId == entryId) {
                    titleColor = R.color.theme_green;
                    subtitleColor = R.color.theme_yellow;
                }

                title.setText(entry.optString(Messages.Key.TITLE, "-"));
                subtitle.setText(entry.optString(Messages.Key.ALBUM_ARTIST, "-"));
            }
            else {
                title.setText("-");
                subtitle.setText("-");
            }

            title.setTextColor(getResources().getColor(titleColor));
            subtitle.setTextColor(getResources().getColor(subtitleColor));
        }
    }

    private class Adapter extends RecyclerView.Adapter<ViewHolder> {
        @Override
        public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
            final LayoutInflater inflater = LayoutInflater.from(parent.getContext());
            final View view = inflater.inflate(R.layout.simple_list_item, parent, false);
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

    private static boolean isValidCategory(final String categoryType, long categoryId) {
        return categoryType != null && categoryType.length() > 0 && categoryId != -1;
    }

    private QueryFactory createCategoryQueryFactory(
        final String categoryType, long categoryId) {

        if (isValidCategory(categoryType, categoryId)) {
            /* tracks for a specified category (album, artists, genres, etc */
            return new QueryFactory() {
                @Override
                public SocketMessage getRequeryMessage() {
                    return SocketMessage.Builder
                        .request(Messages.Request.QueryTracksByCategory)
                        .addOption(Messages.Key.CATEGORY, categoryType)
                        .addOption(Messages.Key.ID, categoryId)
                        .addOption(Messages.Key.COUNT_ONLY, true)
                        .addOption(Messages.Key.FILTER, lastFilter)
                        .build();
                }

                @Override
                public SocketMessage getPageAroundMessage(int offset, int limit) {
                    return SocketMessage.Builder
                        .request(Messages.Request.QueryTracksByCategory)
                        .addOption(Messages.Key.CATEGORY, categoryType)
                        .addOption(Messages.Key.ID, categoryId)
                        .addOption(Messages.Key.OFFSET, offset)
                        .addOption(Messages.Key.LIMIT, limit)
                        .addOption(Messages.Key.FILTER, lastFilter)
                        .build();
                }
            };
        }
        else {
            /* all tracks */
            return new QueryFactory() {
                @Override
                public SocketMessage getRequeryMessage() {
                    return SocketMessage.Builder
                        .request(Messages.Request.QueryTracks)
                        .addOption(Messages.Key.FILTER, lastFilter)
                        .addOption(Messages.Key.COUNT_ONLY, true)
                        .build();
                }

                @Override
                public SocketMessage getPageAroundMessage(int offset, int limit) {
                    return SocketMessage.Builder
                        .request(Messages.Request.QueryTracks)
                        .addOption(Messages.Key.OFFSET, offset)
                        .addOption(Messages.Key.LIMIT, limit)
                        .addOption(Messages.Key.FILTER, lastFilter)
                        .build();
                }
            };
        }
    }
}
