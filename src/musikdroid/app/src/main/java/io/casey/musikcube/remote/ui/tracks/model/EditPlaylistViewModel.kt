package io.casey.musikcube.remote.ui.tracks.model

import io.casey.musikcube.remote.framework.ViewModel
import io.casey.musikcube.remote.service.websocket.Messages.Category.Companion.PLAYLISTS
import io.casey.musikcube.remote.service.websocket.model.IDataProvider
import io.casey.musikcube.remote.service.websocket.model.ITrack
import io.casey.musikcube.remote.service.websocket.model.impl.remote.RemoteTrack
import io.reactivex.Observable
import io.reactivex.disposables.Disposable
import io.reactivex.rxkotlin.subscribeBy
import org.json.JSONObject

class EditPlaylistViewModel(private val playlistId: Long): ViewModel<EditPlaylistViewModel.Status>() {
    enum class Status { NotLoaded, Error, Loading, Saving, Updated }

    private data class CacheEntry(var track: ITrack, var dirty: Boolean = false)

    private var metadataDisposable: Disposable? = null
    private var requestOffset = -1
    private var dataProvider: IDataProvider? = null
    private var externalIds: MutableList<String> = mutableListOf()

    private val cache = object : LinkedHashMap<String, CacheEntry>() {
        override fun removeEldestEntry(eldest: MutableMap.MutableEntry<String, CacheEntry>): Boolean = size >= MAX_SIZE
    }

    var modified: Boolean = false
        private set(value) {
            field = value
        }

    fun attach(dataProvider: IDataProvider) {
        this.dataProvider = dataProvider
    }

    override fun onDestroy() {
        super.onDestroy()
        this.dataProvider = null
    }

    var status: Status = Status.NotLoaded
        private set(value) {
            field = value
            publish(value)
        }

    val count: Int
        get() {
            if (!paused && externalIds.isEmpty() && status != Status.Loading) {
                if (!modified) {
                    refreshTrackIds()
                }
            }
            return externalIds.size
        }

    operator fun get(index: Int): ITrack {
        val entry = cache[externalIds[index]]
        if (entry == null) {
            refreshPageAround(index)
        }
        return entry?.track ?: DEFAULT_TRACK
    }

    fun save(): Observable<Long> {
        if (!modified) {
            return Observable.just(playlistId)
        }

        return dataProvider?.overwritePlaylistWithExternalIds(
            playlistId, externalIds.toList()) ?: Observable.just(-1L)
    }

    fun remove(index: Int) {
        externalIds.removeAt(index)
        modified = true
    }

    fun move(from: Int, to: Int) {
        val id = externalIds.removeAt(from)
        externalIds.add(to, id)
        modified = true
    }

    private fun refreshTrackIds() {
        status = Status.Loading
        dataProvider?.let {
            status = Status.Loading

            it.getTrackIdsByCategory(PLAYLISTS, playlistId).subscribeBy(
                onNext = { result ->
                    externalIds = result.toMutableList()
                    status = Status.Updated
                },
                onError = {
                    status = Status.Error
                })
        }
    }

    private fun refreshPageAround(offset: Int) {
        if (requestOffset != -1 && offset >= requestOffset && offset < requestOffset + PAGE_SIZE) {
            return /* in flight */
        }

        dataProvider?.let {
            metadataDisposable?.dispose()
            metadataDisposable = null

            requestOffset = Math.max(0, offset - PAGE_SIZE / 4)
            val end = Math.min(externalIds.size, requestOffset + PAGE_SIZE)
            val ids = mutableSetOf<String>()
            for (i in requestOffset until end) {
                val id = externalIds[i]
                val entry = cache[id]
                if (entry == null || entry.dirty) {
                    ids.add(id)
                }
            }

            if (ids.isNotEmpty()) {
                status = Status.Loading

                metadataDisposable = it.getTracks(ids)
                    .flatMapIterable { list: Map<String, ITrack> -> list.asIterable() }
                    .subscribeBy(
                        onNext = { entry: Map.Entry<String, ITrack> ->
                            cache.put(entry.key, CacheEntry(entry.value))
                        },
                        onError = {
                            status = Status.Error
                            requestOffset = -1
                        },
                        onComplete = {
                            status = Status.Updated
                            requestOffset = -1
                        })
            }
        }
    }

    companion object {
        private val DEFAULT_TRACK = RemoteTrack(JSONObject())
        private val PAGE_SIZE = 40
        private val MAX_SIZE = 150
    }
}