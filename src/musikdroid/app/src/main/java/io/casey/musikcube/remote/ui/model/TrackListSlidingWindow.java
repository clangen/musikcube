package io.casey.musikcube.remote.ui.model;

import android.support.v7.widget.RecyclerView;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.LinkedHashMap;
import java.util.Map;

import io.casey.musikcube.remote.websocket.Messages;
import io.casey.musikcube.remote.websocket.SocketMessage;
import io.casey.musikcube.remote.websocket.WebSocketService;

public class TrackListSlidingWindow<TrackType> {
    private static final int MAX_SIZE = 150;
    public static final int DEFAULT_WINDOW_SIZE = 75;

    private int count = 0;
    private RecyclerView recyclerView;
    private WebSocketService wss;
    private Mapper<TrackType> mapper;
    private QueryFactory queryFactory;
    private int scrollState = RecyclerView.SCROLL_STATE_IDLE;
    private int queryOffset = -1, queryLimit = -1;
    private int initialPosition = -1;
    private int windowSize = DEFAULT_WINDOW_SIZE;
    private OnMetadataLoadedListener loadedListener;
    boolean connected = false;

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

    public interface OnMetadataLoadedListener {
        void onMetadataLoaded(int offset, int count);
        void onReloaded(int count);
    }

    public interface QueryFactory {
        SocketMessage getRequeryMessage();
        SocketMessage getPageAroundMessage(int offset, int limit);
    }

    public TrackListSlidingWindow(RecyclerView recyclerView,
                                  WebSocketService wss,
                                  QueryFactory queryFactory,
                                  Mapper<TrackType> mapper) {
        this.recyclerView = recyclerView;
        this.wss = wss;
        this.queryFactory = queryFactory;
        this.mapper = mapper;
    }

    public TrackListSlidingWindow(WebSocketService wss,
                                  QueryFactory queryFactory,
                                  Mapper<TrackType> mapper) {
        this.recyclerView = null;
        this.wss = wss;
        this.queryFactory = queryFactory;
        this.mapper = mapper;
    }

    public void setQueryFactory(final QueryFactory factory) {
        this.queryFactory = factory;
        requery();
    }

    public void requery() {
        if (connected) {
            cancelMessages();

            boolean queried = false;

            if (queryFactory != null) {
                final SocketMessage message = queryFactory.getRequeryMessage();

                if (message != null) {
                    wss.send(message, this.client, (SocketMessage response) -> {
                        final int count = response.getIntOption(Messages.Key.COUNT, 0);
                        setCount(count);

                        if (initialPosition != -1 && recyclerView != null) {
                            recyclerView.scrollToPosition(initialPosition);
                            initialPosition = -1;
                        }

                        if (loadedListener != null) {
                            loadedListener.onReloaded(count);
                        }
                    });

                    queried = true;
                }
            }

            if (!queried) {
                setCount(0);
            }
        }
    }

    public void pause() {
        connected = false;
        this.wss.removeClient(this.client);

        if (this.recyclerView != null) {
            this.recyclerView.removeOnScrollListener(scrollListener);
        }
    }

    public void resume() {
        if (this.recyclerView != null) {
            this.recyclerView.addOnScrollListener(scrollListener);
        }

        this.wss.addClient(this.client);
        connected = true;
    }

    public void setInitialPosition(int initialIndex) {
        this.initialPosition = initialIndex;
    }

    public void setOnMetadataLoadedListener(OnMetadataLoadedListener loadedListener) {
        this.loadedListener = loadedListener;
    }

    public void setWindowSize(int windowSize) {
        this.windowSize = windowSize;
    }

    public void setCount(int count) {
        this.count = count;
        invalidateCache();
        cancelMessages();
        notifyAdapterChanged();
        notifyMetadataLoaded(0, 0);
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
        if (!connected) {
            return;
        }

        if (index >= queryOffset && index <= queryOffset + queryLimit) {
            return; /* already in flight */
        }

        int offset = Math.max(0, index - 10); /* snag a couple before */
        int limit = windowSize;

        SocketMessage request = this.queryFactory.getPageAroundMessage(offset, limit);

        if (request != null) {
            cancelMessages();

            queryOffset = offset;
            queryLimit = limit;

            this.wss.send(request, this.client, (SocketMessage response) -> {
                this.queryOffset = this.queryLimit = -1;

                final JSONArray data = response.getJsonArrayOption(Messages.Key.DATA);
                final int responseOffset = response.getIntOption(Messages.Key.OFFSET);
                if (data != null) {
                    for (int i = 0; i < data.length(); i++) {
                        final JSONObject track = data.optJSONObject(i);
                        if (track != null) {
                            final CacheEntry<TrackType> entry = new CacheEntry<>();
                            entry.dirty = false;
                            entry.value = mapper.map(track);
                            cache.put(responseOffset + i, entry);
                        }
                    }

                    notifyAdapterChanged();
                    notifyMetadataLoaded(responseOffset, data.length());
                }
            });
        }
    }

    private void notifyAdapterChanged() {
        if (this.recyclerView != null) {
            final RecyclerView.Adapter<?> adapter = this.recyclerView.getAdapter();
            if (adapter != null) {
                adapter.notifyDataSetChanged();
            }
        }
    }

    private void notifyMetadataLoaded(int offset, int count) {
        if (this.loadedListener != null) {
            this.loadedListener.onMetadataLoaded(offset, count);
        }
    }

    private RecyclerView.OnScrollListener scrollListener = new RecyclerView.OnScrollListener() {
        @Override
        public void onScrollStateChanged(RecyclerView recyclerView, int newState) {
            scrollState = newState;
            if (newState == RecyclerView.SCROLL_STATE_IDLE) {
                notifyAdapterChanged();
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

        @Override
        public void onInvalidPassword() {
        }
    };
}
