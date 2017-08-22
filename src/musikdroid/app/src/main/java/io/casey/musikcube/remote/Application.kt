package io.casey.musikcube.remote

import android.arch.persistence.room.Room
import com.crashlytics.android.Crashlytics
import com.facebook.stetho.Stetho
import io.casey.musikcube.remote.injection.DaggerMainComponent
import io.casey.musikcube.remote.injection.MainComponent
import io.casey.musikcube.remote.injection.MainModule
import io.casey.musikcube.remote.offline.OfflineDb
import io.casey.musikcube.remote.playback.StreamProxy
import io.casey.musikcube.remote.util.NetworkUtil
import io.fabric.sdk.android.Fabric

class Application : android.app.Application() {
    override fun onCreate() {
        instance = this

        super.onCreate()

        mainComponent = DaggerMainComponent.builder().mainModule(MainModule()).build()

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
    }

    companion object {
        lateinit var mainComponent: MainComponent

        var instance: Application? = null
            private set

        var offlineDb: OfflineDb? = null
            private set
    }
}
