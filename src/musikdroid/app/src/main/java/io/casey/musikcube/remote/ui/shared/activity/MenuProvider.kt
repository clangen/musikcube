package io.casey.musikcube.remote.ui.shared.activity

import android.view.Menu

interface MenuProvider {
    fun createOptionsMenu(menu: Menu): Boolean
}