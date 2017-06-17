package io.casey.musikcube.remote.ui.activity;

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

import com.pluscubed.recyclerfastscroll.RecyclerFastScroller;

import org.json.JSONObject;

import io.casey.musikcube.remote.R;
import io.casey.musikcube.remote.playback.Metadata;
import io.casey.musikcube.remote.playback.PlaybackService;
import io.casey.musikcube.remote.ui.fragment.TransportFragment;
import io.casey.musikcube.remote.ui.model.TrackListSlidingWindow;
import io.casey.musikcube.remote.ui.util.Views;
import io.casey.musikcube.remote.ui.view.EmptyListView;
import io.casey.musikcube.remote.util.Debouncer;
import io.casey.musikcube.remote.util.Navigation;
import io.casey.musikcube.remote.util.Strings;
import io.casey.musikcube.remote.websocket.Messages;
import io.casey.musikcube.remote.websocket.SocketMessage;
import io.casey.musikcube.remote.websocket.WebSocketService;

import static io.casey.musikcube.remote.ui.view.EmptyListView.Capability;
import static io.casey.musikcube.remote.ui.model.TrackListSlidingWindow.QueryFactory;

public class TrackListActivity extends WebSocketActivityBase implements Filterable {
    private static String EXTRA_CATEGORY_TYPE = "extra_category_type";
    private static String EXTRA_SELECTED_ID = "extra_selected_id";
    private static String EXTRA_TITLE_ID = "extra_title_id";

    public static Intent getStartIntent(final Context context, final String type, final long id) {
        return new Intent(context, TrackListActivity.class)
            .putExtra(EXTRA_CATEGORY_TYPE, type)
            .putExtra(EXTRA_SELECTED_ID, id);
    }

    public static Intent getOfflineStartIntent(final Context context) {
        return getStartIntent(context, Messages.Category.OFFLINE, 0)
            .putExtra(EXTRA_TITLE_ID, R.string.offline_tracks_title);
    }

    public static Intent getStartIntent(final Context context,
                                        final String type,
                                        final long id,
                                        final String categoryValue) {
        final Intent intent = getStartIntent(context, type, id);

        if (Strings.notEmpty(categoryValue)) {
            intent.putExtra(
                Views.EXTRA_TITLE,
                context.getString(R.string.songs_from_category, categoryValue));
        }

        return intent;
    }

    public static Intent getStartIntent(final Context context) {
        return new Intent(context, TrackListActivity.class);
    }

    private TrackListSlidingWindow<JSONObject> tracks;
    private EmptyListView emptyView;
    private TransportFragment transport;
    private String categoryType;
    private long categoryId;
    private String lastFilter = "";
    private Adapter adapter;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        final Intent intent = getIntent();
        categoryType = intent.getStringExtra(EXTRA_CATEGORY_TYPE);
        categoryId = intent.getLongExtra(EXTRA_SELECTED_ID, 0);
        final int titleId = intent.getIntExtra(EXTRA_TITLE_ID, R.string.songs_title);

        setContentView(R.layout.recycler_view_activity);

        Views.setTitle(this, titleId);
        Views.enableUpNavigation(this);

        final QueryFactory queryFactory =
            createCategoryQueryFactory(categoryType, categoryId);

        adapter = new Adapter();
        final RecyclerFastScroller fastScroller = (RecyclerFastScroller) findViewById(R.id.fast_scroller);
        final RecyclerView recyclerView = (RecyclerView) findViewById(R.id.recycler_view);
        Views.setupDefaultRecyclerView(this, recyclerView, fastScroller, adapter);

        emptyView = (EmptyListView) findViewById(R.id.empty_list_view);
        emptyView.setCapability(isOfflineTracks() ? Capability.OfflineOk : Capability.OnlineOnly);
        emptyView.setEmptyMessage(getEmptyMessage());
        emptyView.setAlternateView(recyclerView);

        tracks = new TrackListSlidingWindow<>(
            recyclerView,
            fastScroller,
            getWebSocketService(),
            queryFactory,
            (JSONObject track) -> track);

        tracks.setOnMetadataLoadedListener(slidingWindowListener);

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
        requeryIfViewingOfflineCache();
    }

    @Override
    protected WebSocketService.Client getWebSocketServiceClient() {
        return socketServiceClient;
    }

    @Override
    protected PlaybackService.EventListener getPlaybackServiceEventListener() {
        return null;
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
            else {
                emptyView.update(newState, adapter.getItemCount());
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

        if (isValidCategory(categoryType, categoryId)) {
            getPlaybackService().play(categoryType, categoryId, index, lastFilter);
        }
        else {
            getPlaybackService().playAll(index, lastFilter);
        }

        setResult(Navigation.ResponseCode.PLAYBACK_STARTED);
        finish();
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
                final String entryExternalId = entry
                    .optString(Metadata.Track.EXTERNAL_ID, "");

                final String playingExternalId = transport.getPlaybackService()
                    .getTrackString(Metadata.Track.EXTERNAL_ID, "");

                if (entryExternalId.equals(playingExternalId)) {
                    titleColor = R.color.theme_green;
                    subtitleColor = R.color.theme_yellow;
                }

                title.setText(entry.optString(Metadata.Track.TITLE, "-"));
                subtitle.setText(entry.optString(Metadata.Track.ALBUM_ARTIST, "-"));
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

    private String getEmptyMessage() {
        if (isOfflineTracks()) {
            return getString(R.string.empty_no_offline_tracks_message);
        }

        return getString(R.string.empty_no_items_format, getString(R.string.browse_type_tracks));
    }

    private boolean isOfflineTracks() {
        return Messages.Category.OFFLINE.equals(categoryType);
    }

    private void requeryIfViewingOfflineCache() {
        if (isOfflineTracks()) {
            tracks.requery();
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

                @Override
                public boolean connectionRequired() {
                    return Messages.Category.OFFLINE.equals(categoryType);
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

    private TrackListSlidingWindow.OnMetadataLoadedListener slidingWindowListener
        = new TrackListSlidingWindow.OnMetadataLoadedListener() {
            public void onReloaded(int count) {
                emptyView.update(getWebSocketService().getState(), count);
            }

            public void onMetadataLoaded(int offset, int count) {
            }
        };
}
