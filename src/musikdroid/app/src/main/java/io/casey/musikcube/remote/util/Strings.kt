package io.casey.musikcube.remote.util

object Strings {
    fun empty(s: String?): Boolean {
        return s == null || s.isEmpty()
    }

    fun notEmpty(s: String?): Boolean {
        return s != null && s.isNotEmpty()
    }
}
