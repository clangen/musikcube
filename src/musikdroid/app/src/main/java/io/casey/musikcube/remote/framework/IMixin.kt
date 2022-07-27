package io.casey.musikcube.remote.framework

import android.os.Bundle

interface IMixin {
    fun onCreate(bundle: Bundle)
    fun onStart()
    fun onResume()
    fun onPause()
    fun onStop()
    fun onSaveInstanceState(bundle: Bundle)
    fun onDestroy()
}