package io.casey.musikcube.remote.injection

import android.content.Context
import androidx.room.Room
import dagger.Module
import dagger.Provides
import io.casey.musikcube.remote.service.gapless.db.GaplessDb
import io.casey.musikcube.remote.service.playback.impl.streaming.db.OfflineDb
import io.casey.musikcube.remote.service.websocket.WebSocketService
import io.casey.musikcube.remote.service.websocket.model.IMetadataProxy
import io.casey.musikcube.remote.service.websocket.model.impl.remote.RemoteMetadataProxy
import io.casey.musikcube.remote.ui.settings.model.ConnectionsDb

@Module
class DataModule {
    @Provides
    fun providesMetadataProxy(wss: WebSocketService): IMetadataProxy = RemoteMetadataProxy(wss)

    @ApplicationScope
    @Provides
    fun providesOfflineDb(context: Context): OfflineDb =
        Room.databaseBuilder(context,  OfflineDb::class.java, "offline").build()

    @ApplicationScope
    @Provides
    fun providesConnectionsDb(context: Context): ConnectionsDb =
        Room.databaseBuilder(context, ConnectionsDb::class.java, "connections").build()

    @ApplicationScope
    @Provides
    fun providesGaplessDb(context: Context): GaplessDb =
        Room.databaseBuilder(context, GaplessDb::class.java, "gapless").build()
}