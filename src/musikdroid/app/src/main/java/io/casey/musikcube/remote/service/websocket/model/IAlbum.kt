package io.casey.musikcube.remote.service.websocket.model

interface IAlbum : ICategoryValue {
    val albumArtist: String
    val albumArtistId: Long
    val name: String
    val thumbnailId: Long
}