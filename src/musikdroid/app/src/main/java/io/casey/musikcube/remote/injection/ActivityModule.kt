package io.casey.musikcube.remote.injection

import dagger.Module
import dagger.android.ContributesAndroidInjector
import io.casey.musikcube.remote.MainActivity
import io.casey.musikcube.remote.ui.activity.*

@Module
abstract class ActivityModule {
    @ContributesAndroidInjector
    abstract fun mainActivityInjector(): MainActivity

    @ContributesAndroidInjector
    abstract fun baseActivityInject(): WebSocketActivityBase

    @ContributesAndroidInjector
    abstract fun settingsInjector(): SettingsActivity

    @ContributesAndroidInjector
    abstract fun albumBrowseInject(): AlbumBrowseActivity

    @ContributesAndroidInjector
    abstract fun categoryBrowseInject(): CategoryBrowseActivity

    @ContributesAndroidInjector
    abstract fun playQueueInject(): PlayQueueActivity

    @ContributesAndroidInjector
    abstract fun trackListInject(): TrackListActivity
}
