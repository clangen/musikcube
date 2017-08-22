package io.casey.musikcube.remote.db.connections

import android.arch.persistence.room.Entity
import android.arch.persistence.room.PrimaryKey

@Entity
class Connection {
    @PrimaryKey var name: String = ""
    var hostname: String = ""
    var password: String = ""
    var httpPort: Int = 7905
    var wssPort: Int = 7906
    var ssl: Boolean = false
    var noValidate: Boolean = true
}
