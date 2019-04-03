package io.casey.musikcube.remote.ui.shared.mixin

import android.content.Context
import android.content.SharedPreferences
import android.os.Bundle
import android.view.KeyEvent
import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.framework.MixinBase
import io.casey.musikcube.remote.service.playback.IPlaybackService
import io.casey.musikcube.remote.service.playback.PlaybackServiceFactory
import io.casey.musikcube.remote.ui.settings.constants.Prefs

class PlaybackMixin(var listener: (() -> Unit)? = { }): MixinBase() {
    private lateinit var prefs: SharedPreferences
    private val context = Application.instance

    var service: IPlaybackService = PlaybackServiceFactory.instance(context)
        private set

    override fun onCreate(bundle: Bundle) {
        super.onCreate(bundle)
        prefs = context.getSharedPreferences(Prefs.NAME, Context.MODE_PRIVATE)
        connect()
    }

    override fun onPause() {
        super.onPause()
        disconnect()
    }

    override fun onResume() {
        super.onResume()
        reload()
    }

    fun connectAll() {
        connect(PlaybackServiceFactory.streaming(context))
        connect(PlaybackServiceFactory.remote(context))
    }

    fun reload() {
        if (active) {
            disconnect()
            connect()
        }
    }

    fun onKeyDown(keyCode: Int): Boolean {
        if (!streaming) {
            if (keyCode == KeyEvent.KEYCODE_VOLUME_DOWN) {
                service.volumeDown()
                return true
            }
            else if (keyCode == KeyEvent.KEYCODE_VOLUME_UP) {
                service.volumeUp()
                return true
            }
        }
        return false
    }

    val streaming: Boolean
        get() {
            return prefs.getBoolean(
                Prefs.Key.STREAMING_PLAYBACK,
                Prefs.Default.STREAMING_PLAYBACK)
        }

    private fun connect() {
        service = PlaybackServiceFactory.instance(context)
        connect(service)
    }

    private fun disconnect() {
        disconnect(service)
    }

    private fun connect(service: IPlaybackService) {
        val listener = this.listener
        if (listener != null) {
            service.connect(listener)
        }
    }

    private fun disconnect(service: IPlaybackService) {
        val listener = this.listener
        if (listener != null) {
            service.disconnect(listener)
        }
    }
}