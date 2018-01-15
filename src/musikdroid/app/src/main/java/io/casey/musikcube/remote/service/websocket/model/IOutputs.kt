package io.casey.musikcube.remote.service.websocket.model

interface IOutputs {
    val outputs: List<IOutput>
    val selectedDriverName: String
    val selectedDeviceId: String
}