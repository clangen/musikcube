package io.casey.musikcube.remote.service.gapless.db

import android.arch.persistence.room.Database
import android.arch.persistence.room.RoomDatabase
import io.reactivex.Single
import io.reactivex.android.schedulers.AndroidSchedulers
import io.reactivex.schedulers.Schedulers

@Database(entities = arrayOf(GaplessTrack::class), version = 1)
abstract class GaplessDb: RoomDatabase() {
    abstract fun dao(): GaplessDao

    fun prune() {
        Single.fromCallable {
            dao().deleteByState(GaplessTrack.DOWNLOADING)
            true
        }
        .subscribeOn(Schedulers.io())
        .observeOn(AndroidSchedulers.mainThread())
        .subscribe({ _ -> }, { })
    }
}