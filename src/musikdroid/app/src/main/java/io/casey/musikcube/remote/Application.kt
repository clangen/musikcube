package io.casey.musikcube.remote

import android.arch.persistence.room.Room
import com.crashlytics.android.Crashlytics
import com.facebook.stetho.Stetho
import io.casey.musikcube.remote.ui.settings.model.ConnectionsDb
import io.casey.musikcube.remote.service.playback.impl.streaming.offline.OfflineDb
import io.casey.musikcube.remote.injection.DaggerAppComponent
import io.casey.musikcube.remote.injection.AppComponent
import io.casey.musikcube.remote.injection.AppModule
import io.casey.musikcube.remote.injection.ServiceModule
import io.casey.musikcube.remote.service.playback.impl.streaming.StreamProxy
import io.casey.musikcube.remote.ui.shared.util.NetworkUtil
import io.fabric.sdk.android.Fabric

class Application : android.app.Application() {
    override fun onCreate() {
        instance = this

        super.onCreate()

        appComponent = DaggerAppComponent.builder()
            .appModule(AppModule())
            .serviceModule(ServiceModule())
            .build()

        if (BuildConfig.DEBUG) {
            Stetho.initializeWithDefaults(this)
        }
        else {
            Fabric.with(this, Crashlytics())
        }

        NetworkUtil.init()
        StreamProxy.init(this)

        offlineDb = Room.databaseBuilder(
            applicationContext,
            OfflineDb::class.java,
            "offline").build()

        connectionsDb = Room.databaseBuilder(
            applicationContext,
            ConnectionsDb::class.java,
            "connections").build()
    }

    companion object {
        lateinit var appComponent: AppComponent

        var instance: Application? = null
            private set

        var offlineDb: OfflineDb? = null
            private set

        var connectionsDb: ConnectionsDb? = null
            private set
    }
}
