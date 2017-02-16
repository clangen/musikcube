package io.casey.musikcube.remote;

import android.support.v7.widget.RecyclerView;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.LinkedHashMap;
import java.util.Map;

public class TrackListScrollCache<TrackType> {
    private static final int MAX_SIZE = 150;
    public static final int DEFAULT_WINDOW_SIZE = 75 ;

    private int count = 0;
    private RecyclerView recyclerView;
    private RecyclerView.Adapter<?> adapter;
    private WebSocketService wss;
    private Mapper<TrackType> mapper;
    private QueryFactory queryFactory;
    private int scrollState = RecyclerView.SCROLL_STATE_IDLE;
    private int queryOffset = -1, queryLimit = -1;
    private int initialPosition = -1;

    private static class CacheEntry<TrackType> {
        TrackType value;
        boolean dirty;
    }

    private Map<Integer, CacheEntry<TrackType>> cache = new LinkedHashMap<Integer, CacheEntry<TrackType>>() {
        protected boolean removeEldestEntry(Map.Entry<Integer, CacheEntry<TrackType>> eldest) {
            return size() >= MAX_SIZE;
        }
    };

    public interface Mapper<TrackType> {
        TrackType map(final JSONObject track);
    }

    public interface QueryFactory {
        SocketMessage getRequeryMessage();
        SocketMessage getPageAroundMessage(int offset, int limit);
    }

    public TrackListScrollCache(RecyclerView recyclerView,
                                RecyclerView.Adapter<?> adapter,
                                WebSocketService wss,
                                QueryFactory queryFactory,
                                Mapper<TrackType> mapper) {
        this.recyclerView = recyclerView;
        this.adapter = adapter;
        this.wss = wss;
        this.queryFactory = queryFactory;
        this.mapper = mapper;
    }

    public void requery() {
        cancelMessages();

        final SocketMessage message = queryFactory.getRequeryMessage();

        wss.send(message, this.client, (SocketMessage response) -> {
            setCount(response.getIntOption(Messages.Key.COUNT, 0));

            if (initialPosition != -1) {
                recyclerView.scrollToPosition(initialPosition);
                initialPosition = -1;
            }
        });
    }

    public void pause() {
        this.recyclerView.removeOnScrollListener(scrollListener);
        this.wss.removeClient(this.client);
    }

    public void resume() {
        this.recyclerView.addOnScrollListener(scrollListener);
        this.wss.addClient(this.client);
    }

    public void setInitialPosition(int initialIndex) {
        this.initialPosition = initialIndex;
    }

    public void setCount(int count) {
        this.count = count;
        invalidateCache();
        cancelMessages();
        adapter.notifyDataSetChanged();
    }

    public int getCount() {
        return count;
    }

    public TrackType getTrack(int index) {
        final CacheEntry<TrackType> track = cache.get(index);

        if (track == null || track.dirty) {
            if (scrollState == RecyclerView.SCROLL_STATE_IDLE) {
                this.getPageAround(index);
            }
        }

        return (track == null) ? null : track.value;
    }

    private void invalidateCache() {
        for (final CacheEntry<TrackType> entry : cache.values()) {
            entry.dirty = true;
        }
    }

    private void cancelMessages() {
        this.queryOffset = this.queryLimit = -1;
        this.wss.cancelMessages(this.client);
    }

    private void getPageAround(int index) {
        if (index >= queryOffset && index <= queryOffset + queryLimit) {
            return; /* already in flight */
        }

        cancelMessages();

        queryOffset = Math.max(0, index - 10); /* snag a couple before */
        queryLimit = DEFAULT_WINDOW_SIZE;
        SocketMessage request = this.queryFactory.getPageAroundMessage(queryOffset, queryLimit);

        this.wss.send(request, this.client, (SocketMessage response) -> {
            this.queryOffset = this.queryLimit = -1;

            final JSONArray data = response.getJsonArrayOption(Messages.Key.DATA);
            final int offset = response.getIntOption(Messages.Key.OFFSET);
            if (data != null) {
                for (int i = 0; i < data.length(); i++) {
                    final JSONObject track = data.optJSONObject(i);
                    if (track != null) {
                        final CacheEntry<TrackType> entry = new CacheEntry<>();
                        entry.dirty = false;
                        entry.value = mapper.map(track);
                        cache.put(offset + i, entry);
                    }
                }

                adapter.notifyDataSetChanged();
            }
        });
    }

    private RecyclerView.OnScrollListener scrollListener = new RecyclerView.OnScrollListener() {
        @Override
        public void onScrollStateChanged(RecyclerView recyclerView, int newState) {
            scrollState = newState;
            if (newState == RecyclerView.SCROLL_STATE_IDLE) {
                adapter.notifyDataSetChanged();
            }
        }
    };

    private WebSocketService.Client client = new WebSocketService.Client() {
        @Override
        public void onStateChanged(WebSocketService.State newState, WebSocketService.State oldState) {

        }

        @Override
        public void onMessageReceived(SocketMessage message) {
            if (message.getType() == SocketMessage.Type.Broadcast) {
                if (Messages.Broadcast.PlayQueueChanged.is(message.getName())) {
                    requery();
                }
            }
        }
    };
}
