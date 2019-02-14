package io.casey.musikcube.remote.ui.shared.activity

import android.view.Menu
import android.view.MenuItem

interface IMenuProvider {
    fun createOptionsMenu(menu: Menu): Boolean
    fun optionsItemSelected(menuItem: MenuItem): Boolean
}