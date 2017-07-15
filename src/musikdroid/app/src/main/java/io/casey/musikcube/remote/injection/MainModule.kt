package io.casey.musikcube.remote.injection

import android.content.Context
import dagger.Module
import dagger.Provides
import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.websocket.WebSocketService
import javax.inject.Singleton

@Module
class MainModule {
    @Provides
    fun providesContext(): Context {
        return Application.instance!!
    }

    @Provides
    @Singleton
    fun providesWebSocketService(context: Context): WebSocketService {
        return WebSocketService(context)
    }
}
