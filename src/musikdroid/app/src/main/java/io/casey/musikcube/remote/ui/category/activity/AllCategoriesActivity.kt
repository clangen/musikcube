package io.casey.musikcube.remote.ui.category.activity

import android.content.Context
import android.content.Intent
import io.casey.musikcube.remote.ui.category.fragment.AllCategoriesFragment
import io.casey.musikcube.remote.ui.shared.activity.FragmentActivityWithTransport
import io.casey.musikcube.remote.ui.shared.extension.withToolbar
import io.casey.musikcube.remote.ui.shared.fragment.BaseFragment

class AllCategoriesActivity: FragmentActivityWithTransport() {
    override fun createContentFragment(): BaseFragment =
        AllCategoriesFragment().withToolbar()

    override val contentFragmentTag: String =
        AllCategoriesFragment.TAG

    companion object {
        fun getStartIntent(context: Context): Intent {
            return Intent(context, AllCategoriesActivity::class.java)
        }
    }
}