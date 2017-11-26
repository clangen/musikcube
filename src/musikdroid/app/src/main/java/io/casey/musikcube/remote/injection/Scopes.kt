package io.casey.musikcube.remote.injection

import javax.inject.Scope

@Scope
@Retention(AnnotationRetention.RUNTIME)
annotation class ApplicationScope

@Scope
@Retention(AnnotationRetention.RUNTIME)
annotation class ViewScope

@Scope
@Retention(AnnotationRetention.RUNTIME)
annotation class ServiceScope

@Scope
@Retention(AnnotationRetention.RUNTIME)
annotation class DataScope

@Scope
@Retention(AnnotationRetention.RUNTIME)
annotation class PlaybackScope