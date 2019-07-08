package io.casey.musikcube.remote.service.websocket.model.impl.remote

import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.injection.DaggerDataComponent
import io.casey.musikcube.remote.service.websocket.model.IMetadataProxy
import io.casey.musikcube.remote.service.websocket.model.ITrack
import io.casey.musikcube.remote.service.websocket.model.ITrackListQueryFactory
import io.reactivex.Observable
import org.json.JSONObject
import javax.inject.Inject

class IdListTrackListQueryFactory(private val idList: List<String>): ITrackListQueryFactory {
    @Inject protected lateinit var metadataProxy: IMetadataProxy

    init {
        DaggerDataComponent.builder()
            .appComponent(Application.appComponent)
            .build().inject(this)

        metadataProxy.attach()
    }

    override fun page(offset: Int, limit: Int): Observable<List<ITrack>>? {
        val window = mutableSetOf<String>()
        val max = Math.min(limit, idList.size)

        for (i in 0 until max) {
            window.add(idList[offset + i])
        }

        val missing = RemoteTrack(JSONObject())
        return metadataProxy.getTracks(window)
            .flatMap{ it ->
                val result = mutableListOf<ITrack>()
                for (i in 0 until max) {
                    result.add(it[idList[offset + i]] ?: missing)
                }
                Observable.just(result)
            }
    }

    override fun count(): Observable<Int> = Observable.just(idList.size)
    override fun offline(): Boolean = false

    fun destroy() {
        metadataProxy.destroy()
    }
}