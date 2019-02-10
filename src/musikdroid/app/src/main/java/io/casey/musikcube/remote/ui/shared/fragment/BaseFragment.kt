package io.casey.musikcube.remote.ui.shared.fragment

import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.os.Bundle
import android.os.Handler
import android.support.v4.app.Fragment
import android.support.v7.app.AppCompatActivity
import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.framework.IMixin
import io.casey.musikcube.remote.framework.MixinSet
import io.casey.musikcube.remote.framework.ViewModel
import io.casey.musikcube.remote.injection.DaggerViewComponent
import io.casey.musikcube.remote.injection.ViewComponent
import io.casey.musikcube.remote.ui.settings.constants.Prefs
import io.casey.musikcube.remote.ui.shared.mixin.ViewModelMixin
import io.reactivex.disposables.CompositeDisposable

open class BaseFragment: Fragment(), ViewModel.Provider {
    private val mixins = MixinSet()
    private val handler = Handler()
    protected lateinit var prefs: SharedPreferences
    protected val component: ViewComponent =
        DaggerViewComponent.builder()
            .appComponent(Application.appComponent)
            .build()

    protected var paused = true /* `private set` confuses proguard. sigh */

    protected var disposables = CompositeDisposable()
        private set

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        mixins.onCreate(savedInstanceState ?: Bundle())
        prefs = Application.instance.getSharedPreferences(Prefs.NAME, Context.MODE_PRIVATE)
        handler.post { onPostCreate(savedInstanceState) }
    }

    open fun onPostCreate(savedInstanceState: Bundle?) {

    }

    override fun onStart() {
        super.onStart()
        mixins.onStart()
    }

    override fun onResume() {
        super.onResume()
        paused = false
        mixins.onResume()
    }

    override fun onPause() {
        super.onPause()
        disposables.dispose()
        disposables = CompositeDisposable()
        paused = true
        mixins.onPause()
    }

    override fun onStop() {
        super.onStop()
        mixins.onStop()
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        mixins.onActivityResult(requestCode, resultCode, data)
    }

    override fun onSaveInstanceState(outState: Bundle) {
        super.onSaveInstanceState(outState)
        mixins.onSaveInstanceState(outState)
    }

    override fun onDestroy() {
        super.onDestroy()
        mixins.onDestroy()
    }

    override fun <T: ViewModel<*>> createViewModel(): T? = null
    protected fun <T: ViewModel<*>> getViewModel(): T? = mixin(ViewModelMixin::class.java)?.get<T>() as T
    protected fun <T: IMixin> mixin(mixin: T): T = mixins.add(mixin)
    protected fun <T: IMixin> mixin(cls: Class<out T>): T? = mixins.get(cls)

    protected val extras: Bundle
        get() = arguments ?: Bundle()

    val appCompatActivity: AppCompatActivity
        get() = activity as AppCompatActivity

    val app: Application
        get() = Application.instance
}