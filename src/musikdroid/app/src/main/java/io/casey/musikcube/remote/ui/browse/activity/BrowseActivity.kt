package io.casey.musikcube.remote.ui.browse.activity

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.support.v4.app.Fragment
import android.support.v4.app.FragmentManager
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.ui.browse.constant.Browse
import io.casey.musikcube.remote.ui.browse.fragment.BrowseFragment
import io.casey.musikcube.remote.ui.shared.activity.BaseActivity
import io.casey.musikcube.remote.ui.shared.activity.IBackHandler
import io.casey.musikcube.remote.ui.shared.activity.ITransportObserver
import io.casey.musikcube.remote.ui.shared.extension.enableUpNavigation
import io.casey.musikcube.remote.ui.shared.extension.findFragment
import io.casey.musikcube.remote.ui.shared.extension.pushTo
import io.casey.musikcube.remote.ui.shared.fragment.TransportFragment

class BrowseActivity: BaseActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(R.layout.browse_activity)

        if (savedInstanceState == null) {
            createFragments()
        }

        findFragment<TransportFragment>(TransportFragment.TAG).modelChangedListener = {
            supportFragmentManager.fragments.forEach {
                if (it is ITransportObserver) {
                    it.onTransportChanged()
                }
            }
        }

        enableUpNavigation()
    }

    override fun onBackPressed() {
        (top as? IBackHandler)?.let {
            if (it.onBackPressed()) {
                return
            }
        }

        when {
            fm.backStackEntryCount > 1 -> fm.popBackStack()
            else -> super.onBackPressed()
        }
    }

    private fun createFragments() {
        supportFragmentManager
            .beginTransaction()
            .add(
                R.id.content_container,
                BrowseFragment.create(extras),
                BrowseFragment.TAG)
            .add(
                R.id.transport_container,
                TransportFragment
                    .create()
                    .pushTo(R.id.content_container),
                TransportFragment.TAG)
            .commit()

        supportFragmentManager.executePendingTransactions()
    }

    private val top: Fragment?
        get() {
            return when {
                fm.backStackEntryCount == 0 ->
                    fm.findFragmentByTag(BrowseFragment.TAG)
                else -> fm.findFragmentByTag(
                    fm.getBackStackEntryAt(fm.backStackEntryCount - 1).name)

            }
        }

        private val fm: FragmentManager
            get() = supportFragmentManager

        companion object {
            fun getStartIntent(context: Context,
                               initialCategoryType: String = ""): Intent =
                Intent(context, BrowseActivity::class.java).apply {
                    putExtra(Browse.Extras.INITIAL_CATEGORY_TYPE, initialCategoryType)
                }
        }
}