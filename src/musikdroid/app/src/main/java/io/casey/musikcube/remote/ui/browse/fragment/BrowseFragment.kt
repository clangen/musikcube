package io.casey.musikcube.remote.ui.browse.fragment

import android.os.Bundle
import android.support.design.widget.FloatingActionButton
import android.support.design.widget.TabLayout
import android.support.v4.view.ViewCompat
import android.support.v4.view.ViewPager
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.ui.browse.adapter.BrowseFragmentAdapter
import io.casey.musikcube.remote.ui.browse.constant.Browse
import io.casey.musikcube.remote.ui.shared.activity.IFabConsumer
import io.casey.musikcube.remote.ui.shared.activity.IFilterable
import io.casey.musikcube.remote.ui.shared.activity.ITitleProvider
import io.casey.musikcube.remote.ui.shared.activity.ITransportObserver
import io.casey.musikcube.remote.ui.shared.extension.initToolbarIfNecessary
import io.casey.musikcube.remote.ui.shared.fragment.BaseFragment

class BrowseFragment: BaseFragment(), ITransportObserver, IFilterable, ITitleProvider {
    private lateinit var adapter: BrowseFragmentAdapter

    override val title: String
        get() = getString(R.string.app_name)

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? =
        inflater.inflate(R.layout.browse_fragment, container, false).apply {
            adapter = BrowseFragmentAdapter(appCompatActivity, childFragmentManager)

            val pager = findViewById<ViewPager>(R.id.view_pager)
            pager.adapter = adapter
            pager.currentItem = adapter.indexOf(extras.getString(Browse.Extras.INITIAL_CATEGORY_TYPE))

            val tabs = findViewById<TabLayout>(R.id.tab_layout)
            tabs.setupWithViewPager(pager)

            val fab = findViewById<FloatingActionButton>(R.id.fab)
            fab.setOnClickListener {
                (adapter.fragmentAt(pager.currentItem) as? IFabConsumer)?.onFabPress(fab)
            }

            pager.addOnPageChangeListener(object: ViewPager.OnPageChangeListener {
                override fun onPageScrollStateChanged(state: Int) {
                }

                override fun onPageScrolled(pos: Int, offset: Float, offsetPixels: Int) {
                }

                override fun onPageSelected(pos: Int) {
                    adapter.fragmentAt(pos)?.let {
                        when (it is IFabConsumer) {
                            true -> {
                                when (it.fabVisible) {
                                    true -> fab.show()
                                    false -> fab.hide()
                                }
                            }
                            false -> fab.hide()
                        }
                    }
                }
            })

            initToolbarIfNecessary(appCompatActivity, this)
        }

    override fun onTransportChanged() =
        adapter.onTransportChanged()

    override fun setFilter(filter: String) {
        adapter.filter = filter
    }

    companion object {
        const val TAG = "BrowseFragment"

        fun create(extras: Bundle): BrowseFragment =
            BrowseFragment().apply { arguments = extras }
    }
}