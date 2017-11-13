package io.casey.musikcube.remote.data

interface IAlbum : ICategoryValue {
    val albumArtist: String
    val albumArtistId: Long
    val name: String
    val thumbnailId: Long
}