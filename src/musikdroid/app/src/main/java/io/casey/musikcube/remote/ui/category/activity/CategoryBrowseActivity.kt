package io.casey.musikcube.remote.ui.category.activity

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.view.Menu
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.ui.category.constant.Category
import io.casey.musikcube.remote.ui.category.constant.NavigationType
import io.casey.musikcube.remote.ui.category.fragment.CategoryBrowseFragment
import io.casey.musikcube.remote.ui.shared.activity.BaseActivity
import io.casey.musikcube.remote.ui.shared.activity.Filterable
import io.casey.musikcube.remote.ui.shared.extension.findFragment
import io.casey.musikcube.remote.ui.shared.fragment.TransportFragment
import io.casey.musikcube.remote.service.websocket.WebSocketService.State as SocketState

class CategoryBrowseActivity : BaseActivity(), Filterable {
    private lateinit var content: CategoryBrowseFragment
    private lateinit var transport: TransportFragment

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(R.layout.fragment_with_transport_activity)

        when (savedInstanceState == null) {
            true -> createFragments()
            else -> restoreFragments()
        }

        transport.modelChangedListener = {
            content.notifyTransportChanged()
        }
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean = content.createOptionsMenu(menu)
    override fun setFilter(filter: String) = content.setFilter(filter)

    private fun createFragments() {
        content = CategoryBrowseFragment.create(intent)
        transport = TransportFragment.create()
        supportFragmentManager
            .beginTransaction()
            .add(R.id.content_container, content, CategoryBrowseFragment.TAG)
            .add(R.id.transport_container, transport, TransportFragment.TAG)
            .commit()
    }

    private fun restoreFragments() {
        transport = findFragment(TransportFragment.TAG)
        content = findFragment(CategoryBrowseFragment.TAG)
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
