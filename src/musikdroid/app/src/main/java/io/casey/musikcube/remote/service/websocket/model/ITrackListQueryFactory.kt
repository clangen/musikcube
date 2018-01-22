package io.casey.musikcube.remote.service.websocket.model

import io.reactivex.Observable

interface ITrackListQueryFactory {
    fun count(): Observable<Int>?
    fun page(offset: Int, limit: Int): Observable<List<ITrack>>?
    fun offline(): Boolean
}