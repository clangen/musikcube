package io.casey.musikcube.remote.injection

import dagger.Component
import io.casey.musikcube.remote.service.playback.PlayerWrapper

@PlaybackScope
@Component(dependencies = arrayOf(AppComponent::class))
interface PlaybackComponent {
    fun inject(playerWrapper: PlayerWrapper)
}