package io.casey.musikcube.remote.ui.settings.model

import android.os.Parcel
import android.os.Parcelable
import androidx.room.Entity
import androidx.room.PrimaryKey

@Entity
class Connection : Parcelable {
    @PrimaryKey var name: String = ""
    var hostname: String = ""
    var password: String = ""
    var httpPort: Int = 7905
    var wssPort: Int = 7906
    var ssl: Boolean = false
    var noValidate: Boolean = true

    constructor()

    constructor(source: Parcel) {
        name = source.readString() ?: ""
        hostname = source.readString() ?: ""
        password = source.readString() ?: ""
        httpPort = source.readInt()
        wssPort = source.readInt()
        ssl = (source.readInt() == 1)
        noValidate = (source.readInt() == 1)
    }

    val valid: Boolean
        get() {
            return name.isNotBlank() && hostname.isNotEmpty() && httpPort > 0 && wssPort > 0
        }

    override fun writeToParcel(parcel: Parcel?, flags: Int) {
        if (parcel != null) {
            parcel.writeString(name)
            parcel.writeString(hostname)
            parcel.writeString(password)
            parcel.writeInt(httpPort)
            parcel.writeInt(wssPort)
            parcel.writeInt(if (ssl) 1 else 0)
            parcel.writeInt(if (noValidate) 1 else 0)
        }
    }

    override fun describeContents(): Int {
        return 0
    }

    companion object {
        @Suppress("unused")
        @JvmField
        val CREATOR: Parcelable.Creator<Connection> = object: Parcelable.Creator<Connection> {
            override fun createFromParcel(source: Parcel?): Connection {
                return Connection(source!!)
            }

            override fun newArray(size: Int): Array<Connection?> {
                return arrayOfNulls(size)
            }
        }
    }
}
