package io.casey.musikcube.remote

import android.content.Context
import io.casey.musikcube.remote.injection.AppComponent
import io.casey.musikcube.remote.injection.AppModule
import io.casey.musikcube.remote.injection.DaggerAppComponent
import io.casey.musikcube.remote.injection.ServiceModule
import io.casey.musikcube.remote.service.gapless.GaplessHeaderService
import io.casey.musikcube.remote.service.playback.impl.streaming.db.OfflineDb
import io.casey.musikcube.remote.ui.settings.constants.Prefs
import io.casey.musikcube.remote.ui.shared.extension.getString
import java.util.*
import javax.inject.Inject

class Application : android.app.Application() {
    @Inject lateinit var gaplessService: GaplessHeaderService
    @Inject @Suppress("unused") lateinit var offlineDb: OfflineDb

    override fun onCreate() {
        instance = this

        super.onCreate()

        val prefs = getSharedPreferences(Prefs.NAME, Context.MODE_PRIVATE)
        Application.deviceId = prefs.getString(Prefs.Key.DEVICE_ID) ?: ""
        if (Application.deviceId.isBlank()) {
            Application.deviceId = UUID.randomUUID().toString()
            prefs.edit().putString(Prefs.Key.DEVICE_ID, Application.deviceId).apply()
        }

        appComponent = DaggerAppComponent.builder()
            .appModule(AppModule())
            .serviceModule(ServiceModule())
            .build()

        appComponent.inject(this)

        gaplessService.schedule()
    }

    companion object {
        lateinit var appComponent: AppComponent
            private set

        val startTimeNanos = System.nanoTime()

        var deviceId: String = ""
            private set

        lateinit var instance: Application
            private set
    }
}
