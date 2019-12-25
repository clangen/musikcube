package io.casey.musikcube.remote.ui.settings.viewmodel

import io.casey.musikcube.remote.service.websocket.model.IEqualizerSettings
import io.casey.musikcube.remote.util.Debouncer
import io.reactivex.rxkotlin.subscribeBy
import io.reactivex.subjects.PublishSubject

class RemoteEqViewModel: BaseRemoteViewModel() {
    private var domainModel: IEqualizerSettings? = null
    private var internalBands = mutableMapOf<Int, Double>()

    val sortedBands: List<Int>
        get() {
            return domainModel?.bands?.keys?.sorted() ?: listOf()
        }

    val bands: Map<Int, Double>
        get() = internalBands

    fun update(hz: Int, db: Double) {
        internalBands[hz] = db
        saveDebouncer.call()
    }

    fun revert() {
        domainModel?.let { internalBands = it.bands }
        bandUpdates.onNext(bands)
        saveDebouncer.call()
    }

    fun zero() {
        val zeroed = mutableMapOf<Int, Double>()
        internalBands.keys.forEach { zeroed[it] = 0.0 }
        internalBands = zeroed
        bandUpdates.onNext(bands)
        saveDebouncer.call()
    }

    var enabled = false
        set(value) {
            if (field != value) {
                field = value
                saveDebouncer.call()
            }
        }

    val bandUpdates = PublishSubject.create<Map<Int, Double>>()

    override fun onConnected() {
        if (domainModel != null) {
            state = State.Ready
        }
        else {
            provider?.let {
                state = State.Loading
                it.getEqualizerSettings().subscribeBy(
                    onNext = { result ->
                        domainModel = result
                        internalBands = result.bands /* makes a copy */
                        enabled = result.enabled
                        state = State.Ready
                    },
                    onError = {
                        state = State.Error
                    }
                )
            }
        }
    }

    private val saveDebouncer = object: Debouncer<Void>(500) {
        override fun onDebounced(last: Void?) {
            provider?.let { proxy ->
                proxy.updateEqualizerSettings(enabled, sortedBands.map {
                    band -> internalBands[band] ?: 0.0
                }.toTypedArray())
            }
        }
    }
}