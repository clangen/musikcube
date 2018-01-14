package io.casey.musikcube.remote.service.websocket.model

interface IOutput {
    val name: String
    val devices: List<IDevice>
}