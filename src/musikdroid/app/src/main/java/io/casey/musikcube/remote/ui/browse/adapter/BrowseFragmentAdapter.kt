package io.casey.musikcube.remote.ui.browse.adapter

import android.content.Context
import android.support.v4.app.Fragment
import android.support.v4.app.FragmentManager
import android.support.v4.app.FragmentPagerAdapter
import android.view.ViewGroup
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.ui.albums.fragment.AlbumBrowseFragment
import io.casey.musikcube.remote.ui.category.constant.NavigationType
import io.casey.musikcube.remote.ui.category.fragment.CategoryBrowseFragment
import io.casey.musikcube.remote.ui.shared.activity.IFilterable
import io.casey.musikcube.remote.ui.tracks.fragment.TrackListFragment
import io.casey.musikcube.remote.service.playback.impl.remote.Metadata
import io.casey.musikcube.remote.ui.shared.activity.ITransportObserver

class BrowseFragmentAdapter(private val context: Context, fm: FragmentManager): FragmentPagerAdapter(fm) {
    private val fragments = mutableMapOf<Int, Fragment>()

    var filter = ""
        set(value) {
            field = value
            fragments.forEach {
                (it.value as? IFilterable)?.setFilter(filter)
            }
        }

    fun onTransportChanged() =
        fragments.forEach {
            (it.value as? ITransportObserver)?.onTransportChanged()
        }

    override fun getItem(index: Int): Fragment =
        when (index) {
            0 -> AlbumBrowseFragment.create()
            1 -> CategoryBrowseFragment.create(
                CategoryBrowseFragment.arguments(context, Metadata.Category.ALBUM_ARTIST))
            2 -> TrackListFragment.create()
            else -> CategoryBrowseFragment.create(
                CategoryBrowseFragment.arguments(Metadata.Category.PLAYLISTS, NavigationType.Tracks))
        }

    override fun getPageTitle(position: Int): CharSequence? =
        context.getString(when (position) {
            0 -> R.string.button_albums
            1 -> R.string.button_artists
            2 -> R.string.button_tracks
            else -> R.string.button_playlists
        })

    override fun getCount(): Int {
        return 4
    }

    override fun instantiateItem(container: ViewGroup, position: Int): Any {
        val result = super.instantiateItem(container, position)
        fragments[position] = result as Fragment
        (result as? IFilterable)?.setFilter(filter)
        return result
    }
}