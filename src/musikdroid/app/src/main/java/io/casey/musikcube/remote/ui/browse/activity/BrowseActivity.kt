package io.casey.musikcube.remote.ui.browse.activity

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.support.design.widget.TabLayout
import android.support.v4.view.ViewPager
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.ui.browse.adapter.BrowseFragmentAdapter
import io.casey.musikcube.remote.ui.shared.activity.BaseActivity
import io.casey.musikcube.remote.ui.shared.extension.findFragment
import io.casey.musikcube.remote.ui.shared.fragment.TransportFragment

class BrowseActivity: BaseActivity() {
    private lateinit var transport: TransportFragment
    private lateinit var pager: ViewPager
    private lateinit var tabs: TabLayout
    private lateinit var adapter: BrowseFragmentAdapter

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(R.layout.browse_activity)

        adapter = BrowseFragmentAdapter(this, supportFragmentManager)
        pager = findViewById(R.id.view_pager)
        tabs = findViewById(R.id.tab_layout)
        tabs.setupWithViewPager(pager)
        pager.adapter = adapter

        when (savedInstanceState == null) {
            true -> createFragments()
            else -> restoreFragments()
        }

        transport.modelChangedListener = {
//            content.notifyTransportChanged()
        }
    }

//    override fun onCreateOptionsMenu(menu: Menu): Boolean = content.createOptionsMenu(menu)
//    override fun setFilter(filter: String) = content.setFilter(filter)

    private fun createFragments() {
//        content = CategoryBrowseFragment.create(intent)
        transport = TransportFragment.create()
        supportFragmentManager
            .beginTransaction()
//          .add(R.id.content_container, content, CategoryBrowseFragment.TAG)
            .add(R.id.transport_container, transport, TransportFragment.TAG)
            .commit()
    }

    private fun restoreFragments() {
        transport = findFragment(TransportFragment.TAG)
//        content = findFragment(CategoryBrowseFragment.TAG)
    }

    companion object {
        private const val EXTRA_INITIAL_CATEGORY_TYPE = "extra_initial_category_type"

        fun getStartIntent(context: Context,
                           initialCategoryType: String = ""): Intent =
            Intent(context, BrowseActivity::class.java).apply {
                putExtra(EXTRA_INITIAL_CATEGORY_TYPE, initialCategoryType)
            }
    }
}