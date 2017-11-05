package io.casey.musikcube.remote.data

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

    fun getString(key: String, default: String): String
    fun getLong(key: String, default: Long): Long
    fun toJson(): JSONObject
}