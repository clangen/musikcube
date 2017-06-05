package io.casey.musikcube.remote;

import com.facebook.stetho.Stetho;

import io.casey.musikcube.remote.playback.StreamProxy;
import io.casey.musikcube.remote.util.NetworkUtil;

public class Application extends android.app.Application {
    private static Application instance;

    @Override
    public void onCreate() {
        instance = this;

        super.onCreate();

        if (BuildConfig.DEBUG) {
            Stetho.initializeWithDefaults(this);
        }

        NetworkUtil.init();
        StreamProxy.init(this);
    }

    public static Application getInstance() {
        return instance;
    }
}
