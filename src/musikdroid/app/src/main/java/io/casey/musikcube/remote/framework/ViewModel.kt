package io.casey.musikcube.remote.framework

import android.content.Context
import android.os.Handler
import android.os.Looper
import com.uacf.taskrunner.Runner
import com.uacf.taskrunner.Task
import io.casey.musikcube.remote.Application
import io.reactivex.Observable
import io.reactivex.android.schedulers.AndroidSchedulers
import io.reactivex.subjects.PublishSubject
import io.reactivex.subjects.Subject
import java.util.concurrent.atomic.AtomicLong

    abstract class ViewModel<T>(protected val runner: Runner? = null): Runner.TaskCallbacks {
    val id: Long = nextId.incrementAndGet()
    private val publisher by lazy { createSubject() }

    interface Provider {
        fun <T: ViewModel<*>> createViewModel(): T?
    }

    open fun onPause() {
    }

    open fun onResume() {
    }

    open fun onDestroy() {
        handler.postDelayed(cleanup, cleanupDelayMs)
    }

    open fun onCleanup() {

    }

    fun observe(): Observable<T> {
        return publisher
            .observeOn(AndroidSchedulers.mainThread())
            .subscribeOn(AndroidSchedulers.mainThread())
    }

    val context: Context = Application.instance!!

    internal val cleanup = Runnable {
        idToInstance.remove(id)
        onCleanup()
    }

    protected fun publish(value: T) {
        publisher.onNext(value)
    }

    open fun createSubject(): Subject<T> {
        return PublishSubject.create<T>()
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