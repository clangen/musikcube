package io.casey.musikcube.remote.ui.shared.activity

import android.os.Bundle
import android.view.Menu
import android.view.MenuItem
import com.google.android.material.floatingactionbutton.FloatingActionButton
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.ui.navigation.Transition
import io.casey.musikcube.remote.ui.shared.constant.Shared
import io.casey.musikcube.remote.ui.shared.extension.*
import io.casey.musikcube.remote.ui.shared.fragment.BaseFragment
import io.casey.musikcube.remote.ui.shared.fragment.TransportFragment

abstract class FragmentActivityWithTransport: BaseActivity() {
    private var transport: TransportFragment? = null

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

        transport?.modelChangedListener = {
            onTransportChanged()
        }

        val fab = findViewById<FloatingActionButton>(R.id.fab)
        (content as? IFabConsumer)?.let { fabConsumer ->
            fab.setOnClickListener { fabConsumer.onFabPress(fab) }
            when (fabConsumer.fabVisible) {
                true -> fab.show()
                false -> fab.hide()
            }
        } ?: this.run {
            fab.hide()
        }
    }

    override fun onResume() {
        super.onResume()
        if (!content.hasToolbar) {
            (content as? ITitleProvider)?.run {
                setTitleFromIntent(this.title)
            }
        }
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        if (!content.hasToolbar) {
            (content as? IMenuProvider)?.run {
                return this.createOptionsMenu(menu)
            }
        }
        return false
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        if (!content.hasToolbar) {
            (content as? IMenuProvider)?.run {
                if (this.optionsItemSelected(item)) {
                    return true
                }
            }
        }
        return super.onOptionsItemSelected(item)
    }

    override val transitionType: Transition
        get() = extras.transitionType

    protected abstract fun createContentFragment(): BaseFragment
    protected abstract val contentFragmentTag: String

    protected open fun onTransportChanged() {
        supportFragmentManager.fragments.forEach {
            if (it is ITransportObserver) {
                it.onTransportChanged()
            }
        }
    }

    private fun createFragments() {
        content = createContentFragment()
            .withToolbar()
            .withTitleOverride(this)
            .pushTo(R.id.content_container)
        supportFragmentManager.beginTransaction().apply {
            add(R.id.content_container, content, contentFragmentTag)
            if (!withoutTransport) {
                transport = TransportFragment
                    .create()
                    .pushTo(R.id.content_container)
                    .apply {
                        add(R.id.transport_container, this, TransportFragment.TAG)
                    }
            }
            commit()
        }
        supportFragmentManager.executePendingTransactions()
    }

    private fun restoreFragments() {
        transport = findFragment(TransportFragment.TAG)
        content = findFragment(contentFragmentTag)
    }

    private val withoutTransport
        get() = extras.getBoolean(Shared.Extra.WITHOUT_TRANSPORT, false)
}