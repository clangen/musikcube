package io.casey.musikcube.remote.service.gapless.db

import androidx.room.Entity
import androidx.room.PrimaryKey

@Entity
data class GaplessTrack(@PrimaryKey val id: Long?, val url: String, val state: Int) {
    companion object {
        const val DOWNLOADING = 1
        const val DOWNLOADED = 2
        const val UPDATED = 3
        const val ERROR = 4
    }
}