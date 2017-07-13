package io.casey.musikcube.remote.injection

import dagger.Module
import dagger.android.ContributesAndroidInjector
import io.casey.musikcube.remote.MainActivity

@Module
abstract class ActivityModule {
    @ContributesAndroidInjector
    abstract fun contributeMainActivityInjector(): MainActivity
}
