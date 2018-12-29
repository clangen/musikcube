package io.casey.musikcube.remote.ui.shared.activity

import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.media.AudioManager
import android.os.Bundle
import android.support.v7.app.AppCompatActivity
import android.view.KeyEvent
import android.view.MenuItem
import com.uacf.taskrunner.Runner
import com.uacf.taskrunner.Task
import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.framework.IMixin
import io.casey.musikcube.remote.framework.MixinSet
import io.casey.musikcube.remote.framework.ViewModel
import io.casey.musikcube.remote.injection.DaggerViewComponent
import io.casey.musikcube.remote.injection.ViewComponent
import io.casey.musikcube.remote.ui.settings.constants.Prefs
import io.casey.musikcube.remote.ui.shared.extension.hideKeyboard
import io.casey.musikcube.remote.ui.shared.mixin.PlaybackMixin
import io.casey.musikcube.remote.ui.shared.mixin.RunnerMixin
import io.casey.musikcube.remote.ui.shared.mixin.ViewModelMixin
import io.reactivex.disposables.CompositeDisposable

abstract class BaseActivity : AppCompatActivity(), ViewModel.Provider, Runner.TaskCallbacks {
    protected var disposables = CompositeDisposable()
    protected lateinit var prefs: SharedPreferences
    private var paused = false
    private val mixins = MixinSet()

    protected val component: ViewComponent =
        DaggerViewComponent.builder()
            .appComponent(Application.appComponent)
            .build()

    override fun onCreate(savedInstanceState: Bundle?) {
        component.inject(this)
        mixin(RunnerMixin(this, javaClass))
        super.onCreate(savedInstanceState)
        prefs = getSharedPreferences(Prefs.NAME, Context.MODE_PRIVATE)
        volumeControlStream = AudioManager.STREAM_MUSIC
        mixins.onCreate(savedInstanceState ?: Bundle())
    }

    override fun onStart() {
        super.onStart()
        mixins.onStart()
    }

    override fun onResume() {
        super.onResume()
        mixins.onResume()
        paused = false
    }

    override fun onPause() {
        hideKeyboard()
        super.onPause()
        mixins.onPause()
        disposables.dispose()
        disposables = CompositeDisposable()
        paused = true
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

    override fun onKeyDown(keyCode: Int, event: KeyEvent): Boolean {
        if (mixin(PlaybackMixin::class.java)?.onKeyDown(keyCode) == true) {
            return true
        }

        return super.onKeyDown(keyCode, event)
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        if (item.itemId == android.R.id.home) {
            finish()
            return true
        }

        return super.onOptionsItemSelected(item)
    }

    override fun onTaskCompleted(taskName: String, taskId: Long, task: Task<*, *>, result: Any) {
    }

    override fun onTaskError(s: String, l: Long, task: Task<*, *>, throwable: Throwable) {
    }

    protected fun isPaused(): Boolean = paused

    override fun <T: ViewModel<*>> createViewModel(): T? = null
    protected fun <T: ViewModel<*>> getViewModel(): T? = mixin(ViewModelMixin::class.java)?.get<T>() as T
    protected fun <T: IMixin> mixin(mixin: T): T = mixins.add(mixin)
    protected fun <T: IMixin> mixin(cls: Class<out T>): T? = mixins.get(cls)

    protected val runner: Runner
        get() = mixin(RunnerMixin::class.java)!!.runner
}
