package io.casey.musikcube.remote.injection

import dagger.Component
import dagger.android.AndroidInjectionModule
import dagger.android.AndroidInjector
import io.casey.musikcube.remote.Application

@Component(modules = arrayOf(AndroidInjectionModule::class, MainModule::class, ActivityModule::class))
interface MainComponent : AndroidInjector<Application> {

}
