package io.casey.musikcube.remote.ui.shared.activity

import android.view.Menu

interface IMenuProvider {
    fun createOptionsMenu(menu: Menu): Boolean
}