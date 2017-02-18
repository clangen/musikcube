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

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.HashMap;
import java.util.Map;

public class CategoryBrowseActivity extends WebSocketActivityBase implements Filterable {
    private static final String EXTRA_CATEGORY = "extra_category";
    private static final String EXTRA_DEEP_LINK_TYPE = "extra_deep_link_type";

    public interface DeepLink {
        int TRACKS = 0;
        int ALBUMS = 1;
    }

    private static final Map<String, String> CATEGORY_NAME_TO_ID = new HashMap<>();
    private static final Map<String, Integer> CATEGORY_NAME_TO_TITLE = new HashMap<>();

    static {
        CATEGORY_NAME_TO_ID.put(Messages.Category.ALBUM_ARTIST, Messages.Key.ALBUM_ARTIST_ID);
        CATEGORY_NAME_TO_ID.put(Messages.Category.GENRE, Messages.Key.GENRE_ID);
        CATEGORY_NAME_TO_ID.put(Messages.Category.ARTIST, Messages.Key.ARTIST_ID);
        CATEGORY_NAME_TO_ID.put(Messages.Category.ALBUM, Messages.Key.ALBUM_ID);
        CATEGORY_NAME_TO_ID.put(Messages.Category.PLAYLISTS, Messages.Key.ALBUM_ID);

        CATEGORY_NAME_TO_TITLE.put(Messages.Category.ALBUM_ARTIST, R.string.artists_title);
        CATEGORY_NAME_TO_TITLE.put(Messages.Category.GENRE, R.string.genres_title);
        CATEGORY_NAME_TO_TITLE.put(Messages.Category.ARTIST, R.string.artists_title);
        CATEGORY_NAME_TO_TITLE.put(Messages.Category.ALBUM, R.string.albums_title);
        CATEGORY_NAME_TO_TITLE.put(Messages.Category.PLAYLISTS, R.string.playlists_title);
    }

    public static Intent getStartIntent(final Context context, final String category) {
        return new Intent(context, CategoryBrowseActivity.class)
            .putExtra(EXTRA_CATEGORY, category);
    }

    public static Intent getStartIntent(final Context context, final String category, int deepLinkType) {
        return new Intent(context, CategoryBrowseActivity.class)
            .putExtra(EXTRA_CATEGORY, category)
            .putExtra(EXTRA_DEEP_LINK_TYPE, deepLinkType);
    }

    private String category;
    private Adapter adapter;
    private String lastFilter;
    private TransportFragment transport;
    private int deepLinkType;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        this.category = getIntent().getStringExtra(EXTRA_CATEGORY);
        this.deepLinkType = getIntent().getIntExtra(EXTRA_DEEP_LINK_TYPE, DeepLink.ALBUMS);
        this.adapter = new Adapter();

        setContentView(R.layout.recycler_view_activity);

        if (CATEGORY_NAME_TO_TITLE.containsKey(category)) {
            setTitle(CATEGORY_NAME_TO_TITLE.get(category));
        }

        final RecyclerView recyclerView = (RecyclerView) findViewById(R.id.recycler_view);

        Views.setupDefaultRecyclerView(this, recyclerView, this.adapter);

        transport = Views.addTransportFragment(this,
            (TransportFragment fragment) -> adapter.notifyDataSetChanged());
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        if (!Messages.Category.PLAYLISTS.equals(category)) { /* bleh */
            Views.initSearchMenu(this, menu, this);
        }
        return true;
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (resultCode == Navigation.ResponseCode.PLAYBACK_STARTED) {
            setResult(Navigation.ResponseCode.PLAYBACK_STARTED);
            finish();
        }

        super.onActivityResult(requestCode, resultCode, data);
    }

    @Override
    public void setFilter(String filter) {
        this.lastFilter = filter;
        this.filterDebouncer.call();
    }

    @Override
    protected WebSocketService.Client getWebSocketServiceClient() {
        return socketClient;
    }

    private void requery() {
        final SocketMessage request = SocketMessage.Builder
            .request(Messages.Request.QueryCategory)
            .addOption(Messages.Key.CATEGORY, category)
            .addOption(Messages.Key.FILTER, lastFilter)
            .build();

        getWebSocketService().send(request, this.socketClient, (SocketMessage response) -> {
            JSONArray data = response.getJsonArrayOption(Messages.Key.DATA);
            if (data != null && data.length() > 0) {
                adapter.setModel(data);
            }
        });
    }

    private final Debouncer<String> filterDebouncer = new Debouncer<String>(350) {
        @Override
        protected void onDebounced(String caller) {
            if (!isPaused()) {
                requery();
            }
        }
    };

    private WebSocketService.Client socketClient = new WebSocketService.Client() {
        @Override
        public void onStateChanged(WebSocketService.State newState, WebSocketService.State oldState) {
            if (newState == WebSocketService.State.Connected) {
                filterDebouncer.cancel();
                requery();
            }
        }

        @Override
        public void onMessageReceived(SocketMessage message) {
        }

        @Override
        public void onInvalidPassword() {
        }
    };

    private View.OnClickListener onItemClickListener = (View view) -> {
        final JSONObject entry = (JSONObject) view.getTag();
        if (deepLinkType == DeepLink.ALBUMS) {
            navigateToAlbums(entry);
        }
        else {
            navigateToTracks(entry);
        }
    };

    private View.OnLongClickListener onItemLongClickListener = (View view) -> {
        /* if we deep link to albums by default, long press will get to
        tracks. if we deep link to tracks, just ignore */
        if (deepLinkType == DeepLink.ALBUMS) {
            navigateToTracks((JSONObject) view.getTag());
            return true;
        }
        else {
            return false;
        }
    };

    private void navigateToAlbums(final JSONObject entry) {
        final Intent intent = AlbumBrowseActivity.getStartIntent(this, category, entry);
        startActivityForResult(intent, Navigation.RequestCode.ALBUM_BROWSE_ACTIVITY);
    }

    private void navigateToTracks(final JSONObject entry) {
        final long categoryId = entry.optLong(Messages.Key.ID);
        final String value = entry.optString(Messages.Key.VALUE);
        final Intent intent = TrackListActivity.getStartIntent(this, category, categoryId, value);
        startActivityForResult(intent, Navigation.RequestCode.CATEGORY_TRACKS_ACTIVITY);
    }

    private class ViewHolder extends RecyclerView.ViewHolder {
        private final TextView title;

        ViewHolder(View itemView) {
            super(itemView);
            title = (TextView) itemView.findViewById(R.id.title);
            itemView.findViewById(R.id.subtitle).setVisibility(View.GONE);
        }

        void bind(JSONObject entry) {
            final long entryId = entry.optLong(Messages.Key.ID);
            long playingId = -1;

            final String idKey = CATEGORY_NAME_TO_ID.get(category);
            if (idKey != null && idKey.length() > 0) {
                playingId = transport.getModel().getTrackValueLong(idKey, -1);
            }

            int titleColor = R.color.theme_foreground;
            if (playingId != -1 && entryId == playingId) {
                titleColor = R.color.theme_green;
            }

            /* note optString only does a null check! */
            String value = entry.optString(Messages.Key.VALUE, "");
            value = Strings.empty(value) ? getString(R.string.unknown_value) : value;

            title.setText(value);
            title.setTextColor(getResources().getColor(titleColor));

            itemView.setTag(entry);
        }
    }

    private class Adapter extends RecyclerView.Adapter<ViewHolder> {
        private JSONArray model;

        void setModel(JSONArray model) {
            this.model = model;
            this.notifyDataSetChanged();
        }

        @Override
        public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
            final LayoutInflater inflater = LayoutInflater.from(parent.getContext());
            final View view = inflater.inflate(R.layout.simple_list_item, parent, false);
            view.setOnClickListener(onItemClickListener);
            view.setOnLongClickListener(onItemLongClickListener);
            return new ViewHolder(view);
        }

        @Override
        public void onBindViewHolder(ViewHolder holder, int position) {
            holder.bind(model.optJSONObject(position));
        }

        @Override
        public int getItemCount() {
            return (model == null) ? 0 : model.length();
        }
    }
}
