package io.casey.musikcube.remote.ui.shared.activity

import android.content.Context
import android.content.SharedPreferences
import android.media.AudioManager
import android.os.Bundle
import android.support.v7.app.AppCompatActivity
import android.view.KeyEvent
import android.view.MenuItem
import com.uacf.taskrunner.LifecycleDelegate
import com.uacf.taskrunner.Runner
import com.uacf.taskrunner.Task
import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.framework.components.ComponentSet
import io.casey.musikcube.remote.framework.components.IComponent
import io.casey.musikcube.remote.service.websocket.model.IDataProvider
import io.casey.musikcube.remote.injection.*
import io.casey.musikcube.remote.service.playback.IPlaybackService
import io.casey.musikcube.remote.service.playback.PlaybackServiceFactory
import io.casey.musikcube.remote.ui.shared.extension.hideKeyboard
import io.casey.musikcube.remote.ui.settings.constants.Prefs
import io.casey.musikcube.remote.service.websocket.WebSocketService
import io.reactivex.disposables.CompositeDisposable
import javax.inject.Inject

abstract class BaseActivity : AppCompatActivity(), Runner.TaskCallbacks {
    protected var disposables = CompositeDisposable()
    private lateinit var runnerDelegate: LifecycleDelegate
    private lateinit var prefs: SharedPreferences
    private var paused = false
    private val components = ComponentSet()
    @Inject lateinit var wss: WebSocketService
    @Inject lateinit var dataProvider: IDataProvider

    protected val component: ViewComponent =
        DaggerViewComponent.builder()
            .appComponent(Application.appComponent)
            .dataModule(DataModule())
            .build()

    override fun onCreate(savedInstanceState: Bundle?) {
        component.inject(this)

        super.onCreate(savedInstanceState)

        components.onCreate(savedInstanceState ?: Bundle())
        volumeControlStream = AudioManager.STREAM_MUSIC
        runnerDelegate = LifecycleDelegate(this, this, javaClass, null)
        runnerDelegate.onCreate(savedInstanceState)
        playbackService = PlaybackServiceFactory.instance(this)
        prefs = getSharedPreferences(Prefs.NAME, Context.MODE_PRIVATE)
    }

    override fun onStart() {
        super.onStart()
        components.onStart()
    }

    override fun onResume() {
        super.onResume()

        components.onResume()
        dataProvider.attach()
        runnerDelegate.onResume()

        playbackService = PlaybackServiceFactory.instance(this)

        val playbackListener = playbackServiceEventListener
        if (playbackListener != null) {
            this.playbackService?.connect(playbackServiceEventListener!!)
        }

        paused = false
    }

    override fun onPause() {
        hideKeyboard()

        super.onPause()

        components.onPause()
        dataProvider.detach()
        runnerDelegate.onPause()

        val playbackListener = playbackServiceEventListener
        if (playbackListener != null) {
            playbackService?.disconnect(playbackServiceEventListener!!)
        }

        disposables.dispose()
        disposables = CompositeDisposable()

        paused = true
    }

    override fun onStop() {
        super.onStop()
        components.onStop()
    }

    override fun onSaveInstanceState(outState: Bundle) {
        super.onSaveInstanceState(outState)
        components.onSaveInstanceState(outState)
        runnerDelegate.onSaveInstanceState(outState)
    }

    override fun onDestroy() {
        super.onDestroy()
        components.onDestroy()
        runnerDelegate.onDestroy()
        dataProvider.destroy()
    }

    override fun onKeyDown(keyCode: Int, event: KeyEvent): Boolean {
        val streaming = prefs.getBoolean(
            Prefs.Key.STREAMING_PLAYBACK, Prefs.Default.STREAMING_PLAYBACK)

        /* if we're not streaming we want the hardware buttons to go out to the system */
        if (!streaming) {
            if (keyCode == KeyEvent.KEYCODE_VOLUME_DOWN) {
                playbackService?.volumeDown()
                return true
            }
            else if (keyCode == KeyEvent.KEYCODE_VOLUME_UP) {
                playbackService?.volumeUp()
                return true
            }
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
    protected fun component(component: IComponent) = components.add(component)
    protected fun <T> component(cls: Class<out IComponent>): T? = components.get(cls)

    protected val socketService: WebSocketService get() = wss

    protected var playbackService: IPlaybackService? = null
        private set

    protected val runner: Runner
        get() = runnerDelegate.runner()

    protected fun reloadPlaybackService() {
        if (!isPaused() && playbackService != null) {
            val playbackListener = playbackServiceEventListener

            if (playbackListener != null) {
                playbackService?.disconnect(playbackServiceEventListener!!)
            }

            playbackService = PlaybackServiceFactory.instance(this)

            if (playbackListener != null) {
                playbackService?.connect(playbackServiceEventListener!!)
            }
        }
    }

    protected open val playbackServiceEventListener: (() -> Unit)?
        get() = null
}
