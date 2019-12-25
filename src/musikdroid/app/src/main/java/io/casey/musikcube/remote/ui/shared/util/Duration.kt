package io.casey.musikcube.remote.ui.shared.util

import java.util.*

object Duration {
    fun format(seconds: Double): String {
        val mins = seconds.toInt() / 60
        val secs = seconds.toInt() - mins * 60
        return String.format(Locale.getDefault(), "%d:%02d", mins, secs)
    }

    fun formatWithHours(seconds: Double): String {
        return when(val hours = seconds.toInt() / 3600) {
            0 -> format(seconds)
            else -> {
                val mins = (seconds.toInt() % 3600) / 60
                val secs = (seconds.toInt() % 60)
                String.format(Locale.getDefault(), "%d:%02d:%02d", hours, mins, secs)
            }
        }
    }
}
