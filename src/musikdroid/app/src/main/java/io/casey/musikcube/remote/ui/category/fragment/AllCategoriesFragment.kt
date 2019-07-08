package io.casey.musikcube.remote.ui.category.fragment

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import com.simplecityapps.recyclerview_fastscroll.views.FastScrollRecyclerView
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.service.websocket.model.IMetadataProxy
import io.casey.musikcube.remote.ui.category.adapter.AllCategoriesAdapter
import io.casey.musikcube.remote.ui.navigation.Navigate
import io.casey.musikcube.remote.ui.shared.activity.ITitleProvider
import io.casey.musikcube.remote.ui.shared.extension.getLayoutId
import io.casey.musikcube.remote.ui.shared.extension.setupDefaultRecyclerView
import io.casey.musikcube.remote.ui.shared.fragment.BaseFragment
import io.casey.musikcube.remote.ui.shared.mixin.MetadataProxyMixin
import io.reactivex.rxkotlin.subscribeBy

class AllCategoriesFragment: BaseFragment(), ITitleProvider {
    private lateinit var data: MetadataProxyMixin
    private lateinit var adapter: AllCategoriesAdapter

    override val title: String
        get() = getString(R.string.category_activity)

    override fun onCreate(savedInstanceState: Bundle?) {
        component.inject(this)
        data = mixin(MetadataProxyMixin())

        super.onCreate(savedInstanceState)

        adapter = AllCategoriesAdapter(adapterListener)
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? =
        inflater.inflate(this.getLayoutId(), container, false).apply {
            val recyclerView = findViewById<FastScrollRecyclerView>(R.id.recycler_view)
            setupDefaultRecyclerView(recyclerView, adapter)
        }

    override fun onInitObservables() {
        disposables.add(data.provider.observeState().subscribeBy(
            onNext = { states ->
                if (states.first == IMetadataProxy.State.Connected) {
                    requery()
                }
            },
            onError = {
            }))
    }

    private fun requery() {
        disposables.add(data.provider.listCategories().subscribeBy(
            onNext = {
                adapter.setModel(it)
            },
            onError = {
            }))
    }


    private val adapterListener = object:AllCategoriesAdapter.EventListener {
        override fun onItemClicked(category: String) =
            Navigate.toCategoryList(
                category,
                appCompatActivity,
                this@AllCategoriesFragment)
    }

    companion object {
        const val TAG = "AllCategoriesFragment"

        fun create(): AllCategoriesFragment = AllCategoriesFragment()
    }
}