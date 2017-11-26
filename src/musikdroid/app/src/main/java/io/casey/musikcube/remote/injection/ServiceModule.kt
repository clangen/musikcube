package io.casey.musikcube.remote.injection

import android.content.Context
import dagger.Module
import dagger.Provides
import io.casey.musikcube.remote.service.gapless.GaplessHeaderService
import io.casey.musikcube.remote.service.playback.impl.streaming.StreamProxy
import io.casey.musikcube.remote.service.websocket.WebSocketService

@Module
class ServiceModule {
    @ApplicationScope
    @Provides
    fun providesWebSocketService(context: Context): WebSocketService {
        return WebSocketService(context)
    }

    @ApplicationScope
    @Provides
    fun providesGaplessHeaderService(): GaplessHeaderService {
        return GaplessHeaderService()
    }

    @ApplicationScope
    @Provides
    fun providesStreamProxy(context: Context): StreamProxy {
        return StreamProxy(context)
    }
}
