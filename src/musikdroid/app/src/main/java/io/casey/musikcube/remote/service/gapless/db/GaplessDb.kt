package io.casey.musikcube.remote.service.gapless.db

import androidx.room.Database
import androidx.room.RoomDatabase
import io.reactivex.Single
import io.reactivex.android.schedulers.AndroidSchedulers
import io.reactivex.schedulers.Schedulers

@Database(entities = [GaplessTrack::class], version = 1)
abstract class GaplessDb: RoomDatabase() {
    abstract fun dao(): GaplessDao

    fun prune() {
        @Suppress("unused")
        Single.fromCallable {
            dao().deleteByState(GaplessTrack.DOWNLOADING)
            true
        }
        .subscribeOn(Schedulers.io())
        .observeOn(AndroidSchedulers.mainThread())
        .subscribe({ }, { })
    }
}