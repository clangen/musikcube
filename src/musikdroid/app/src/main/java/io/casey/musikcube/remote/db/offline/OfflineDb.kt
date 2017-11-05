package io.casey.musikcube.remote.db.offline

import android.arch.persistence.room.Database
import android.arch.persistence.room.RoomDatabase
import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.injection.DaggerDataComponent
import io.casey.musikcube.remote.playback.StreamProxy
import io.casey.musikcube.remote.util.Strings
import io.casey.musikcube.remote.websocket.Messages
import io.casey.musikcube.remote.websocket.SocketMessage
import io.casey.musikcube.remote.websocket.WebSocketService
import io.reactivex.Single
import io.reactivex.android.schedulers.AndroidSchedulers
import io.reactivex.schedulers.Schedulers
import org.json.JSONArray
import org.json.JSONObject
import java.util.*
import javax.inject.Inject

@Database(entities = arrayOf(OfflineTrack::class), version = 1)
abstract class OfflineDb : RoomDatabase() {
    @Inject lateinit var wss: WebSocketService

    init {
        DaggerDataComponent.builder()
            .appComponent(Application.appComponent)
            .build().inject(this)

        wss.addInterceptor({ message, responder ->
            var result = false
            if (Messages.Request.QueryTracksByCategory.matches(message.name)) {
                val category = message.getStringOption(Messages.Key.CATEGORY)
                if (Messages.Category.OFFLINE == category) {
                    queryTracks(message, responder)
                    result = true
                }
            }
            result
        })

        prune()
    }

    abstract fun trackDao(): OfflineTrackDao

    fun prune() {
        Single.fromCallable {
            val uris = trackDao().queryUris()
            val toDelete = ArrayList<String>()

            uris.forEach {
                if (!StreamProxy.isCached(it)) {
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
        .subscribe({ _ -> }, { })
    }

    private fun queryTracks(message: SocketMessage, responder: WebSocketService.Responder) {
        Single.fromCallable {
            val dao = trackDao()

            val countOnly = message.getBooleanOption(Messages.Key.COUNT_ONLY, false)
            val filter = message.getStringOption(Messages.Key.FILTER, "")

            val tracks = JSONArray()
            val options = JSONObject()

            if (countOnly) {
                val count = if (Strings.empty(filter)) dao.countTracks() else dao.countTracks(filter)
                options.put(Messages.Key.COUNT, count)
            }
            else {
                val offset = message.getIntOption(Messages.Key.OFFSET, -1)
                val limit = message.getIntOption(Messages.Key.LIMIT, -1)

                val offlineTracks: List<OfflineTrack>

                if (Strings.empty(filter)) {
                    offlineTracks = if (offset == -1 || limit == -1)
                        dao.queryTracks() else dao.queryTracks(limit, offset)
                }
                else {
                    offlineTracks = if (offset == -1 || limit == -1)
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
