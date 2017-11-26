package io.casey.musikcube.remote.injection

import android.content.Context
import dagger.Component
import io.casey.musikcube.remote.service.playback.impl.streaming.db.OfflineDb

@DataScope
@Component(
    dependencies = arrayOf(AppComponent::class),
    modules = arrayOf(DataModule::class))
interface DataComponent {
    fun inject(db: OfflineDb)

    fun context(): Context /* via AppComponent */
}