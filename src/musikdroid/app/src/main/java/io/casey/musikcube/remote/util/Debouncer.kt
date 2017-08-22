package io.casey.musikcube.remote.util

import android.os.Handler
import android.os.Looper

abstract class Debouncer<in T>(private val delay: Long) {
    private val handler = Handler(Looper.getMainLooper())
    private var last: T? = null

    fun call() {
        handler.removeCallbacks(deferred)
        handler.postDelayed(deferred, delay)
    }

    fun call(context: T) {
        last = context
        handler.removeCallbacks(deferred)
        handler.postDelayed(deferred, delay)
    }

    fun cancel() {
        handler.removeCallbacks(deferred)
    }

    protected abstract fun onDebounced(last: T?)

    private val deferred:Runnable = object : Runnable {
        override fun run() {
            onDebounced(last)
        }
    }
}
