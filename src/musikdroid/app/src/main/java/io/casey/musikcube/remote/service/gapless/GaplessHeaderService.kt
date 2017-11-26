package io.casey.musikcube.remote.service.gapless

import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.injection.DaggerServiceComponent
import io.casey.musikcube.remote.injection.DataModule
import io.casey.musikcube.remote.service.gapless.db.GaplessDb
import javax.inject.Inject

class GaplessHeaderService {
    @Inject lateinit var db: GaplessDb

    init {
        DaggerServiceComponent.builder()
            .appComponent(Application.appComponent)
            .build().inject(this)
    }
}