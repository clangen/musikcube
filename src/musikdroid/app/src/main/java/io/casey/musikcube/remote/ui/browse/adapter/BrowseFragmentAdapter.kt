package io.casey.musikcube.remote.ui.browse.adapter

import android.content.Context
import android.support.v4.app.Fragment
import android.support.v4.app.FragmentManager
import android.support.v4.app.FragmentPagerAdapter
import io.casey.musikcube.remote.ui.albums.fragment.AlbumBrowseFragment
import io.casey.musikcube.remote.ui.category.constant.NavigationType
import io.casey.musikcube.remote.ui.category.fragment.CategoryBrowseFragment

class BrowseFragmentAdapter(private val context: Context, fm: FragmentManager): FragmentPagerAdapter(fm) {
    override fun getItem(index: Int): Fragment {
        if (index == 0) {
            return AlbumBrowseFragment.create()
        }
        else if (index == 1) {
            return CategoryBrowseFragment.create(
                CategoryBrowseFragment.arguments(context, "artist"))
        }
        return CategoryBrowseFragment.create(
            CategoryBrowseFragment.arguments("playlists", NavigationType.Tracks))
    }

    override fun getPageTitle(position: Int): CharSequence? =
        when (position) {
            0 -> "albums"
            1 -> "artists"
            else -> "playlists"
        }

    override fun getCount(): Int {
        return 3
    }
}