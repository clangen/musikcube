package io.casey.musikcube.remote.service.websocket.model

interface IAlbum : ICategoryValue {
    val artist: String
    val artistId: Long
    val albumArtist: String
    val albumArtistId: Long
    val name: String
    val thumbnailId: Long
}