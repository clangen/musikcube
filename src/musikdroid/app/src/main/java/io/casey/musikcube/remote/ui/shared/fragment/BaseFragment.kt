package io.casey.musikcube.remote.ui.shared.fragment

import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.os.Bundle
import android.os.Handler
import android.support.v4.app.Fragment
import android.support.v7.app.AppCompatActivity
import android.support.v7.widget.Toolbar
import android.view.animation.Animation
import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.framework.IMixin
import io.casey.musikcube.remote.framework.MixinSet
import io.casey.musikcube.remote.framework.ViewModel
import io.casey.musikcube.remote.injection.DaggerViewComponent
import io.casey.musikcube.remote.injection.ViewComponent
import io.casey.musikcube.remote.ui.settings.constants.Prefs
import io.casey.musikcube.remote.ui.shared.activity.ITitleProvider
import io.casey.musikcube.remote.ui.shared.extension.setTitleFromIntent
import io.casey.musikcube.remote.ui.shared.mixin.ViewModelMixin
import io.reactivex.disposables.CompositeDisposable
import android.view.animation.AlphaAnimation
import android.view.animation.AnimationUtils
import io.casey.musikcube.remote.R
import java.lang.Exception


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
        if (this is ITitleProvider) {
            toolbar?.setTitleFromIntent(title)
        }
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

    /* https://stackoverflow.com/a/23276145 */
    override fun onCreateAnimation(transit: Int, enter: Boolean, nextAnim: Int): Animation? {
        val parent = parentFragment

        /* only apply for childFragmentManager transitions */
        return when (!enter && parent != null && parent.isRemoving) {
            true -> {
                /* this is a workaround for the bug where child fragments disappear when
                the parent is removed (as all children are first removed from the parent)
                See https://code.google.com/p/android/issues/detail?id=55228 */
                val doNothingAnim = AlphaAnimation(1f, 1f)
                doNothingAnim.duration = getNextAnimationDuration(parent, DEFAULT_CHILD_ANIMATION_DURATION)
                doNothingAnim
            }
            false -> {
                super.onCreateAnimation(transit, enter, nextAnim)
            }
        }
    }

    override fun <T: ViewModel<*>> createViewModel(): T? = null
    protected fun <T: ViewModel<*>> getViewModel(): T? = mixin(ViewModelMixin::class.java)?.get<T>() as T
    protected fun <T: IMixin> mixin(mixin: T): T = mixins.add(mixin)
    protected fun <T: IMixin> mixin(cls: Class<out T>): T? = mixins.get(cls)

    val hasToolbar: Boolean
        get() = this.toolbar != null

    val toolbar: Toolbar?
        get() = this.view?.findViewById(R.id.toolbar)

    val extras: Bundle
        get() {
            if (arguments == null) {
                arguments = Bundle()
            }
            return arguments!!
        }

    val appCompatActivity: AppCompatActivity
        get() = activity as AppCompatActivity

    val app: Application
        get() = Application.instance

    companion object {
        private const val DEFAULT_CHILD_ANIMATION_DURATION = 250L

        private fun getNextAnimationDuration(fragment: Fragment, defValue: Long): Long =
            try {
                /* attempt to get the resource ID of the next animation that
                will be applied to the given fragment. */
                val nextAnimField = Fragment::class.java.getDeclaredField("mNextAnim")
                nextAnimField.isAccessible = true
                val nextAnimResource = nextAnimField.getInt(fragment)
                val nextAnim = AnimationUtils.loadAnimation(fragment.activity, nextAnimResource)
                when (nextAnim == null) {
                    true -> defValue
                    false -> nextAnim.duration
                }
            }
            catch (ex: Exception) {
                defValue
            }
    }
}