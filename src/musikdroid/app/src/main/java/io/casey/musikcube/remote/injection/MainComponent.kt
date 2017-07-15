package io.casey.musikcube.remote.injection

import dagger.Component
import dagger.android.AndroidInjectionModule
import dagger.android.AndroidInjector
import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.offline.OfflineDb
import io.casey.musikcube.remote.playback.RemotePlaybackService
import io.casey.musikcube.remote.playback.StreamingPlaybackService
import io.casey.musikcube.remote.ui.view.EmptyListView
import io.casey.musikcube.remote.ui.view.MainMetadataView
import javax.inject.Singleton

@Singleton
@Component(modules = arrayOf(AndroidInjectionModule::class, MainModule::class, ActivityModule::class))
interface MainComponent : AndroidInjector<Application> {
    /* views */
    fun inject(view: EmptyListView)
    fun inject(view: MainMetadataView)

    /* services */
    fun inject(service: StreamingPlaybackService)
    fun inject(service: RemotePlaybackService)

    /* data */
    fun inject(db: OfflineDb)
}
