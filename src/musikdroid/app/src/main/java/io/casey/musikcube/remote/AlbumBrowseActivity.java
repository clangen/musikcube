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

import static io.casey.musikcube.remote.Messages.Key;

public class AlbumBrowseActivity extends WebSocketActivityBase implements Filterable {
    public static final String EXTRA_TITLE = "extra_title";
    private static final String EXTRA_CATEGORY_NAME = "extra_category_name";
    private static final String EXTRA_CATEGORY_ID = "extra_category_id";

    public static Intent getStartIntent(final Context context) {
        return new Intent(context, AlbumBrowseActivity.class);
    }

    public static Intent getStartIntent(final Context context, final String categoryName, long categoryId) {
        return new Intent(context, AlbumBrowseActivity.class)
            .putExtra(EXTRA_CATEGORY_NAME, categoryName)
            .putExtra(EXTRA_CATEGORY_ID, categoryId);
    }

    public static Intent getStartIntent(final Context context,
                                        final String categoryName,
                                        long categoryId,
                                        final String categoryValue)
    {
        final Intent intent = getStartIntent(context, categoryName, categoryId);

        if (Strings.notEmpty(categoryValue)) {
            intent.putExtra(
                AlbumBrowseActivity.EXTRA_TITLE,
                context.getString(R.string.albums_by_title, categoryValue));
        }

        return intent;
    }

    public static Intent getStartIntent(final Context context,
                                        final String categoryName,
                                        final JSONObject categoryJson)
    {
        final String value = categoryJson.optString(Messages.Key.VALUE);
        final long categoryId = categoryJson.optLong(Messages.Key.ID);
        return getStartIntent(context, categoryName, categoryId, value);
    }

    private WebSocketService wss;
    private Adapter adapter;
    private TransportFragment transport;
    private String categoryName;
    private long categoryId;
    private String filter = "";

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        this.categoryName = getIntent().getStringExtra(EXTRA_CATEGORY_NAME);
        this.categoryId = getIntent().getLongExtra(EXTRA_CATEGORY_ID, categoryId);

        setContentView(R.layout.recycler_view_activity);
        setTitle(R.string.albums_title);

        final String title = getIntent().getStringExtra(EXTRA_TITLE);
        if (Strings.notEmpty(title)) {
            setTitle(title);
        }

        this.wss = getWebSocketService();
        this.adapter = new Adapter();

        final RecyclerView recyclerView = (RecyclerView) findViewById(R.id.recycler_view);

        Views.setupDefaultRecyclerView(this, recyclerView, adapter);

        transport = Views.addTransportFragment(this,
            (TransportFragment fragment) -> adapter.notifyDataSetChanged());
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        Views.initSearchMenu(this, menu, this);
        return true;
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

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (resultCode == Navigation.ResponseCode.PLAYBACK_STARTED) {
            setResult(Navigation.ResponseCode.PLAYBACK_STARTED);
            finish();
        }

        super.onActivityResult(requestCode, resultCode, data);
    }

    private void requery() {
        final SocketMessage message =
            SocketMessage.Builder
                .request(Messages.Request.QueryAlbums)
                .addOption(Messages.Key.CATEGORY, categoryName)
                .addOption(Messages.Key.CATEGORY_ID, categoryId)
                .addOption(Key.FILTER, filter)
                .build();

        wss.send(message, socketClient, (SocketMessage response) ->
            adapter.setModel(response.getJsonArrayOption(Messages.Key.DATA)));
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
        final JSONObject album = (JSONObject) view.getTag();
        final long id = album.optLong(Key.ID);
        final String title = album.optString(Key.TITLE, "");

        final Intent intent = TrackListActivity.getStartIntent(
            AlbumBrowseActivity.this, Messages.Category.ALBUM, id, title);

        startActivityForResult(intent, Navigation.RequestCode.ALBUM_TRACKS_ACTIVITY);
    };

    private class ViewHolder extends RecyclerView.ViewHolder {
        private final TextView title;
        private final TextView subtitle;

        ViewHolder(View itemView) {
            super(itemView);
            title = (TextView) itemView.findViewById(R.id.title);
            subtitle = (TextView) itemView.findViewById(R.id.subtitle);
        }

        void bind(JSONObject entry) {
            long playingId = transport.getModel().getTrackValueLong(Key.ALBUM_ID, -1);
            long entryId = entry.optLong(Key.ID);

            int titleColor = R.color.theme_foreground;
            int subtitleColor = R.color.theme_disabled_foreground;

            if (playingId != -1 && entryId == playingId) {
                titleColor = R.color.theme_green;
                subtitleColor = R.color.theme_yellow;
            }

            title.setText(entry.optString(Key.TITLE, "-"));
            title.setTextColor(getResources().getColor(titleColor));

            subtitle.setText(entry.optString(Key.ALBUM_ARTIST, "-"));
            subtitle.setTextColor(getResources().getColor(subtitleColor));

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
