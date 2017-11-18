package io.casey.musikcube.remote.framework

import android.content.Context
import android.os.Handler
import android.os.Looper
import com.uacf.taskrunner.Runner
import com.uacf.taskrunner.Task
import io.casey.musikcube.remote.Application
import java.util.concurrent.atomic.AtomicLong

abstract class ViewModel<ListenerT>(protected val runner: Runner? = null): Runner.TaskCallbacks {
    val id: Long = nextId.incrementAndGet()

    interface Provider {
        fun <T: ViewModel<*>> createViewModel(): T?
    }

    protected var listener: ListenerT? = null
        private set

    fun onPause() {
    }

    fun onResume() {
    }

    fun onDestroy() {
        listener = null
        handler.postDelayed(cleanup, cleanupDelayMs)
    }

    fun observe(listener: ListenerT) {
        this.listener = listener
    }

    val context: Context = Application.instance!!

    internal val cleanup = object: Runnable {
        override fun run() {
            listener = null
            idToInstance.remove(id)
        }
    }

    override fun onTaskError(name: String?, id: Long, task: Task<*, *>?, error: Throwable?) {
    }

    override fun onTaskCompleted(name: String?, id: Long, task: Task<*, *>?, result: Any?) {
    }

    companion object {
        private val cleanupDelayMs = 3000L
        private val nextId = AtomicLong(System.currentTimeMillis() + 0)
        private val handler by lazy { Handler(Looper.getMainLooper()) }
        private val idToInstance = mutableMapOf<Long, ViewModel<*>>()

        fun <T: ViewModel<*>> restore(id: Long): T?  {
            val instance: T? = idToInstance[id] as T?
            if (instance != null) {
                handler.removeCallbacks(instance.cleanup)
            }
            return instance
        }
    }
}