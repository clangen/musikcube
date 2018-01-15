package io.casey.musikcube.remote.service.websocket.model.impl.remote

import io.casey.musikcube.remote.service.websocket.model.IOutput
import io.casey.musikcube.remote.service.websocket.model.IOutputs

class RemoteOutputs(private val driverName: String,
                    private val deviceId: String,
                    private val allOutputs: List<IOutput>): IOutputs
{
    override val outputs: List<IOutput>
        get() = allOutputs
    override val selectedDriverName: String
        get() = driverName
    override val selectedDeviceId: String
        get() = deviceId
}
