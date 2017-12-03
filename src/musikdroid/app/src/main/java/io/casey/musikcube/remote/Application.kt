package io.casey.musikcube.remote

import com.crashlytics.android.Crashlytics
import com.facebook.stetho.Stetho
import io.casey.musikcube.remote.injection.AppComponent
import io.casey.musikcube.remote.injection.AppModule
import io.casey.musikcube.remote.injection.DaggerAppComponent
import io.casey.musikcube.remote.injection.ServiceModule
import io.casey.musikcube.remote.service.gapless.GaplessHeaderService
import io.casey.musikcube.remote.service.playback.impl.streaming.db.OfflineDb
import io.fabric.sdk.android.Fabric
import javax.inject.Inject

class Application : android.app.Application() {
    @Inject lateinit var gaplessService: GaplessHeaderService
    @Inject lateinit var offlineDb: OfflineDb

    override fun onCreate() {
        instance = this

        super.onCreate()

        appComponent = DaggerAppComponent.builder()
            .appModule(AppModule())
            .serviceModule(ServiceModule())
            .build()

        appComponent.inject(this)

        gaplessService.schedule()

        if (BuildConfig.DEBUG) {
            Stetho.initializeWithDefaults(this)
        }
        else {
            Fabric.with(this, Crashlytics())
        }
    }

    companion object {
        lateinit var appComponent: AppComponent

        var instance: Application? = null
            private set
    }
}
