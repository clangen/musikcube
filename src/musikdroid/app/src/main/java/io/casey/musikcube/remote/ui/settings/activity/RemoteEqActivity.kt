package io.casey.musikcube.remote.ui.settings.activity

import io.casey.musikcube.remote.ui.shared.activity.BaseActivity
import io.casey.musikcube.remote.ui.shared.extension.slideThisRight

class RemoteEqActivity: BaseActivity() {
    override fun finish() {
        super.finish()
        slideThisRight()
    }
}