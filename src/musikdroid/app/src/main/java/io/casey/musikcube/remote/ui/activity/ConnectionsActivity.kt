package io.casey.musikcube.remote.ui.activity

import android.os.Bundle
import android.support.v7.app.AppCompatActivity
import io.casey.musikcube.remote.Application

/**
 * Created by casey on 8/21/2017.
 */

class ConnectionsActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        Application.mainComponent.inject(this);
        super.onCreate(savedInstanceState)
    }
}