package io.casey.musikcube.remote.ui.settings.viewmodel

import io.casey.musikcube.remote.service.websocket.model.*
import io.casey.musikcube.remote.util.Strings
import io.reactivex.Observable
import io.reactivex.android.schedulers.AndroidSchedulers
import io.reactivex.functions.BiFunction
import io.reactivex.functions.Function3
import io.reactivex.rxkotlin.subscribeBy

class RemoteSettingsViewModel: BaseRemoteViewModel() {
    private var gain: IGainSettings? = null
    private var outputs: IOutputs? = null
    private var provider: IDataProvider? = null

    var transportType: TransportType = TransportType.Gapless
        private set(value) {
            field = value
        }

    val driverName: String
        get() {
            outputs?.let {
                return it.selectedDriverName
            }
            return ""
        }

    val selectedDriverIndex: Int
        get() {
            var index = 0
            outputs?.let {
                index = it.outputs.indexOfFirst { it.name == driverName }
            }
            return Math.max(0, index)
        }

    val deviceId: String
        get() {
            outputs?.let {
                return it.selectedDeviceId
            }
            return ""
        }

    val selectedDeviceIndex: Int
        get() {
            var deviceIndex = 0
            val driverIndex = selectedDriverIndex
            if (driverIndex >= 0) {
                outputs?.let {
                    if (it.outputs.size > driverIndex) {
                        deviceIndex = it.outputs[driverIndex].devices.indexOfFirst {
                            it.id == deviceId
                        }
                    }
                }
            }
            return Math.max(0, deviceIndex)
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
        provider?.let {
            val oldState = state
            state = State.Saving
            val gainQuery = it.updateGainSettings(replayGainMode, preampGain)
            val transportQuery = it.setTransportType(transport)
            Observable.zip<Boolean, Boolean, Boolean>(
                    gainQuery,
                    transportQuery,
                    BiFunction { b1, b2 -> b1 && b2 })
                .observeOn(AndroidSchedulers.mainThread())
                .subscribeBy(
                    onNext = {
                        if (it) { state = State.Saved }
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
        if (Strings.empty(outputDriver)) {
            save(replayGainMode, preampGain, transport)
        }
        else {
            provider?.let {
                val oldState = state
                state = State.Saving
                val gainQuery = it.updateGainSettings(replayGainMode, preampGain)
                val outputQuery = it.setDefaultOutputDriver(outputDriver, outputDeviceId)
                val transportQuery = it.setTransportType(transport)
                Observable.zip<Boolean, Boolean, Boolean, Boolean>(
                    gainQuery,
                    outputQuery,
                    transportQuery,
                    Function3 { b1, b2, b3 -> b1 && b2 && b3 })
                .observeOn(AndroidSchedulers.mainThread())
                .subscribeBy(
                    onNext = {
                        if (it) { state = State.Saved }
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
}