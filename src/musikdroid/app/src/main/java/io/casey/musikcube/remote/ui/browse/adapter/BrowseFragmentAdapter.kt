package io.casey.musikcube.remote.ui.browse.adapter

import android.content.Context
import android.support.v4.app.Fragment
import android.support.v4.app.FragmentManager
import android.support.v4.app.FragmentPagerAdapter
import android.view.ViewGroup
import io.casey.musikcube.remote.ui.albums.fragment.AlbumBrowseFragment
import io.casey.musikcube.remote.ui.category.constant.NavigationType
import io.casey.musikcube.remote.ui.category.fragment.CategoryBrowseFragment
import io.casey.musikcube.remote.ui.shared.activity.Filterable
import io.casey.musikcube.remote.ui.tracks.fragment.TrackListFragment

class BrowseFragmentAdapter(private val context: Context, fm: FragmentManager): FragmentPagerAdapter(fm) {
    private val fragments = mutableMapOf<Int, Fragment>()

    var filter = ""
        set(value) {
            field = value
            fragments.forEach {
                (it.value as? Filterable)?.setFilter(filter)
            }
        }

    override fun getItem(index: Int): Fragment =
        when (index) {
            0 -> AlbumBrowseFragment.create()
            1 -> CategoryBrowseFragment.create(
                CategoryBrowseFragment.arguments(context, "artist"))
            2 -> TrackListFragment.create()
            else -> CategoryBrowseFragment.create(
                CategoryBrowseFragment.arguments("playlists", NavigationType.Tracks))
        }

    override fun getPageTitle(position: Int): CharSequence? =
        when (position) {
            0 -> "albums"
            1 -> "artists"
            2 -> "songs"
            else -> "playlists"
        }

    override fun getCount(): Int {
        return 4
    }

    override fun instantiateItem(container: ViewGroup, position: Int): Any {
        val result = super.instantiateItem(container, position)
        fragments[position] = result as Fragment
        (result as? Filterable)?.setFilter(filter)
        return result
    }
}