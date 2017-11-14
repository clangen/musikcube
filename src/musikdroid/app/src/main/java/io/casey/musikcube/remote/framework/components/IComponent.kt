package io.casey.musikcube.remote.framework.components

import android.os.Bundle

interface IComponent {
    fun onCreate(bundle: Bundle)
    fun onStart()
    fun onResume()
    fun onPause()
    fun onStop()
    fun onSaveInstanceState(bundle: Bundle)
    fun onDestroy()
}