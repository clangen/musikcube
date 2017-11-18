package io.casey.musikcube.remote.framework

import android.content.Intent
import android.os.Bundle

interface IMixin {
    fun onCreate(bundle: Bundle)
    fun onStart()
    fun onResume()
    fun onPause()
    fun onStop()
    fun onSaveInstanceState(bundle: Bundle)
    fun onActivityResult(request: Int, result: Int, data: Intent?)
    fun onDestroy()
}