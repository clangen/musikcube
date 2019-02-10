package io.casey.musikcube.remote.ui.shared.activity

import android.os.Bundle
import android.view.Menu
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.ui.shared.extension.enableUpNavigation
import io.casey.musikcube.remote.ui.shared.extension.findFragment
import io.casey.musikcube.remote.ui.shared.extension.setTitleFromIntent
import io.casey.musikcube.remote.ui.shared.fragment.BaseFragment
import io.casey.musikcube.remote.ui.shared.fragment.TransportFragment

abstract class FragmentActivityWithTransport: BaseActivity(), Filterable {
    protected lateinit var transport: TransportFragment
        private set

    protected lateinit var content: BaseFragment
        private set

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.fragment_with_transport_activity)
        enableUpNavigation()
    }

    override fun onPostCreate(savedInstanceState: Bundle?) {
        super.onPostCreate(savedInstanceState)

        when (savedInstanceState == null) {
            true -> createFragments()
            else -> restoreFragments()
        }

        transport.modelChangedListener = {
            onTransportChanged()
        }
    }

    override fun onResume() {
        super.onResume()
        (content as? TitleProvider)?.run {
            setTitleFromIntent(this.title)
        }
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean =
        (content as? MenuProvider)?.run {
            return this.createOptionsMenu(menu)
        } ?: false

    override fun setFilter(filter: String) =
        (content as? Filterable)?.run {
            setFilter(filter)
        } ?: Unit

    protected abstract fun createContentFragment(): BaseFragment
    protected abstract val contentFragmentTag: String

    protected open fun onTransportChanged() {

    }

    private fun createFragments() {
        content = createContentFragment()
        transport = TransportFragment.create()
        supportFragmentManager
            .beginTransaction()
            .add(R.id.content_container, content, contentFragmentTag)
            .add(R.id.transport_container, transport, TransportFragment.TAG)
            .commit()
    }

    private fun restoreFragments() {
        transport = findFragment(TransportFragment.TAG)
        content = findFragment(contentFragmentTag)
    }
}