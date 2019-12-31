package io.casey.musikcube.remote.injection

import android.content.Context
import dagger.Component
import io.casey.musikcube.remote.service.playback.impl.streaming.db.OfflineDb
import io.casey.musikcube.remote.service.websocket.model.impl.remote.IdListTrackListQueryFactory

@DataScope
@Component(dependencies = [AppComponent::class])
interface DataComponent {
    fun inject(db: OfflineDb)
    fun inject(slidingWindow: IdListTrackListQueryFactory)
    fun context(): Context /* via AppComponent */
}