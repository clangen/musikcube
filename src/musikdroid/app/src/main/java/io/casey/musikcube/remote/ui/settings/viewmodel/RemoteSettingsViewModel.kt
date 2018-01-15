package io.casey.musikcube.remote.ui.settings.viewmodel

import io.casey.musikcube.remote.framework.ViewModel
import io.casey.musikcube.remote.service.websocket.model.*
import io.casey.musikcube.remote.util.Strings
import io.reactivex.Observable
import io.reactivex.android.schedulers.AndroidSchedulers
import io.reactivex.disposables.CompositeDisposable
import io.reactivex.functions.BiFunction
import io.reactivex.rxkotlin.subscribeBy

class RemoteSettingsViewModel: ViewModel<RemoteSettingsViewModel.State>() {
    private var gain: IGainSettings? = null
    private var outputs: IOutputs? = null
    private var provider: IDataProvider? = null
    private var disposables = CompositeDisposable()

    enum class State { Disconnected, Saving, Saved, Loading, Ready }

    fun attach(provider: IDataProvider) {
        this.provider = provider
        this.provider?.let {
            this.disposables.add(it.observeState().subscribeBy(
                onNext = {
                    if (it.first == IDataProvider.State.Connected) {
                        load()
                    }
                },
                onError = {
                    state = State.Disconnected
                }
            ))
        }
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
                    deviceIndex = it.outputs[driverIndex].devices.indexOfFirst { it.id == deviceId }
                }
            }
            return Math.max(0, deviceIndex)
        }

    val replayGainMode: IGainSettings.ReplayGainMode
        get() {
            gain?.let {
                return it.replayGainMode
            }
            return IGainSettings.ReplayGainMode.Disabled
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

    private fun save(replayGainMode: IGainSettings.ReplayGainMode, preampGain: Float)
    {
        provider?.let {
            val oldState = state
            state = State.Saving
            it.updateGainSettings(replayGainMode, preampGain)
                .observeOn(AndroidSchedulers.mainThread())
                .subscribeBy(
                    onNext = {
                        if (it) { state = State.Saved }
                        state = oldState
                    },
                    onError = { state = oldState })
        }
    }

    fun save(replayGainMode: IGainSettings.ReplayGainMode,
             preampGain: Float,
             outputDriver: String,
             outputDeviceId: String)
    {
        if (Strings.empty(outputDriver)) {
            save(replayGainMode, preampGain)
        }
        else {
            provider?.let {
                val oldState = state
                state = State.Saving
                val gainQuery = it.updateGainSettings(replayGainMode, preampGain)
                val outputQuery = it.setDefaultOutputDriver(outputDriver, outputDeviceId)
                Observable.zip<Boolean, Boolean, Boolean>(gainQuery, outputQuery, BiFunction { b1, b2 -> b1 && b2 })
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

    override fun onPause() {
        super.onPause()
        this.provider = null
        this.disposables.dispose()
        this.disposables = CompositeDisposable()
    }

    private fun load() {
        if (outputs != null && gain != null) {
            state = State.Ready
            return
        }

        provider?.let {
            state = State.Loading
            val gainQuery = it.getGainSettings()
            val outputsQuery = it.listOutputDrivers()
            Observable.zip<IGainSettings, IOutputs, Pair<IGainSettings, IOutputs>>(
                gainQuery,
                outputsQuery,
                BiFunction { gainSettings, outputs ->
                    Pair(gainSettings, outputs)
                })
                .observeOn(AndroidSchedulers.mainThread())
                .subscribeBy(
                    onNext = {
                        gain = it.first
                        outputs = it.second
                        state = State.Ready
                    },
                    onError = {
                        state = State.Disconnected
                    })
        }
    }

    private var state: State = State.Disconnected
        set(value) {
            if (state != value) {
                field = value
                publish(state)
            }
        }
}