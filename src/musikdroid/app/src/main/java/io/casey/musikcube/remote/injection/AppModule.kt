package io.casey.musikcube.remote.injection

import android.content.Context
import dagger.Module
import dagger.Provides
import io.casey.musikcube.remote.Application

@Module
class AppModule {
    @Provides
    @ApplicationScope
    fun providesContext(): Context {
        return Application.instance
    }
}
