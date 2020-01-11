package io.casey.musikcube.remote.ui.shared.fragment

import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.os.Bundle
import android.os.Handler
import android.view.View
import android.view.animation.AlphaAnimation
import android.view.animation.Animation
import android.view.animation.AnimationUtils
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.Toolbar
import androidx.core.view.ViewCompat
import androidx.fragment.app.Fragment
import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.framework.IMixin
import io.casey.musikcube.remote.framework.MixinSet
import io.casey.musikcube.remote.framework.ViewModel
import io.casey.musikcube.remote.injection.DaggerViewComponent
import io.casey.musikcube.remote.injection.ViewComponent
import io.casey.musikcube.remote.ui.settings.constants.Prefs
import io.casey.musikcube.remote.ui.shared.activity.IBackHandler
import io.casey.musikcube.remote.ui.shared.activity.IFilterable
import io.casey.musikcube.remote.ui.shared.activity.ITitleProvider
import io.casey.musikcube.remote.ui.shared.extension.collapseActionViewIfExpanded
import io.casey.musikcube.remote.ui.shared.extension.elevation
import io.casey.musikcube.remote.ui.shared.extension.initToolbarIfNecessary
import io.casey.musikcube.remote.ui.shared.extension.setTitleFromIntent
import io.casey.musikcube.remote.ui.shared.mixin.ViewModelMixin
import io.reactivex.disposables.CompositeDisposable


open class BaseFragment: Fragment(), ViewModel.Provider, IBackHandler {
    private val mixins = MixinSet()
    protected val handler = Handler()
    protected lateinit var prefs: SharedPreferences
    protected val component: ViewComponent =
        DaggerViewComponent.builder()
            .appComponent(Application.appComponent)
            .build()

    protected var paused = true /* `private set` confuses proguard. sigh */

    protected var animating = false
        private set(value) {
            field = value
            when (field) {
                true -> onDestroyObservables()
                false -> onInitObservables()
            }
        }

    protected var disposables = CompositeDisposable()
        private set

    private fun onDestroyObservables() {
        disposables.dispose()
        disposables = CompositeDisposable()
    }

    protected open fun onInitObservables() {
        /* for subclass use */
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        mixins.onCreate(savedInstanceState ?: Bundle())
        prefs = Application.instance.getSharedPreferences(Prefs.NAME, Context.MODE_PRIVATE)
        handler.post { onPostCreate(savedInstanceState) }
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        ViewCompat.setElevation(view, extras.elevation)
        val enableFilter = (this is IFilterable) && this.addFilterToToolbar
        initToolbarIfNecessary(view, enableFilter)
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
        if (!animating) {
            onInitObservables()
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
                AlphaAnimation(1f, 1f).apply {
                    duration = getNextAnimationDuration(parent)
                }
            }
            false -> {
                if (nextAnim == 0) {
                    super.onCreateAnimation(transit, enter, nextAnim)
                }
                else {
                    AnimationUtils.loadAnimation(activity, nextAnim)?.apply {
                        view?.setLayerType(View.LAYER_TYPE_HARDWARE, null)
                        animating = true

                        setAnimationListener(object : Animation.AnimationListener {
                            override fun onAnimationRepeat(animation: Animation?) {
                            }

                            override fun onAnimationEnd(animation: Animation?) {
                                view?.setLayerType(View.LAYER_TYPE_NONE, null)
                                animating = false
                            }

                            override fun onAnimationStart(animation: Animation?) {
                            }
                        })
                    }
                }
            }
        }
    }

    override fun onBackPressed(): Boolean =
        toolbar?.collapseActionViewIfExpanded() ?: false

    override fun <T: ViewModel<*>> createViewModel(): T? = null
    @Suppress protected fun <T: ViewModel<*>> getViewModel(): T? = mixin(ViewModelMixin::class.java)?.get<T>() as T
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

        private fun getNextAnimationDuration(fragment: Fragment): Long =
            try {
                /* attempt to get the resource ID of the next animation that
                will be applied to the given fragment. */
                val nextAnimField = Fragment::class.java.getDeclaredMethod("getNextAnim")
                nextAnimField.isAccessible = true
                val nextAnimResource = nextAnimField.invoke(fragment) as Int
                val nextAnim = AnimationUtils.loadAnimation(fragment.activity, nextAnimResource)
                when (nextAnim == null) {
                    true -> DEFAULT_CHILD_ANIMATION_DURATION
                    false -> nextAnim.duration
                }
            }
            catch (ex: Exception) {
                DEFAULT_CHILD_ANIMATION_DURATION
            }
    }
}