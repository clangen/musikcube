package io.casey.musikcube.remote.injection

import dagger.Module
import dagger.Provides
import io.casey.musikcube.remote.data.IDataProvider
import io.casey.musikcube.remote.data.impl.remote.RemoteDataProvider
import io.casey.musikcube.remote.websocket.WebSocketService

@Module
class DataModule {
    @Provides
    fun providesDataProvider(wss: WebSocketService): IDataProvider {
        return RemoteDataProvider(wss)
    }
}