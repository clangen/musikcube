package io.casey.musikcube.remote.ui.settings.activity

import android.os.Bundle
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.ui.shared.activity.BaseActivity
import io.casey.musikcube.remote.ui.shared.extension.enableUpNavigation

class DiagnosticsActivity: BaseActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.diagnostics_activity)
        setTitle(R.string.diagnostics_title)
        enableUpNavigation()
    }
}