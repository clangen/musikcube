package io.casey.musikcube.remote.offline;

import android.arch.persistence.room.Database;
import android.arch.persistence.room.RoomDatabase;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;

import io.casey.musikcube.remote.Application;
import io.casey.musikcube.remote.playback.StreamProxy;
import io.casey.musikcube.remote.util.Strings;
import io.casey.musikcube.remote.websocket.Messages;
import io.casey.musikcube.remote.websocket.SocketMessage;
import io.casey.musikcube.remote.websocket.WebSocketService;
import io.reactivex.Single;
import io.reactivex.android.schedulers.AndroidSchedulers;
import io.reactivex.schedulers.Schedulers;

@Database(entities = {OfflineTrack.class}, version = 1)
public abstract class OfflineDb extends RoomDatabase {
    public OfflineDb() {
        WebSocketService.getInstance(Application.Companion.getInstance())
            .addInterceptor((message, responder) -> {
                if (Messages.Request.QueryTracksByCategory.is(message.getName())) {
                    final String category = message.getStringOption(Messages.Key.CATEGORY);
                    if (Messages.Category.OFFLINE.equals(category)) {
                        queryTracks(message, responder);
                        return true;
                    }
                }
                return false;
            });

        prune();
    }

    public abstract OfflineTrackDao trackDao();

    public void prune() {
        Single.fromCallable(() -> {
            List<String> uris = trackDao().queryUris();
            List<String> toDelete = new ArrayList<>();

            for (final String uri : uris) {
                if (!StreamProxy.Companion.isCached(uri)) {
                    toDelete.add(uri);
                }
            }

            if (toDelete.size() > 0) {
                trackDao().deleteByUri(toDelete);
            }

            return true;
        })
        .subscribeOn(Schedulers.io())
        .observeOn(AndroidSchedulers.mainThread())
        .subscribe();
    }

    public void queryTracks(final SocketMessage message, final WebSocketService.Responder responder) {
        Single.fromCallable(() -> {
            final OfflineTrackDao dao = trackDao();

            final boolean countOnly = message.getBooleanOption(Messages.Key.COUNT_ONLY, false);
            final String filter = message.getStringOption(Messages.Key.FILTER, "");

            final JSONArray tracks = new JSONArray();
            final JSONObject options = new JSONObject();

            if (countOnly) {
                final int count = Strings.empty(filter) ? dao.countTracks() : dao.countTracks(filter);
                options.put(Messages.Key.COUNT, count);
            }
            else {
                final int offset = message.getIntOption(Messages.Key.OFFSET, -1);
                final int limit = message.getIntOption(Messages.Key.LIMIT, -1);

                List<OfflineTrack> offlineTracks;

                if (Strings.empty(filter)) {
                    offlineTracks = (offset == -1 || limit == -1)
                        ? dao.queryTracks() : dao.queryTracks(limit, offset);
                }
                else {
                    offlineTracks = (offset == -1 || limit == -1)
                        ? dao.queryTracks(filter) : dao.queryTracks(filter, limit, offset);
                }

                for (final OfflineTrack track : offlineTracks) {
                    tracks.put(track.toJSONObject());
                }

                options.put(Messages.Key.OFFSET, offset);
                options.put(Messages.Key.LIMIT, limit);
            }

            options.put(Messages.Key.DATA, tracks);

            final SocketMessage response = SocketMessage.Builder
                .respondTo(message).withOptions(options).build();

            responder.respond(response);

            return true;
        })
        .subscribeOn(Schedulers.io())
        .observeOn(AndroidSchedulers.mainThread())
        .subscribe();
    }
}
