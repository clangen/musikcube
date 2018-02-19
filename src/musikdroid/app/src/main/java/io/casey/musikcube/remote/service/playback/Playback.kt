package io.casey.musikcube.remote.service.playback

import android.content.Context
import io.casey.musikcube.remote.service.playback.impl.streaming.StreamingPlaybackService
import io.casey.musikcube.remote.ui.shared.mixin.PlaybackMixin

sealed class Playback {
    enum class SwitchMode { Transfer, Copy, Swap }

    companion object {
        fun transferPlayback(context: Context, mode: SwitchMode) {
            transferPlayback(context, PlaybackMixin(), mode)
        }

        fun transferPlayback(context: Context, playback: PlaybackMixin, mode: SwitchMode) {
            val isStreaming = playback.service is StreamingPlaybackService

            if (mode == SwitchMode.Swap) {
                if (isStreaming) {
                    playback.service.pause()
                }
            }
            else {
                playback.connectAll()

                val streaming = PlaybackServiceFactory.streaming(context)
                val remote = PlaybackServiceFactory.remote(context)

                if (!isStreaming) {
                    streaming.playFrom(remote)
                    if (mode == SwitchMode.Transfer) {
                        remote.pause()
                    }
                }
                else {
                    remote.playFrom(streaming)
                    if (mode == SwitchMode.Transfer) {
                        streaming.pause()
                    }
                }
            }

            playback.reload()
        }
    }
}

