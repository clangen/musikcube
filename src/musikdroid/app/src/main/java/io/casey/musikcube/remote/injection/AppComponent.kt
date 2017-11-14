package io.casey.musikcube.remote.injection

import dagger.Component
import io.casey.musikcube.remote.service.websocket.WebSocketService

@ApplicationScope
@Component(modules = arrayOf(AppModule::class, ServiceModule::class))
interface AppComponent {
    fun webSocketService(): WebSocketService
}
