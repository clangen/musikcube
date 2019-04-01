package io.casey.musikcube.remote.ui.shared.activity

import com.google.android.material.floatingactionbutton.FloatingActionButton

interface IFabConsumer {
    val fabVisible: Boolean
    fun onFabPress(fab: FloatingActionButton)
}