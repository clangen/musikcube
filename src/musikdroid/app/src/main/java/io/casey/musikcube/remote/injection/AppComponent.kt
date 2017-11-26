package io.casey.musikcube.remote.injection

import android.content.Context
import dagger.Component
import io.casey.musikcube.remote.service.gapless.GaplessHeaderService
import io.casey.musikcube.remote.service.gapless.db.GaplessDb
import io.casey.musikcube.remote.service.playback.impl.streaming.StreamProxy
import io.casey.musikcube.remote.service.playback.impl.streaming.db.OfflineDb
import io.casey.musikcube.remote.service.websocket.WebSocketService
import io.casey.musikcube.remote.service.websocket.model.IDataProvider
import io.casey.musikcube.remote.ui.settings.model.ConnectionsDb

@ApplicationScope
@Component(modules = arrayOf(AppModule::class, DataModule::class, ServiceModule::class))
interface AppComponent {
    fun webSocketService(): WebSocketService /* via ServiceModule */
    fun gaplessHeaderService(): GaplessHeaderService /* via ServiceModule */
    fun streamProxy(): StreamProxy /* via ServiceModule */
    fun offlineDb(): OfflineDb /* via DataModule */
    fun gaplessDb(): GaplessDb /* via DataModule */
    fun connectionsDb(): ConnectionsDb /* via DataModule */
    fun dataProvider(): IDataProvider /* via DataModule */
    fun context(): Context /* via AppModule */
}
