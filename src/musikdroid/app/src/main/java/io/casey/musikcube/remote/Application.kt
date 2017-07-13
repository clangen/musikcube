package io.casey.musikcube.remote

import android.app.Activity
import android.arch.persistence.room.Room
import android.content.Context
import com.crashlytics.android.Crashlytics
import com.facebook.stetho.Stetho
import dagger.android.AndroidInjector
import dagger.android.DispatchingAndroidInjector
import dagger.android.HasActivityInjector
import io.casey.musikcube.remote.injection.DaggerMainComponent
import io.casey.musikcube.remote.injection.MainModule
import io.casey.musikcube.remote.offline.OfflineDb
import io.casey.musikcube.remote.playback.StreamProxy
import io.casey.musikcube.remote.util.NetworkUtil
import io.fabric.sdk.android.Fabric
import javax.inject.Inject

class Application : android.app.Application(), HasActivityInjector {
    @Inject lateinit var activityInjector: DispatchingAndroidInjector<Activity>

    override fun onCreate() {
        instance = this

        super.onCreate()

        DaggerMainComponent.builder().mainModule(MainModule()).build().inject(this)

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

    override fun activityInjector(): AndroidInjector<Activity> {
        return activityInjector
    }

    companion object {
        var instance: Context? = null
            private set

        var offlineDb: OfflineDb? = null
            private set
    }
}
