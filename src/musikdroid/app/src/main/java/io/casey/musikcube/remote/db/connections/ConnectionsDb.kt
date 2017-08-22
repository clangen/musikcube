package io.casey.musikcube.remote.db.connections

import android.arch.persistence.room.Database
import android.arch.persistence.room.RoomDatabase

@Database(entities = arrayOf(Connection::class), version = 1)
abstract class ConnectionsDb : RoomDatabase() {
    abstract fun connectionsDao(): ConnectionsDao
}
