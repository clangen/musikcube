package io.casey.musikcube.remote.service.websocket.model

import org.json.JSONObject

interface ITrack {
    val id: Long
    val externalId: String
    val uri: String
    val title: String
    val album: String
    val albumId: Long
    val albumArtist: String
    val albumArtistId: Long
    val genre: String
    val trackNum: Int
    val genreId: Long
    val artist: String
    val artistId: Long
    val thumbnailId: Long

    fun getCategoryId(categoryType: String): Long
    fun toJson(): JSONObject
}