package io.casey.musikcube.remote.injection

import dagger.Component
import io.casey.musikcube.remote.service.playback.impl.streaming.offline.OfflineDb

@DataScope
@Component(
    dependencies = arrayOf(AppComponent::class),
    modules = arrayOf())
interface DataComponent {
    fun inject(db: OfflineDb)
}