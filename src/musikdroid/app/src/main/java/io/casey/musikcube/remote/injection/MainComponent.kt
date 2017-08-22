package io.casey.musikcube.remote.injection

import dagger.Component
import io.casey.musikcube.remote.MainActivity
import io.casey.musikcube.remote.db.offline.OfflineDb
import io.casey.musikcube.remote.playback.RemotePlaybackService
import io.casey.musikcube.remote.playback.StreamingPlaybackService
import io.casey.musikcube.remote.ui.activity.*
import io.casey.musikcube.remote.ui.view.EmptyListView
import io.casey.musikcube.remote.ui.view.MainMetadataView
import javax.inject.Singleton

@Singleton
@Component(modules = arrayOf(MainModule::class))
interface MainComponent {
    /* activities */
    fun inject(activity: ConnectionsActivity)
    fun inject(activity: MainActivity)
    fun inject(activity: WebSocketActivityBase)
    fun inject(activity: SettingsActivity)
    fun inject(activity: AlbumBrowseActivity)
    fun inject(activity: CategoryBrowseActivity)
    fun inject(activity: PlayQueueActivity)
    fun inject(activity: TrackListActivity)

    /* views */
    fun inject(view: EmptyListView)
    fun inject(view: MainMetadataView)

    /* services */
    fun inject(service: StreamingPlaybackService)
    fun inject(service: RemotePlaybackService)

    /* data */
    fun inject(db: OfflineDb)
}
