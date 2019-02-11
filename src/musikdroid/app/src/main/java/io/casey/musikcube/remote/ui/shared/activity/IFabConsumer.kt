package io.casey.musikcube.remote.ui.shared.activity

import android.support.design.widget.FloatingActionButton

interface IFabConsumer {
    val fabVisible: Boolean
    fun onFabPress(fab: FloatingActionButton)
}