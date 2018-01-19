package io.casey.musikcube.remote.ui.category.constant

import android.content.Context
import io.casey.musikcube.remote.R

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

fun categoryToString(context: Context, category: String): String {
    categoryToStringIdMap[category]?.let {
        return context.getString(it)
    }
    return category
}