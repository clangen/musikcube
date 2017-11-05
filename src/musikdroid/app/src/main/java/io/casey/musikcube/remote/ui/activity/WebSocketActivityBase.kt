package io.casey.musikcube.remote.ui.activity

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
import io.casey.musikcube.remote.data.IDataProvider
import io.casey.musikcube.remote.injection.*
import io.casey.musikcube.remote.playback.PlaybackService
import io.casey.musikcube.remote.playback.PlaybackServiceFactory
import io.casey.musikcube.remote.ui.extension.hideKeyboard
import io.casey.musikcube.remote.websocket.Prefs
import io.casey.musikcube.remote.websocket.WebSocketService
import io.reactivex.disposables.CompositeDisposable
import javax.inject.Inject

abstract class WebSocketActivityBase : AppCompatActivity(), Runner.TaskCallbacks {
    protected var disposables = CompositeDisposable()
    private lateinit var runnerDelegate: LifecycleDelegate
    private lateinit var prefs: SharedPreferences
    private var paused = false
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

        volumeControlStream = AudioManager.STREAM_MUSIC
        runnerDelegate = LifecycleDelegate(this, this, javaClass, null)
        runnerDelegate.onCreate(savedInstanceState)
        playbackService = PlaybackServiceFactory.instance(this)
        prefs = getSharedPreferences(Prefs.NAME, Context.MODE_PRIVATE)
    }

    override fun onPause() {
        hideKeyboard()

        super.onPause()

        dataProvider.detach()
        runnerDelegate.onPause()

        val playbackListener = playbackServiceEventListener
        if (playbackListener != null) {
            playbackService?.disconnect(playbackServiceEventListener!!)
        }

        val wssClient = webSocketServiceClient
        if (wssClient != null) {
            wss.removeClient(webSocketServiceClient!!)
        }

        paused = true
    }

    protected fun isPaused(): Boolean {
        return paused
    }

    override fun onResume() {
        super.onResume()

        dataProvider.attach()
        runnerDelegate.onResume()

        playbackService = PlaybackServiceFactory.instance(this)

        val playbackListener = playbackServiceEventListener
        if (playbackListener != null) {
            this.playbackService?.connect(playbackServiceEventListener!!)
        }

        val wssClient = webSocketServiceClient
        if (wssClient != null) {
            wss.addClient(webSocketServiceClient!!)
        }

        paused = false

        disposables.dispose()
        disposables = CompositeDisposable()
    }

    override fun onDestroy() {
        super.onDestroy()
        runnerDelegate.onDestroy()
        dataProvider.destroy()
    }

    override fun onSaveInstanceState(outState: Bundle) {
        super.onSaveInstanceState(outState)
        runnerDelegate.onSaveInstanceState(outState)
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

    protected fun getWebSocketService(): WebSocketService {
        return wss
    }

    protected var playbackService: PlaybackService? = null
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

    protected open val webSocketServiceClient: WebSocketService.Client?
        get() = null

    protected open val playbackServiceEventListener: (() -> Unit)?
        get() = null
}
