package io.casey.musikcube.remote.ui.settings.activity

import android.os.Bundle
import android.os.Handler
import android.widget.TextView
import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.service.system.SystemService
import io.casey.musikcube.remote.ui.shared.activity.BaseActivity
import io.casey.musikcube.remote.ui.shared.extension.enableUpNavigation
import io.casey.musikcube.remote.ui.shared.util.Duration

class DiagnosticsActivity: BaseActivity() {
    private lateinit var appRuntime: TextView
    private lateinit var wakeRuntime: TextView
    private lateinit var wakeAcquired: TextView
    private lateinit var serviceState: TextView
    private val handler = Handler()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.diagnostics_activity)
        appRuntime = findViewById(R.id.app_runtime)
        wakeRuntime = findViewById(R.id.wakelock_runtime)
        wakeAcquired = findViewById(R.id.wakelock_acquired)
        serviceState = findViewById(R.id.service_state)
        rebind()
        setTitle(R.string.diagnostics_title)
        enableUpNavigation()
    }

    override fun onPause() {
        super.onPause()
        handler.removeCallbacks(refreshRunnable)
    }

    override fun onResume() {
        super.onResume()
        handler.post(refreshRunnable)
    }

    private fun rebind() {
        val appDuration = (System.nanoTime() - Application.startTimeNanos).toDouble() / 1_000_000_000.0
        appRuntime.text = getString(
            R.string.diagnostics_app_runtime,
            Duration.formatWithHours(appDuration))
        wakeRuntime.text = getString(
            R.string.diagnostics_wakelock_runtime,
            Duration.formatWithHours(SystemService.wakeLockTime))
        wakeAcquired.text = getString(
            R.string.diagnostics_wakelock_acquired,
            SystemService.isWakeLockActive.toString().toLowerCase())
        serviceState.text = getString(
            R.string.diagnostics_system_service,
            SystemService.state.toString().toLowerCase())
    }

    private val refreshRunnable = object: Runnable {
        override fun run() {
            rebind()
            handler.postDelayed(this, 1000)
        }
    }
}