package io.casey.musikcube.remote.ui.settings.viewmodel

import io.casey.musikcube.remote.service.websocket.model.IEqualizerSettings
import io.reactivex.rxkotlin.subscribeBy

class RemoteEqViewModel: BaseRemoteViewModel() {
    private var domainModel: IEqualizerSettings? = null

    val sortedBands: List<Int>
        get() {
            return domainModel?.bands?.keys?.sorted() ?: listOf()
        }

    val bands: Map<Int, Double>
        get() {
            return domainModel?.bands ?: mapOf()
        }

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
                        state = State.Ready
                    },
                    onError = {
                        state = State.Error
                    }
                )
            }
        }
    }
}