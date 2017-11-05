package io.casey.musikcube.remote.injection

import dagger.Component
import io.casey.musikcube.remote.db.offline.OfflineDb

@DataScope
@Component(
    dependencies = arrayOf(AppComponent::class),
    modules = arrayOf())
interface DataComponent {
    fun inject(db: OfflineDb)
}