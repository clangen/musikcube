package io.casey.musikcube.remote.ui.category.activity

import android.content.Context
import android.content.Intent
import android.os.Bundle
import com.simplecityapps.recyclerview_fastscroll.views.FastScrollRecyclerView
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.service.websocket.model.IDataProvider
import io.casey.musikcube.remote.ui.category.adapter.AllCategoriesAdapter
import io.casey.musikcube.remote.ui.shared.activity.BaseActivity
import io.casey.musikcube.remote.ui.shared.extension.addTransportFragment
import io.casey.musikcube.remote.ui.shared.extension.enableUpNavigation
import io.casey.musikcube.remote.ui.shared.extension.setupDefaultRecyclerView
import io.casey.musikcube.remote.ui.shared.mixin.DataProviderMixin
import io.reactivex.rxkotlin.subscribeBy

class AllCategoriesActivity: BaseActivity() {
    private lateinit var data: DataProviderMixin
    private lateinit var adapter: AllCategoriesAdapter

    override fun onCreate(savedInstanceState: Bundle?) {
        component.inject(this)
        data = mixin(DataProviderMixin())

        super.onCreate(savedInstanceState)

        setTitle(R.string.category_activity)
        setContentView(R.layout.recycler_view_activity)

        val recyclerView = findViewById<FastScrollRecyclerView>(R.id.recycler_view)
        adapter = AllCategoriesAdapter(adapterListener)

        setupDefaultRecyclerView(recyclerView, adapter)
        enableUpNavigation()
        addTransportFragment()
    }

    override fun onResume() {
        super.onResume()
        initObservers()
    }

    private fun requery() {
        disposables.add(data.provider.listCategories().subscribeBy(
            onNext = {
                adapter.setModel(it)
            },
            onError = {
            }))
    }

    private fun initObservers() {
        disposables.add(data.provider.observeState().subscribeBy(
            onNext = { states ->
                if (states.first == IDataProvider.State.Connected) {
                    requery()
                }
            },
            onError = {
            }))
    }

    private val adapterListener = object:AllCategoriesAdapter.EventListener {
        override fun onItemClicked(category: String) {
            startActivity(CategoryBrowseActivity.getStartIntent(this@AllCategoriesActivity, category))
        }
    }

    companion object {
        fun getStartIntent(context: Context): Intent {
            return Intent(context, AllCategoriesActivity::class.java)
        }
    }
}