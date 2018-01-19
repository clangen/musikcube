package io.casey.musikcube.remote.injection

import dagger.Component
import io.casey.musikcube.remote.ui.albums.activity.AlbumBrowseActivity
import io.casey.musikcube.remote.ui.category.activity.AllCategoriesActivity
import io.casey.musikcube.remote.ui.category.activity.CategoryBrowseActivity
import io.casey.musikcube.remote.ui.home.activity.MainActivity
import io.casey.musikcube.remote.ui.home.view.MainMetadataView
import io.casey.musikcube.remote.ui.playqueue.activity.PlayQueueActivity
import io.casey.musikcube.remote.ui.settings.activity.ConnectionsActivity
import io.casey.musikcube.remote.ui.settings.activity.RemoteSettingsActivity
import io.casey.musikcube.remote.ui.settings.activity.SettingsActivity
import io.casey.musikcube.remote.ui.shared.activity.BaseActivity
import io.casey.musikcube.remote.ui.shared.mixin.DataProviderMixin
import io.casey.musikcube.remote.ui.shared.mixin.ItemContextMenuMixin
import io.casey.musikcube.remote.ui.shared.view.EmptyListView
import io.casey.musikcube.remote.ui.tracks.activity.TrackListActivity

@ViewScope
@Component(dependencies = arrayOf(AppComponent::class))
interface ViewComponent {
    fun inject(activity: AlbumBrowseActivity)
    fun inject(activity: AllCategoriesActivity)
    fun inject(activity: BaseActivity)
    fun inject(activity: CategoryBrowseActivity)
    fun inject(activity: ConnectionsActivity)
    fun inject(activity: MainActivity)
    fun inject(activity: PlayQueueActivity)
    fun inject(activity: RemoteSettingsActivity)
    fun inject(activity: SettingsActivity)
    fun inject(activity: TrackListActivity)

    fun inject(view: EmptyListView)
    fun inject(view: MainMetadataView)

    fun inject(mixin: DataProviderMixin)
    fun inject(mixin: ItemContextMenuMixin)
}

