package io.casey.musikcube.remote.service.websocket.model

interface IEnvironment {
    val apiVersion: Int
    val sdkVersion: Int
    val serverVersion: String
}