package io.casey.musikcube.remote.ui.browse.adapter

import android.content.Context
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentManager
import androidx.fragment.app.FragmentPagerAdapter
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.service.playback.impl.remote.Metadata
import io.casey.musikcube.remote.ui.albums.fragment.AlbumBrowseFragment
import io.casey.musikcube.remote.ui.category.constant.NavigationType
import io.casey.musikcube.remote.ui.category.fragment.CategoryBrowseFragment
import io.casey.musikcube.remote.ui.shared.activity.IFilterable
import io.casey.musikcube.remote.ui.shared.activity.ITransportObserver
import io.casey.musikcube.remote.ui.shared.extension.pushTo
import io.casey.musikcube.remote.ui.shared.fragment.BaseFragment
import io.casey.musikcube.remote.ui.shared.mixin.PlaybackMixin
import io.casey.musikcube.remote.ui.tracks.fragment.TrackListFragment

class BrowseFragmentAdapter(private val context: Context,
                            private val playback: PlaybackMixin,
                            fm: FragmentManager,
                            private val containerId: Int = -1)
    : FragmentPagerAdapter(fm, BEHAVIOR_RESUME_ONLY_CURRENT_FRAGMENT)
{
    private val fragments = mutableMapOf<Int, Fragment>()

    var filter = ""
        set(value) {
            field = value
            fragments.forEach {
                (it.value as? IFilterable)?.setFilter(filter)
            }
        }

    var onFragmentInstantiated: ((Int) -> Unit?)? = null

    fun onTransportChanged() =
        fragments.forEach {
            (it.value as? ITransportObserver)?.onTransportChanged()
        }

    fun fragmentAt(pos: Int): Fragment? = fragments[pos]

    fun indexOf(category: String?): Int =
        when (category) {
            Metadata.Category.ALBUM_ARTIST -> 0
            Metadata.Category.ALBUM -> 1
            Metadata.Category.TRACKS -> 2
            Metadata.Category.PLAYLISTS -> 3
            Metadata.Category.OFFLINE -> 4
            else -> 0
        }

    override fun getItem(index: Int): Fragment {
        val fragment: BaseFragment = when (index) {
            0 -> CategoryBrowseFragment.create(
                CategoryBrowseFragment.arguments(context, Metadata.Category.ALBUM_ARTIST))
            1 -> AlbumBrowseFragment.create(context)
            2 -> TrackListFragment.create()
            3 -> CategoryBrowseFragment.create(
                CategoryBrowseFragment.arguments(Metadata.Category.PLAYLISTS, NavigationType.Tracks))
            else -> TrackListFragment.create(
                TrackListFragment.arguments(context, Metadata.Category.OFFLINE))
        }
        return fragment.pushTo(this.containerId)
    }

    override fun getPageTitle(position: Int): CharSequence? =
        context.getString(when (position) {
            0 -> R.string.button_artists
            1 -> R.string.button_albums
            2 -> R.string.button_tracks
            3 -> R.string.button_playlists
            else -> R.string.button_offline
        })

    override fun getCount(): Int {
        return if (playback.streaming) 5 else 4 /* hide "offline" for remote playback */
    }

    override fun instantiateItem(container: ViewGroup, position: Int): Any {
        val result = super.instantiateItem(container, position)
        fragments[position] = result as Fragment
        (result as? IFilterable)?.setFilter(filter)
        onFragmentInstantiated?.invoke(position)
        return result
    }
}