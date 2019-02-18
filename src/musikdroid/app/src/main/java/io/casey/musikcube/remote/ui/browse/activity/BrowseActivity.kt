package io.casey.musikcube.remote.ui.browse.activity

import android.content.Context
import android.content.Intent
import android.os.Bundle
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.ui.browse.constant.Browse
import io.casey.musikcube.remote.ui.browse.fragment.BrowseFragment
import io.casey.musikcube.remote.ui.shared.activity.BaseActivity
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

    companion object {
        fun getStartIntent(context: Context,
                           initialCategoryType: String = ""): Intent =
            Intent(context, BrowseActivity::class.java).apply {
                putExtra(Browse.Extras.INITIAL_CATEGORY_TYPE, initialCategoryType)
            }
    }
}