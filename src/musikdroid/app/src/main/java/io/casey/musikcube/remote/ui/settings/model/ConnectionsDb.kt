package io.casey.musikcube.remote.ui.settings.model

import androidx.room.Database
import androidx.room.RoomDatabase

@Database(entities = [Connection::class], version = 1)
abstract class ConnectionsDb : RoomDatabase() {
    abstract fun connectionsDao(): ConnectionsDao
}
