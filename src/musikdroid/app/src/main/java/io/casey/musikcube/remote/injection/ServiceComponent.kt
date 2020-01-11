package io.casey.musikcube.remote.injection

import dagger.Component
import io.casey.musikcube.remote.service.gapless.GaplessHeaderService
import io.casey.musikcube.remote.service.playback.impl.remote.RemotePlaybackService
import io.casey.musikcube.remote.service.playback.impl.streaming.StreamProxy
import io.casey.musikcube.remote.service.playback.impl.streaming.StreamingPlaybackService

@ServiceScope
@Component(dependencies = [AppComponent::class])
interface ServiceComponent {
    fun inject(service: StreamingPlaybackService)
    fun inject(service: RemotePlaybackService)
    fun inject(service: GaplessHeaderService)
    fun inject(service: StreamProxy)
}