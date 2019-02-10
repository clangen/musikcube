package io.casey.musikcube.remote.ui.browse.adapter

import android.content.Context
import android.support.v4.app.Fragment
import android.support.v4.app.FragmentManager
import android.support.v4.app.FragmentPagerAdapter
import io.casey.musikcube.remote.ui.category.fragment.CategoryBrowseFragment

class BrowseFragmentAdapter(private val context: Context, fm: FragmentManager): FragmentPagerAdapter(fm) {
    override fun getItem(index: Int): Fragment {
        return CategoryBrowseFragment.create(
            CategoryBrowseFragment.arguments(context, "artist"))
    }

    override fun getPageTitle(position: Int): CharSequence? {
        return "artist"
    }

    override fun getCount(): Int {
        return 4
    }
}