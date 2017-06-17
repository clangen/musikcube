package io.casey.musikcube.remote

import android.arch.persistence.room.Room
import android.content.Context
import com.crashlytics.android.Crashlytics
import com.facebook.stetho.Stetho
import io.casey.musikcube.remote.offline.OfflineDb
import io.casey.musikcube.remote.playback.StreamProxy
import io.casey.musikcube.remote.util.NetworkUtil
import io.fabric.sdk.android.Fabric

class Application : android.app.Application() {

    override fun onCreate() {
        instance = this

        super.onCreate()

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
        var instance: Context? = null
            private set

        var offlineDb: OfflineDb? = null
            private set
    }
}
