package io.casey.musikcube.remote.ui.category.activity

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.view.Menu
import io.casey.musikcube.remote.ui.category.constant.Category
import io.casey.musikcube.remote.ui.category.constant.NavigationType
import io.casey.musikcube.remote.ui.category.fragment.CategoryBrowseFragment
import io.casey.musikcube.remote.ui.shared.activity.FragmentActivityWithTransport
import io.casey.musikcube.remote.ui.shared.fragment.BaseFragment

class CategoryBrowseActivity: FragmentActivityWithTransport() {
    private val category
        get() = content as CategoryBrowseFragment

    override fun onCreateOptionsMenu(menu: Menu): Boolean = category.createOptionsMenu(menu)
    override val contentFragmentTag: String = CategoryBrowseFragment.TAG

    override fun createContentFragment(): BaseFragment =
        (intent.extras ?: Bundle()).run {
            CategoryBrowseFragment.create(intent)
        }

    companion object {
        fun getStartIntent(context: Context,
                           category: String,
                           predicateType: String = "",
                           predicateId: Long = -1,
                           predicateValue: String = ""): Intent =
            Intent(context, CategoryBrowseActivity::class.java).apply {
                putExtra(
                    Category.Extra.FRAGMENT_ARGUMENTS,
                    CategoryBrowseFragment.arguments(
                        context,
                        category,
                        predicateType,
                        predicateId,
                        predicateValue))
            }

        fun getStartIntent(context: Context,
                           category: String,
                           navigationType: NavigationType,
                           title: String = ""): Intent =
            Intent(context, CategoryBrowseActivity::class.java).apply {
                putExtra(
                    Category.Extra.FRAGMENT_ARGUMENTS,
                    CategoryBrowseFragment.arguments(category, navigationType, title))
            }
    }
}
