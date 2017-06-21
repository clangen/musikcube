package io.casey.musikcube.remote.util

import android.os.Looper

object Preconditions {
    fun throwIfNotOnMainThread() {
        if (Thread.currentThread() !== Looper.getMainLooper().thread) {
            throw IllegalStateException("not on main thread")
        }
    }
}
