package io.casey.musikcube.remote.service.gapless.db

import android.arch.persistence.room.Database
import android.arch.persistence.room.RoomDatabase

@Database(entities = arrayOf(GaplessTrack::class), version = 1)
abstract class GaplessDb: RoomDatabase() {

}