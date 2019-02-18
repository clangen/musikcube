package io.casey.musikcube.remote.injection

import dagger.Component
import io.casey.musikcube.remote.ui.albums.activity.AlbumBrowseActivity
import io.casey.musikcube.remote.ui.albums.fragment.AlbumBrowseFragment
import io.casey.musikcube.remote.ui.browse.activity.BrowseActivity
import io.casey.musikcube.remote.ui.category.activity.AllCategoriesActivity
import io.casey.musikcube.remote.ui.category.activity.CategoryBrowseActivity
import io.casey.musikcube.remote.ui.category.fragment.AllCategoriesFragment
import io.casey.musikcube.remote.ui.category.fragment.CategoryBrowseFragment
import io.casey.musikcube.remote.ui.home.activity.MainActivity
import io.casey.musikcube.remote.ui.home.view.MainMetadataView
import io.casey.musikcube.remote.ui.playqueue.activity.PlayQueueActivity
import io.casey.musikcube.remote.ui.playqueue.fragment.PlayQueueFragment
import io.casey.musikcube.remote.ui.settings.activity.ConnectionsActivity
import io.casey.musikcube.remote.ui.settings.activity.RemoteEqActivity
import io.casey.musikcube.remote.ui.settings.activity.RemoteSettingsActivity
import io.casey.musikcube.remote.ui.settings.activity.SettingsActivity
import io.casey.musikcube.remote.ui.shared.activity.BaseActivity
import io.casey.musikcube.remote.ui.shared.fragment.BaseFragment
import io.casey.musikcube.remote.ui.shared.mixin.DataProviderMixin
import io.casey.musikcube.remote.ui.shared.mixin.ItemContextMenuMixin
import io.casey.musikcube.remote.ui.shared.view.EmptyListView
import io.casey.musikcube.remote.ui.tracks.activity.TrackListActivity
import io.casey.musikcube.remote.ui.tracks.fragment.TrackListFragment

@ViewScope
@Component(dependencies = arrayOf(AppComponent::class))
interface ViewComponent {
    fun inject(activity: AlbumBrowseActivity)
    fun inject(activity: AllCategoriesActivity)
    fun inject(activity: BaseActivity)
    fun inject(activity: BrowseActivity)
    fun inject(activity: CategoryBrowseActivity)
    fun inject(activity: ConnectionsActivity)
    fun inject(activity: MainActivity)
    fun inject(activity: PlayQueueActivity)
    fun inject(activity: RemoteEqActivity)
    fun inject(activity: RemoteSettingsActivity)
    fun inject(activity: SettingsActivity)
    fun inject(activity: TrackListActivity)

    fun inject(fragment: AlbumBrowseFragment)
    fun inject(fragment: AllCategoriesFragment)
    fun inject(fragment: BaseFragment)
    fun inject(fragment: CategoryBrowseFragment)
    fun inject(fragment: PlayQueueFragment)
    fun inject(fragment: TrackListFragment)

    fun inject(view: EmptyListView)
    fun inject(view: MainMetadataView)

    fun inject(mixin: DataProviderMixin)
    fun inject(mixin: ItemContextMenuMixin)
}

