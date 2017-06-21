package io.casey.musikcube.remote.util

import java.util.*

object Duration {
    fun format(seconds: Double): String {
        val mins = seconds.toInt() / 60
        val secs = seconds.toInt() - mins * 60
        return String.format(Locale.getDefault(), "%d:%02d", mins, secs)
    }
}
