package io.casey.musikcube.remote.injection

import dagger.Component
import io.casey.musikcube.remote.MainActivity
import io.casey.musikcube.remote.ui.activity.*
import io.casey.musikcube.remote.ui.view.EmptyListView
import io.casey.musikcube.remote.ui.view.MainMetadataView

@ViewScope
@Component(
    dependencies = arrayOf(AppComponent::class),
    modules = arrayOf(DataModule::class))
interface ViewComponent {
    fun inject(activity: ConnectionsActivity)
    fun inject(activity: MainActivity)
    fun inject(activity: BaseActivity)
    fun inject(activity: SettingsActivity)
    fun inject(activity: AlbumBrowseActivity)
    fun inject(activity: CategoryBrowseActivity)
    fun inject(activity: PlayQueueActivity)
    fun inject(activity: TrackListActivity)
    fun inject(view: EmptyListView)
    fun inject(view: MainMetadataView)
}

