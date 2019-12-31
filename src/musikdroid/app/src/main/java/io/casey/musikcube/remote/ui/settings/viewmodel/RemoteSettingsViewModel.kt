package io.casey.musikcube.remote.ui.settings.viewmodel

import io.casey.musikcube.remote.service.websocket.model.*
import io.reactivex.Observable
import io.reactivex.android.schedulers.AndroidSchedulers
import io.reactivex.functions.BiFunction
import io.reactivex.functions.Function3
import io.reactivex.rxkotlin.subscribeBy
import kotlin.math.max

class RemoteSettingsViewModel: BaseRemoteViewModel() {
    private var gain: IGainSettings? = null
    private var outputs: IOutputs? = null

    var transportType: TransportType = TransportType.Gapless
        private set

    val selectedDriverIndex: Int
        get() {
            var index = 0
            outputs?.let {
                index = it.outputs.indexOfFirst { output ->
                    output.name == driverName
                }
            }
            return max(0, index)
        }


    val selectedDeviceIndex: Int
        get() {
            var deviceIndex = 0
            val driverIndex = selectedDriverIndex
            if (driverIndex >= 0) {
                outputs?.let {
                    if (it.outputs.size > driverIndex) {
                        deviceIndex = it.outputs[driverIndex].devices.indexOfFirst { device ->
                            device.id == deviceId
                        }
                    }
                }
            }
            return max(0, deviceIndex)
        }

    val replayGainMode: ReplayGainMode
        get() {
            gain?.let {
                return it.replayGainMode
            }
            return ReplayGainMode.Disabled
        }

    val preampGain: Float
        get() {
            gain?.let {
                return it.preampGain
            }
            return 0.0f
        }

    val outputDrivers: List<IOutput>
        get() {
            outputs?.let {
                return it.outputs
            }
            return listOf()
        }

    fun devicesAt(index: Int): List<IDevice> {
        outputs?.let {
            if (index >= 0 && it.outputs.size > index) {
                return it.outputs[index].devices
            }
        }
        return listOf()
    }

    private fun save(replayGainMode: ReplayGainMode,
                     preampGain: Float,
                     transport: TransportType)
    {
        provider?.let { proxy ->
            val oldState = state
            state = State.Saving
            val gainQuery = proxy.updateGainSettings(replayGainMode, preampGain)
            val transportQuery = proxy.setTransportType(transport)
            Observable.zip<Boolean, Boolean, Boolean>(
                    gainQuery,
                    transportQuery,
                    BiFunction { b1, b2 -> b1 && b2 })
                .observeOn(AndroidSchedulers.mainThread())
                .subscribeBy(
                    onNext = { success ->
                        if (success) { state = State.Saved }
                        state = oldState
                    },
                    onError = { state = oldState })
        }
    }

    fun save(replayGainMode: ReplayGainMode,
             preampGain: Float,
             transport: TransportType,
             outputDriver: String,
             outputDeviceId: String)
    {
        if (outputDriver.isEmpty()) {
            save(replayGainMode, preampGain, transport)
        }
        else {
            provider?.let { proxy ->
                val oldState = state
                state = State.Saving
                val gainQuery = proxy.updateGainSettings(replayGainMode, preampGain)
                val outputQuery = proxy.setDefaultOutputDriver(outputDriver, outputDeviceId)
                val transportQuery = proxy.setTransportType(transport)
                Observable.zip<Boolean, Boolean, Boolean, Boolean>(
                    gainQuery,
                    outputQuery,
                    transportQuery,
                    Function3 { b1, b2, b3 -> b1 && b2 && b3 })
                .observeOn(AndroidSchedulers.mainThread())
                .subscribeBy(
                    onNext = { success ->
                        if (success) { state = State.Saved }
                        state = oldState
                    },
                    onError = { state = oldState })
            }
        }
    }

    override fun onConnected() {
        if (outputs != null && gain != null) {
            state = State.Ready
            return
        }

        provider?.let {
            state = State.Loading
            val gainQuery = it.getGainSettings()
            val outputsQuery = it.listOutputDrivers()
            val transportQuery = it.getTransportType()
            Observable.zip<IGainSettings, IOutputs, TransportType, Boolean>(
                gainQuery,
                outputsQuery,
                transportQuery,
                Function3 { gainSettings, outputs, transportType ->
                    this.gain = gainSettings
                    this.outputs = outputs
                    this.transportType = transportType
                    true
                })
                .observeOn(AndroidSchedulers.mainThread())
                .subscribeBy(
                    onNext = { state = State.Ready },
                    onError = { state = State.Disconnected })
        }
    }

    private val driverName: String
        get() {
            outputs?.let {
                return it.selectedDriverName
            }
            return ""
        }

    private val deviceId: String
        get() {
            outputs?.let {
                return it.selectedDeviceId
            }
            return ""
        }
}