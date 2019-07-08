package io.casey.musikcube.remote.service.websocket.model.impl.remote

import io.casey.musikcube.remote.service.playback.impl.remote.Metadata
import io.casey.musikcube.remote.service.websocket.Messages
import io.casey.musikcube.remote.service.websocket.SocketMessage
import io.casey.musikcube.remote.service.websocket.WebSocketService
import io.casey.musikcube.remote.service.websocket.model.*
import io.reactivex.Observable
import io.reactivex.android.schedulers.AndroidSchedulers
import io.reactivex.disposables.CompositeDisposable
import io.reactivex.rxkotlin.subscribeBy
import io.reactivex.schedulers.Schedulers
import io.reactivex.subjects.PublishSubject
import io.reactivex.subjects.ReplaySubject
import org.json.JSONArray
import org.json.JSONObject

class RemoteMetadataProxy(private val service: WebSocketService) : IMetadataProxy {
    private var disposables = CompositeDisposable()
    private var currentState = mapState(service.state)

    private val connectionStatePublisher: ReplaySubject<
        Pair<IMetadataProxy.State, IMetadataProxy.State>> = ReplaySubject.createWithSize(1)

    private val playQueueStatePublisher: PublishSubject<Unit> = PublishSubject.create()

    private val authFailurePublisher: PublishSubject<Unit> = PublishSubject.create()

    init {
        disposables.add(observeState().subscribe({ updatedStates ->
            currentState = updatedStates.first
        }, { /*error */ }))
    }

    override val state: IMetadataProxy.State
        get() = currentState

    override fun getAlbums(filter: String): Observable<List<IAlbum>> =
        getAlbumsForCategory("", 0, filter)

    override fun getAlbumsForCategory(categoryType: String, categoryId: Long, filter: String): Observable<List<IAlbum>> {
        val message = SocketMessage.Builder
            .request(Messages.Request.QueryAlbums)
            .addOption(Messages.Key.CATEGORY, categoryType)
            .addOption(Messages.Key.CATEGORY_ID, categoryId)
            .addOption(Messages.Key.FILTER, filter)
            .build()

        return service.observe(message, client)
            .observeOn(Schedulers.computation())
            .flatMap<List<IAlbum>> { socketMessage -> toAlbumList(socketMessage) }
            .observeOn(AndroidSchedulers.mainThread())
    }

    override fun getTrackCount(filter: String): Observable<Int> {
        val message = SocketMessage.Builder
            .request(Messages.Request.QueryTracks)
            .addOption(Messages.Key.FILTER, filter)
            .addOption(Messages.Key.COUNT_ONLY, true)
            .build()

        return service.observe(message, client)
            .observeOn(Schedulers.computation())
            .flatMap<Int> { socketMessage -> toCount(socketMessage) }
            .observeOn(AndroidSchedulers.mainThread())
    }

    override fun getTracks(limit: Int, offset: Int, filter: String): Observable<List<ITrack>> {
        val builder = SocketMessage.Builder
            .request(Messages.Request.QueryTracks)
            .addOption(Messages.Key.FILTER, filter)

        if (limit > 0 && offset >= 0) {
            builder.addOption(Messages.Key.LIMIT, limit)
            builder.addOption(Messages.Key.OFFSET, offset)
        }

        return service.observe(builder.build(), client)
            .observeOn(Schedulers.computation())
            .flatMap<List<ITrack>> { socketMessage -> toTrackList(socketMessage) }
            .observeOn(AndroidSchedulers.mainThread())
    }

    override fun getTracks(externalIds: Set<String>): Observable<Map<String, ITrack>> {
        val jsonIds = JSONArray()
        externalIds.forEach { jsonIds.put(it) }

        val message = SocketMessage.Builder
            .request(Messages.Request.QueryTracksByExternalIds)
            .addOption(Messages.Key.EXTERNAL_IDS, jsonIds)
            .build()

        return service.observe(message, client)
            .observeOn(Schedulers.computation())
            .flatMap<Map<String, ITrack>> { socketMessage ->
                val tracks = HashMap<String, ITrack>()
                val json = socketMessage.getJsonObjectOption(Messages.Key.DATA, JSONObject())!!
                json.keys().forEach { tracks[it] = RemoteTrack(json.getJSONObject(it)) }
                Observable.just(tracks)
            }
            .observeOn(AndroidSchedulers.mainThread())
    }

    override fun listCategories(): Observable<List<String>> {
        val message = SocketMessage.Builder
            .request(Messages.Request.ListCategories)
            .build()

        return service.observe(message, client)
            .observeOn(Schedulers.computation())
            .flatMap<List<String>> { socketMessage -> toStringList(socketMessage) }
            .observeOn(AndroidSchedulers.mainThread())
    }

    override fun getTracks(filter: String): Observable<List<ITrack>> =
        getTracks(-1, -1, filter)

    override fun getTrackCountByCategory(category: String, id: Long, filter: String): Observable<Int> {
        val message = SocketMessage.Builder
            .request(Messages.Request.QueryTracksByCategory)
            .addOption(Messages.Key.FILTER, filter)
            .addOption(Messages.Key.CATEGORY, category)
            .addOption(Messages.Key.ID, id)
            .addOption(Messages.Key.COUNT_ONLY, true)
            .build()

        return service.observe(message, client)
            .flatMap<Int> { socketMessage -> toCount(socketMessage) }
            .observeOn(AndroidSchedulers.mainThread())
    }

    override fun getTrackIdsByCategory(category: String, id: Long, filter: String): Observable<List<String>> {
        val message = SocketMessage.Builder
            .request(Messages.Request.QueryTracksByCategory)
            .addOption(Messages.Key.FILTER, filter)
            .addOption(Messages.Key.CATEGORY, category)
            .addOption(Messages.Key.ID, id)
            .addOption(Messages.Key.COUNT_ONLY, false)
            .addOption(Messages.Key.IDS_ONLY, true)
            .build()

        return service.observe(message, client)
            .flatMap<List<String>> { socketMessage -> toStringList(socketMessage) }
            .observeOn(AndroidSchedulers.mainThread())
    }

    override fun getTracksByCategory(category: String, id: Long, filter: String): Observable<List<ITrack>> =
        getTracksByCategory(category, id, -1, -1, filter)

    override fun getTracksByCategory(category: String, id: Long, limit: Int, offset: Int, filter: String): Observable<List<ITrack>> {
        val builder = SocketMessage.Builder
            .request(Messages.Request.QueryTracksByCategory)
            .addOption(Messages.Key.FILTER, filter)
            .addOption(Messages.Key.CATEGORY, category)
            .addOption(Messages.Key.ID, id)

        if (limit > 0 && offset >= 0) {
            builder.addOption(Messages.Key.LIMIT, limit)
            builder.addOption(Messages.Key.OFFSET, offset)
        }

        return service.observe(builder.build(), client)
            .observeOn(Schedulers.computation())
            .flatMap<List<ITrack>> { socketMessage -> toTrackList(socketMessage) }
            .observeOn(AndroidSchedulers.mainThread())
    }

    override fun getPlayQueueTracksCount(type: PlayQueueType): Observable<Int> {
        val message = SocketMessage.Builder
            .request(Messages.Request.QueryPlayQueueTracks)
            .addOption(Messages.Key.COUNT_ONLY, true)
            .addOption(Messages.Key.TYPE, type.rawValue)
            .build()

        return service.observe(message, client)
            .flatMap<Int> { socketMessage -> toCount(socketMessage) }
            .observeOn(AndroidSchedulers.mainThread())
    }

    override fun getPlayQueueTracks(type: PlayQueueType): Observable<List<ITrack>> =
        getPlayQueueTracks(-1, -1, type)

    override fun getPlayQueueTracks(limit: Int, offset: Int, type: PlayQueueType): Observable<List<ITrack>> {
        val builder = SocketMessage.Builder
            .request(Messages.Request.QueryPlayQueueTracks)
            .addOption(Messages.Key.TYPE, type.rawValue)

        if (limit > 0 && offset >= 0) {
            builder.addOption(Messages.Key.LIMIT, limit)
            builder.addOption(Messages.Key.OFFSET, offset)
        }

        return service.observe(builder.build(), client)
            .observeOn(Schedulers.computation())
            .flatMap<List<ITrack>> { socketMessage -> toTrackList(socketMessage) }
            .observeOn(AndroidSchedulers.mainThread())
    }

    override fun getPlayQueueTrackIds(type: PlayQueueType): Observable<List<String>> =
        getPlayQueueTrackIds(-1, -1, type)

    override fun snapshotPlayQueue(): Observable<Boolean> {
        val message = SocketMessage.Builder
            .request(Messages.Request.SnapshotPlayQueue)
            .build()

        return service.observe(message, client)
            .flatMap<Boolean> { socketMessage -> isSuccessful(socketMessage) }
            .observeOn(AndroidSchedulers.mainThread())
    }

    override fun invalidatePlayQueueSnapshot() {
        val message = SocketMessage.Builder
            .request(Messages.Request.InvalidatePlayQueueSnapshot)
            .build()

        @Suppress("unused")
        service.observe(message, client)
            .flatMap<Boolean> { socketMessage -> isSuccessful(socketMessage) }
            .observeOn(AndroidSchedulers.mainThread())
            .subscribeBy(onError = { })
    }

    override fun getPlayQueueTrackIds(limit: Int, offset: Int, type: PlayQueueType): Observable<List<String>> {
        val builder = SocketMessage.Builder
            .request(Messages.Request.QueryPlayQueueTracks)
            .addOption(Messages.Key.IDS_ONLY, true)
            .addOption(Messages.Key.TYPE, type.rawValue)

        if (limit > 0 && offset >= 0) {
            builder.addOption(Messages.Key.LIMIT, limit)
            builder.addOption(Messages.Key.OFFSET, offset)
        }

        return service.observe(builder.build(), client)
            .observeOn(Schedulers.computation())
            .flatMap<List<String>> { socketMessage -> toStringList(socketMessage) }
            .observeOn(AndroidSchedulers.mainThread())
    }

    override fun getPlaylists(): Observable<List<IPlaylist>> {
        val message = SocketMessage.Builder
            .request(Messages.Request.QueryCategory)
            .addOption(Messages.Key.CATEGORY, Metadata.Category.PLAYLISTS)
            .build()

        return service.observe(message, client)
            .observeOn(Schedulers.computation())
            .flatMap<List<ICategoryValue>> { socketMessage ->
                toCategoryList(socketMessage, Metadata.Category.PLAYLISTS)
            }
            .flatMap { values ->
                Observable.just(values.filterIsInstance<IPlaylist>())
            }
            .observeOn(AndroidSchedulers.mainThread())
    }

    override fun getCategoryValues(type: String, predicateType: String, predicateId: Long, filter: String): Observable<List<ICategoryValue>> {
        val message = SocketMessage.Builder
            .request(Messages.Request.QueryCategory)
            .addOption(Messages.Key.CATEGORY, type)
            .addOption(Messages.Key.PREDICATE_CATEGORY, predicateType)
            .addOption(Messages.Key.PREDICATE_ID, predicateId)
            .addOption(Messages.Key.FILTER, filter)
            .build()

        return service.observe(message, client)
            .observeOn(Schedulers.computation())
            .flatMap<List<ICategoryValue>> { socketMessage -> toCategoryList(socketMessage, type) }
            .observeOn(AndroidSchedulers.mainThread())
    }

    override fun createPlaylist(playlistName: String, categoryType: String, categoryId: Long, filter: String): Observable<Long> {
        if (playlistName.isBlank()) {
            return Observable.just(0)
        }

        val message = SocketMessage.Builder
            .request(Messages.Request.SavePlaylist)
            .addOption(Messages.Key.PLAYLIST_NAME, playlistName)
            .addOption(Messages.Key.SUBQUERY, createTrackListSubquery(categoryType, categoryId, filter))
            .build()

        return service.observe(message, client)
            .flatMap<Long> { socketMessage -> extractId(socketMessage, Messages.Key.PLAYLIST_ID) }
            .observeOn(AndroidSchedulers.mainThread())
    }

    override fun createPlaylist(playlistName: String, tracks: List<ITrack>): Observable<Long> {
        if (playlistName.isBlank()) {
            return Observable.just(0)
        }

        val externalIds = JSONArray()
        tracks.forEach {
            if (it.externalId.isNotEmpty()) {
                externalIds.put(it.externalId)
            }
        }

        val message = SocketMessage.Builder
            .request(Messages.Request.SavePlaylist)
            .addOption(Messages.Key.PLAYLIST_NAME, playlistName)
            .addOption(Messages.Key.EXTERNAL_IDS, externalIds)
            .build()

        return service.observe(message, client)
            .flatMap<Long> { socketMessage -> extractId(socketMessage, Messages.Key.PLAYLIST_ID) }
            .observeOn(AndroidSchedulers.mainThread())
    }

    override fun createPlaylistWithExternalIds(playlistName: String, externalIds: List<String>): Observable<Long> {
        if (playlistName.isBlank()) {
            return Observable.just(-1L)
        }

        val jsonArray = JSONArray()
        externalIds.forEach { jsonArray.put(it) }

        val message = SocketMessage.Builder
            .request(Messages.Request.SavePlaylist)
            .addOption(Messages.Key.PLAYLIST_NAME, playlistName)
            .addOption(Messages.Key.EXTERNAL_IDS, jsonArray)
            .build()

        return service.observe(message, client)
            .flatMap<Long> { socketMessage -> extractId(socketMessage, Messages.Key.PLAYLIST_ID) }
            .observeOn(AndroidSchedulers.mainThread())
    }

    override fun overwritePlaylistWithExternalIds(playlistId: Long, externalIds: List<String>): Observable<Long> {
        if (playlistId < 0L) {
            return Observable.just(-1L)
        }

        val jsonArray = JSONArray()
        externalIds.forEach { jsonArray.put(it) }

        val message = SocketMessage.Builder
            .request(Messages.Request.SavePlaylist)
            .addOption(Messages.Key.PLAYLIST_ID, playlistId)
            .addOption(Messages.Key.EXTERNAL_IDS, jsonArray)
            .build()

        return service.observe(message, client)
            .flatMap<Long> { socketMessage -> extractId(socketMessage, Messages.Key.PLAYLIST_ID) }
            .observeOn(AndroidSchedulers.mainThread())
    }

    override fun appendToPlaylist(playlistId: Long, categoryType: String, categoryId: Long, filter: String, offset: Long): Observable<Boolean> {
        val message = SocketMessage.Builder
            .request(Messages.Request.AppendToPlaylist)
            .addOption(Messages.Key.PLAYLIST_ID, playlistId)
            .addOption(Messages.Key.OFFSET, offset)
            .addOption(Messages.Key.SUBQUERY, createTrackListSubquery(categoryType, categoryId, filter))
            .build()

        return service.observe(message, client)
            .flatMap<Boolean> { socketMessage -> isSuccessful(socketMessage) }
            .observeOn(AndroidSchedulers.mainThread())
    }

    override fun appendToPlaylist(playlistId: Long, tracks: List<ITrack>, offset: Long): Observable<Boolean> {
        val externalIds = JSONArray()
        tracks.forEach {
            if (it.externalId.isNotEmpty()) {
                externalIds.put(it.externalId)
            }
        }

        val message = SocketMessage.Builder
            .request(Messages.Request.AppendToPlaylist)
            .addOption(Messages.Key.PLAYLIST_ID, playlistId)
            .addOption(Messages.Key.EXTERNAL_IDS, externalIds)
            .addOption(Messages.Key.OFFSET, offset)
            .build()

        return service.observe(message, client)
            .flatMap<Boolean> { socketMessage -> isSuccessful(socketMessage) }
            .observeOn(AndroidSchedulers.mainThread())
    }

    override fun appendToPlaylist(playlistId: Long, categoryValue: ICategoryValue): Observable<Boolean> =
        appendToPlaylist(playlistId, categoryValue.type, categoryValue.id)

    override fun appendToPlaylistWithExternalIds(playlistId: Long, externalIds: List<String>, offset: Long): Observable<Boolean> {
        val jsonArray = JSONArray()
        externalIds.forEach { jsonArray.put(it) }

        val message = SocketMessage.Builder
            .request(Messages.Request.AppendToPlaylist)
            .addOption(Messages.Key.PLAYLIST_ID, playlistId)
            .addOption(Messages.Key.EXTERNAL_IDS, jsonArray)
            .addOption(Messages.Key.OFFSET, offset)
            .build()

        return service.observe(message, client)
            .flatMap<Boolean> { socketMessage -> isSuccessful(socketMessage) }
            .observeOn(AndroidSchedulers.mainThread())
    }

    override fun removeTracksFromPlaylist(playlistId: Long, externalIds: List<String>, sortOrders: List<Int>): Observable<Int> {
        val jsonIds = JSONArray()
        externalIds.forEach { jsonIds.put(it) }

        val jsonOrders = JSONArray()
        sortOrders.forEach { jsonOrders.put(it) }

        val message = SocketMessage.Builder
            .request(Messages.Request.RemoveTracksFromPlaylist)
            .addOption(Messages.Key.PLAYLIST_ID, playlistId)
            .addOption(Messages.Key.EXTERNAL_IDS, jsonIds)
            .addOption(Messages.Key.SORT_ORDERS, jsonOrders)
            .build()

        return service.observe(message, client)
            .flatMap<Int> { socketMessage -> toCount(socketMessage) }
            .observeOn(AndroidSchedulers.mainThread())
    }

    override fun renamePlaylist(playlistId: Long, newName: String): Observable<Boolean> {
        if (newName.isBlank()) {
            return Observable.just(false)
        }

        val message = SocketMessage.Builder
            .request(Messages.Request.RenamePlaylist)
            .addOption(Messages.Key.PLAYLIST_ID, playlistId)
            .addOption(Messages.Key.PLAYLIST_NAME, newName)
            .build()

        return service.observe(message, client)
            .flatMap<Boolean> { socketMessage -> isSuccessful(socketMessage) }
            .observeOn(AndroidSchedulers.mainThread())
    }

    override fun deletePlaylist(playlistId: Long): Observable<Boolean> {
        val message = SocketMessage.Builder
            .request(Messages.Request.DeletePlaylist)
            .addOption(Messages.Key.PLAYLIST_ID, playlistId)
            .build()

        return service.observe(message, client)
            .flatMap<Boolean> { socketMessage -> isSuccessful(socketMessage) }
            .observeOn(AndroidSchedulers.mainThread())
    }

    override fun listOutputDrivers(): Observable<IOutputs> {
        val message = SocketMessage.Builder
            .request(Messages.Request.ListOutputDrivers)
            .build()

        return service.observe(message, client)
            .flatMap<IOutputs> { socketMessage -> toOutputs(socketMessage) }
            .observeOn(AndroidSchedulers.mainThread())
    }

    override fun setDefaultOutputDriver(driverName: String, deviceId: String): Observable<Boolean> {
        val message = SocketMessage.Builder
            .request(Messages.Request.SetDefaultOutputDriver)
            .addOption(Messages.Key.DRIVER_NAME, driverName)
            .addOption(Messages.Key.DEVICE_ID, deviceId)
            .build()

        return service.observe(message, client)
            .flatMap<Boolean> { socketMessage -> isSuccessful(socketMessage) }
            .observeOn(AndroidSchedulers.mainThread())
    }

    override fun getGainSettings(): Observable<IGainSettings> {
        val message = SocketMessage.Builder
            .request(Messages.Request.GetGainSettings)
            .build()

        return service.observe(message, client)
            .flatMap<IGainSettings> { socketMessage ->
                Observable.just(RemoteGainSettings(socketMessage.getJsonObject()))
            }
            .observeOn(AndroidSchedulers.mainThread())
    }

    override fun updateGainSettings(replayGainMode: ReplayGainMode, preampGain: Float): Observable<Boolean> {
        val message = SocketMessage.Builder
            .request(Messages.Request.SetGainSettings)
            .addOption(Messages.Key.REPLAYGAIN_MODE, replayGainMode.rawValue)
            .addOption(Messages.Key.PREAMP_GAIN, preampGain)
            .build()

        return service.observe(message, client)
            .flatMap<Boolean> { socketMessage -> isSuccessful(socketMessage) }
            .observeOn(AndroidSchedulers.mainThread())
    }

    override fun getEqualizerSettings(): Observable<IEqualizerSettings> {
        val message = SocketMessage.Builder
            .request(Messages.Request.GetEqualizerSettings)
            .build()

        return service.observe(message, client)
            .flatMap<IEqualizerSettings> { socketMessage ->
                Observable.just(RemoteEqualizerSettings(socketMessage.getJsonObject()))
            }
            .observeOn(AndroidSchedulers.mainThread())
    }

    override fun updateEqualizerSettings(enabled: Boolean, freqs: Array<Double>): Observable<Boolean> {
        val message = SocketMessage.Builder
            .request(Messages.Request.SetEqualizerSettings)
            .addOption(Messages.Key.ENABLED, enabled)
            .addOption(Messages.Key.BANDS, JSONArray().apply {
                freqs.forEach { this.put(it) }
            })
            .build()

        return service.observe(message, client)
            .flatMap<Boolean> { socketMessage -> isSuccessful(socketMessage) }
            .observeOn(AndroidSchedulers.mainThread())
    }

    override fun reindexMetadata(): Observable<Boolean> {
        return runIndexer(Messages.Value.REINDEX)
    }

    override fun rebuildMetadata(): Observable<Boolean> {
        return runIndexer(Messages.Value.REBUILD)
    }

    override fun getTransportType(): Observable<TransportType> {
        val message = SocketMessage.Builder
            .request(Messages.Request.GetTransportType)
            .build()

        return service.observe(message, client)
            .flatMap<TransportType> { socketMessage ->
                Observable.just(TransportType.find(
                    socketMessage.getStringOption(Messages.Key.TYPE,
                    TransportType.Gapless.rawValue)))
            }
            .observeOn(AndroidSchedulers.mainThread())
    }

    override fun setTransportType(type: TransportType): Observable<Boolean> {
        val message = SocketMessage.Builder
            .request(Messages.Request.SetTransportType)
            .addOption(Messages.Key.TYPE, type.rawValue)
            .build()

        return service.observe(message, client)
            .flatMap<Boolean> { socketMessage -> isSuccessful(socketMessage) }
            .observeOn(AndroidSchedulers.mainThread())
    }

    override fun observeState(): Observable<Pair<IMetadataProxy.State, IMetadataProxy.State>> =
        connectionStatePublisher.observeOn(AndroidSchedulers.mainThread())

    override fun observePlayQueue(): Observable<Unit> =
        playQueueStatePublisher.observeOn(AndroidSchedulers.mainThread())

    override fun observeAuthFailure(): Observable<Unit> =
        authFailurePublisher.observeOn(AndroidSchedulers.mainThread())

    override fun attach() {
        service.addClient(client)
    }

    override fun detach() {
        service.cancelMessages(client)
        service.removeClient(client)
    }

    override fun destroy() {
        detach()
        disposables.dispose()
    }

    private fun runIndexer(type: String): Observable<Boolean> {
        val message = SocketMessage.Builder
            .request(Messages.Request.RunIndexer)
            .addOption(Messages.Key.TYPE, type)
            .build()

        return service.observe(message, client)
            .flatMap<Boolean> { socketMessage -> isSuccessful(socketMessage) }
            .observeOn(AndroidSchedulers.mainThread())
    }

    private val client: WebSocketService.Client = object : WebSocketService.Client {
        override fun onStateChanged(newState: WebSocketService.State, oldState: WebSocketService.State) {
            connectionStatePublisher.onNext(Pair(mapState(newState), mapState(oldState)))
        }

        override fun onMessageReceived(message: SocketMessage) {
            if (message.type == SocketMessage.Type.Broadcast) {
                if (Messages.Broadcast.PlayQueueChanged.matches(message.name)) {
                    playQueueStatePublisher.onNext(Unit)
                }
            }
        }

        override fun onInvalidPassword() {
            authFailurePublisher.onNext(Unit)
        }
    }

    private fun mapState(state: WebSocketService.State): IMetadataProxy.State =
        when (state) {
            WebSocketService.State.Disconnected -> IMetadataProxy.State.Disconnected
            WebSocketService.State.Connecting -> IMetadataProxy.State.Connecting
            WebSocketService.State.Connected -> IMetadataProxy.State.Connected
        }

    companion object {
        private fun createTrackListSubquery(categoryType: String, categoryId: Long, filter: String): JSONObject {
            val type = if (categoryType.isNotEmpty() && categoryId > 0)
                Messages.Request.QueryTracksByCategory else Messages.Request.QueryTracks

            val suboptions = JSONObject()

            if (type == Messages.Request.QueryTracksByCategory) {
                suboptions.put(Messages.Key.CATEGORY, categoryType)
                suboptions.put(Messages.Key.ID, categoryId)
            }

            if (filter.isNotBlank()) {
                suboptions.put(Messages.Key.FILTER, filter)
            }

            return JSONObject()
                .put(Messages.Key.TYPE, type.toString())
                .put(Messages.Key.OPTIONS, suboptions)
        }

        private val toAlbumArtist: (JSONObject, String) -> ICategoryValue = { json, _ -> RemoteAlbumArtist(json) }
        private val toPlaylist: (JSONObject, String) -> ICategoryValue = { json, _ -> RemotePlaylist(json) }
        private val toCategoryValue: (JSONObject, String) -> ICategoryValue = { json, type -> RemoteCategoryValue(type, json) }

        private fun toCategoryList(socketMessage: SocketMessage, type: String): Observable<List<ICategoryValue>> {
            val converter: (JSONObject, String) -> ICategoryValue = when (type) {
                Metadata.Category.ALBUM_ARTIST -> toAlbumArtist
                Metadata.Category.PLAYLISTS -> toPlaylist
                else -> toCategoryValue
            }
            val values = ArrayList<ICategoryValue>()
            val json = socketMessage.getJsonArrayOption(Messages.Key.DATA, JSONArray())!!
            for (i in 0 until json.length()) {
                values.add(converter(json.getJSONObject(i), type))
            }
            return Observable.just(values)
        }

        private fun toOutputs(socketMessage: SocketMessage): Observable<IOutputs> {
            val outputList = ArrayList<IOutput>()
            val allOutputs = socketMessage.getJsonArrayOption(Messages.Key.ALL, JSONArray())!!
            val selectedOutput = socketMessage.getJsonObjectOption(Messages.Key.SELECTED, JSONObject())!!

            for (i in 0 until allOutputs.length()) {
                outputList.add(RemoteOutput(allOutputs.getJSONObject(i)))
            }

            return Observable.just(RemoteOutputs(
                selectedOutput.optString(Messages.Key.DRIVER_NAME, ""),
                selectedOutput.optString(Messages.Key.DEVICE_ID, ""),
                outputList
            ))
        }

        private fun toTrackList(socketMessage: SocketMessage): Observable<List<ITrack>> {
            val tracks = ArrayList<ITrack>()
            val json = socketMessage.getJsonArrayOption(Messages.Key.DATA, JSONArray())!!
            for (i in 0 until json.length()) {
                tracks.add(RemoteTrack(json.getJSONObject(i)))
            }
            return Observable.just(tracks)
        }

        private fun toAlbumList(socketMessage: SocketMessage): Observable<List<IAlbum>> {
            val albums = ArrayList<IAlbum>()
            val json = socketMessage.getJsonArrayOption(Messages.Key.DATA, JSONArray())!!
            for (i in 0 until json.length()) {
                albums.add(RemoteAlbum(json.getJSONObject(i)))
            }
            return Observable.just(albums)
        }

        private fun toStringList(socketMessage: SocketMessage): Observable<List<String>> {
            val strings = ArrayList<String>()
            val json = socketMessage.getJsonArrayOption(Messages.Key.DATA, JSONArray())!!
            for (i in 0 until json.length()) {
                strings.add(json.getString(i))
            }
            return Observable.just(strings)
        }

        private fun toCount(message: SocketMessage): Observable<Int> {
            return Observable.just(message.getIntOption(Messages.Key.COUNT, 0))
        }

        private fun isSuccessful(message: SocketMessage): Observable<Boolean> {
            return Observable.just(message.getBooleanOption(Messages.Key.SUCCESS, false))
        }

        private fun extractId(message: SocketMessage, idKey: String): Observable<Long> {
            return Observable.just(message.getLongOption(idKey, 0))
        }
    }
}