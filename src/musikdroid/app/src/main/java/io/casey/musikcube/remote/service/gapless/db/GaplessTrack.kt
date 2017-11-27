package io.casey.musikcube.remote.service.gapless.db

import android.arch.persistence.room.Entity
import android.arch.persistence.room.PrimaryKey

@Entity
data class GaplessTrack(@PrimaryKey val id: Long?, val url: String, val state: Int) {
    companion object {
        val DOWNLOADING = 1
        val DOWNLOADED = 2
        val UPDATED = 3
        val ERROR = 4
    }
}