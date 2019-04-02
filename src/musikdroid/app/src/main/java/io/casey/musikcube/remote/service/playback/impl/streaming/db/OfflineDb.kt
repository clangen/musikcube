package io.casey.musikcube.remote.service.playback.impl.streaming.db

import androidx.room.Database
import androidx.room.RoomDatabase
import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.injection.DaggerDataComponent
import io.casey.musikcube.remote.service.playback.impl.remote.Metadata
import io.casey.musikcube.remote.service.playback.impl.streaming.StreamProxy
import io.casey.musikcube.remote.service.websocket.Messages
import io.casey.musikcube.remote.service.websocket.SocketMessage
import io.casey.musikcube.remote.service.websocket.WebSocketService
import io.reactivex.Single
import io.reactivex.android.schedulers.AndroidSchedulers
import io.reactivex.schedulers.Schedulers
import org.json.JSONArray
import org.json.JSONObject
import java.util.*
import javax.inject.Inject

@Database(entities = [OfflineTrack::class], version = 1)
abstract class OfflineDb : RoomDatabase() {
    @Inject lateinit var wss: WebSocketService
    @Inject lateinit var streamProxy: StreamProxy

    init {
        DaggerDataComponent.builder()
            .appComponent(Application.appComponent)
            .build().inject(this)

        wss.addInterceptor{ message, responder ->
            var result = false
            if (Messages.Request.QueryTracksByCategory.matches(message.name)) {
                val category = message.getStringOption(Messages.Key.CATEGORY)
                if (Metadata.Category.OFFLINE == category) {
                    queryTracks(message, responder)
                    result = true
                }
            }
            result
        }

        prune()
    }

    abstract fun trackDao(): OfflineTrackDao

    fun prune() {
        @Suppress("unused")
        Single.fromCallable {
            val uris = trackDao().queryUris()
            val toDelete = ArrayList<String>()

            uris.forEach {
                if (!streamProxy.isCached(it)) {
                    toDelete.add(it)
                }
            }

            if (toDelete.size > 0) {
                trackDao().deleteByUri(toDelete)
            }

            true
        }
        .subscribeOn(Schedulers.io())
        .observeOn(AndroidSchedulers.mainThread())
        .subscribe({ }, { })
    }

    private fun queryTracks(message: SocketMessage, responder: WebSocketService.Responder) {
        Single.fromCallable {
            val dao = trackDao()

            val countOnly = message.getBooleanOption(Messages.Key.COUNT_ONLY, false)
            val filter = message.getStringOption(Messages.Key.FILTER, "")

            val tracks = JSONArray()
            val options = JSONObject()

            if (countOnly) {
                val count = if (filter.isEmpty()) dao.countTracks() else dao.countTracks(filter)
                options.put(Messages.Key.COUNT, count)
            }
            else {
                val offset = message.getIntOption(Messages.Key.OFFSET, -1)
                val limit = message.getIntOption(Messages.Key.LIMIT, -1)

                val offlineTracks: List<OfflineTrack> =
                    if (filter.isEmpty()) {
                        if (offset == -1 || limit == -1)
                            dao.queryTracks() else dao.queryTracks(limit, offset)
                    }
                    else {
                        if (offset == -1 || limit == -1)
                            dao.queryTracks(filter) else dao.queryTracks(filter, limit, offset)
                    }

                for (track in offlineTracks) {
                    tracks.put(track.toJSONObject())
                }

                options.put(Messages.Key.OFFSET, offset)
                options.put(Messages.Key.LIMIT, limit)
            }

            options.put(Messages.Key.DATA, tracks)

            val response = SocketMessage.Builder
                .respondTo(message).withOptions(options).build()

            responder.respond(response)

            true
        }
        .subscribeOn(Schedulers.io())
        .observeOn(AndroidSchedulers.mainThread())
        .subscribe()
    }
}
