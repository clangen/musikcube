package io.casey.musikcube.remote.ui.shared.activity

interface IBackHandler {
    fun onInterceptBackButton(): Boolean
}