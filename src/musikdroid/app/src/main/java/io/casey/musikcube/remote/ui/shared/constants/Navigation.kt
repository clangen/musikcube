package io.casey.musikcube.remote.ui.shared.constants

import android.app.Activity

object Navigation {
    interface RequestCode {
        companion object {
            val ALBUM_TRACKS_ACTIVITY = 10
            val ALBUM_BROWSE_ACTIVITY = 11
            val CATEGORY_TRACKS_ACTIVITY = 12
        }
    }

    interface ResponseCode {
        companion object {
            val PLAYBACK_STARTED = Activity.RESULT_FIRST_USER + 0xbeef
        }
    }
}
