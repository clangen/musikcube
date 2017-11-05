package io.casey.musikcube.remote.injection

import android.content.Context
import dagger.Module
import dagger.Provides
import io.casey.musikcube.remote.websocket.WebSocketService

@Module
class ServiceModule {
    @ApplicationScope
    @Provides
    fun providesWebSocketService(context: Context): WebSocketService {
        return WebSocketService(context)
    }
}
