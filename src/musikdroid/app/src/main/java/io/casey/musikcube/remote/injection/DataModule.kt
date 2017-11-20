package io.casey.musikcube.remote.injection

import dagger.Module
import dagger.Provides
import io.casey.musikcube.remote.service.websocket.WebSocketService
import io.casey.musikcube.remote.service.websocket.model.IDataProvider
import io.casey.musikcube.remote.service.websocket.model.impl.remote.RemoteDataProvider

@Module
class DataModule {
    @Provides
    fun providesDataProvider(wss: WebSocketService): IDataProvider = RemoteDataProvider(wss)
}