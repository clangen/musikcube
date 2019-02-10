package io.casey.musikcube.remote.ui.category.constant

import android.content.Context
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.service.websocket.Messages

private val categoryToStringIdMap = mapOf(
    "album" to R.string.category_artist,
    "album_artist" to R.string.category_album_artist,
    "artist" to R.string.category_artist,
    "bitrate" to R.string.category_bitrate,
    "comment" to R.string.category_comment,
    "composer" to R.string.category_composer,
    "conductor" to R.string.category_conductor,
    "copyright" to R.string.category_copyright,
    "genre" to R.string.category_genre,
    "publisher" to R.string.category_publisher,
    "year" to R.string.category_year)

object Category {
    val NAME_TO_TITLE: Map<String, Int> = mapOf(
        Messages.Category.ALBUM_ARTIST to R.string.artists_title,
        Messages.Category.GENRE to R.string.genres_title,
        Messages.Category.ARTIST to R.string.artists_title,
        Messages.Category.ALBUM to R.string.albums_title,
        Messages.Category.PLAYLISTS to R.string.playlists_title)

    val NAME_TO_RELATED_TITLE: Map<String, Int> = mapOf(
        Messages.Category.ALBUM_ARTIST to R.string.artists_from_category,
        Messages.Category.GENRE to R.string.genres_from_category,
        Messages.Category.ARTIST to R.string.artists_from_category,
        Messages.Category.ALBUM to R.string.albums_by_title)

    val NAME_TO_EMPTY_TYPE: Map<String, Int> = mapOf(
        Messages.Category.ALBUM_ARTIST to R.string.browse_type_artists,
        Messages.Category.GENRE to R.string.browse_type_genres,
        Messages.Category.ARTIST to R.string.browse_type_artists,
        Messages.Category.ALBUM to R.string.browse_type_albums,
        Messages.Category.PLAYLISTS to R.string.browse_type_playlists)

    fun toDisplayString(context: Context, category: String): String {
        categoryToStringIdMap[category]?.let {
            return context.getString(it)
        }
        return category
    }

    object Extra {
        const val CATEGORY = "extra_category"
        const val ID = "extra_id"
        const val NAME = "extra_name"
        const val PREDICATE_TYPE = "extra_predicate_type"
        const val PREDICATE_ID = "extra_predicate_id"
        const val NAVIGATION_TYPE = "extra_navigation_type"
        const val TITLE = "extra_title"
        const val FRAGMENT_ARGUMENTS = "fragment_arguments"
    }
}

