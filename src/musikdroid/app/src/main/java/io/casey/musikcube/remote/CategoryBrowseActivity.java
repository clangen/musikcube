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

    private static final Map<String, String> CATEGORY_NAME_TO_ID = new HashMap<>();
    private static final Map<String, Integer> CATEGORY_NAME_TO_TITLE = new HashMap<>();

    static {
        CATEGORY_NAME_TO_ID.put(Messages.Key.ALBUM_ARTIST, Messages.Key.ALBUM_ARTIST_ID);
        CATEGORY_NAME_TO_ID.put(Messages.Key.GENRE, Messages.Key.GENRE_ID);
        CATEGORY_NAME_TO_ID.put(Messages.Key.ARTIST, Messages.Key.ARTIST_ID);
        CATEGORY_NAME_TO_ID.put(Messages.Key.ALBUM, Messages.Key.ALBUM_ID);

        CATEGORY_NAME_TO_TITLE.put(Messages.Key.ALBUM_ARTIST, R.string.artists_title);
        CATEGORY_NAME_TO_TITLE.put(Messages.Key.GENRE, R.string.genres_title);
        CATEGORY_NAME_TO_TITLE.put(Messages.Key.ARTIST, R.string.artists_title);
        CATEGORY_NAME_TO_TITLE.put(Messages.Key.ALBUM, R.string.albums_title);
    }

    public static Intent getStartIntent(final Context context, final String category) {
        return new Intent(context, CategoryBrowseActivity.class)
            .putExtra(EXTRA_CATEGORY, category);
    }

    private String category;
    private Adapter adapter;
    private String filter;
    private TransportFragment transport;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        this.category = getIntent().getStringExtra(EXTRA_CATEGORY);
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
        Views.initSearchMenu(this, menu, this);
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
        this.filter = filter;
        requery();
    }

    @Override
    protected WebSocketService.Client getWebSocketServiceClient() {
        return socketClient;
    }

    private void requery() {
        final SocketMessage request = SocketMessage.Builder
            .request(Messages.Request.QueryCategory)
            .addOption(Messages.Key.CATEGORY, category)
            .addOption(Messages.Key.FILTER, filter)
            .build();

        getWebSocketService().send(request, this.socketClient, (SocketMessage response) -> {
            JSONArray data = response.getJsonArrayOption(Messages.Key.DATA);
            if (data != null && data.length() > 0) {
                adapter.setModel(data);
            }
        });
    }

    private WebSocketService.Client socketClient = new WebSocketService.Client() {
        @Override
        public void onStateChanged(WebSocketService.State newState, WebSocketService.State oldState) {
            if (newState == WebSocketService.State.Connected) {
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
        final long categoryId = entry.optLong(Messages.Key.ID);
        final String value = entry.optString(Messages.Key.VALUE);

        final Intent intent = AlbumBrowseActivity.getStartIntent(this, category, categoryId);

        if (Strings.notEmpty(value)) {
            intent.putExtra(
                AlbumBrowseActivity.EXTRA_TITLE,
                getString(R.string.albums_by_title, value));
        }

        startActivityForResult(intent, Navigation.RequestCode.ALBUM_BROWSE_ACTIVITY);
    };

    private View.OnLongClickListener onItemLongClickListener = (View view) -> {
        final JSONObject entry = (JSONObject) view.getTag();
        final long categoryId = entry.optLong(Messages.Key.ID);
        final String value = entry.optString(Messages.Key.VALUE);

        final Intent intent = TrackListActivity.getStartIntent(this, category, categoryId);

        if (Strings.notEmpty(value)) {
            intent.putExtra(
                TrackListActivity.EXTRA_TITLE,
                getString(R.string.songs_from_category, value));
        }

        startActivityForResult(intent, Navigation.RequestCode.CATEGORY_TRACKS_ACTIVITY);

        return true;
    };

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

            title.setText(entry.optString(Messages.Key.VALUE, "-"));
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
