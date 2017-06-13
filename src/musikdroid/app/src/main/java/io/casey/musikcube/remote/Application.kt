package io.casey.musikcube.remote

import android.arch.persistence.room.Room
import android.content.Context
import com.facebook.stetho.Stetho
import io.casey.musikcube.remote.offline.OfflineDb
import io.casey.musikcube.remote.playback.StreamProxy
import io.casey.musikcube.remote.util.NetworkUtil

class Application : android.app.Application() {

    override fun onCreate() {
        instance = this

        super.onCreate()

        if (BuildConfig.DEBUG) {
            Stetho.initializeWithDefaults(this)
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
