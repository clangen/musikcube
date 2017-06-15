package io.casey.musikcube.remote.playback

import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.offline.OfflineTrack
import java.util.HashSet

import io.casey.musikcube.remote.util.Preconditions
import io.reactivex.Single
import io.reactivex.android.schedulers.AndroidSchedulers
import io.reactivex.schedulers.Schedulers
import org.json.JSONObject

abstract class PlayerWrapper {
    private enum class Type {
        MediaPlayer, ExoPlayer
    }

    enum class State {
        Stopped,
        Preparing,
        Prepared,
        Playing,
        Buffering,
        Paused,
        Error,
        Finished,
        Killing,
        Disposed
    }

    interface OnStateChangedListener {
        fun onStateChanged(mpw: PlayerWrapper, state: State)
    }

    private var listener: OnStateChangedListener? = null

    var state = State.Stopped
        protected set(state) {
            if (this.state != state) {
                field = state
                if (listener != null) {
                    this.listener!!.onStateChanged(this, state)
                }
            }
        }

    abstract fun play(uri: String, metadata: JSONObject)
    abstract fun prefetch(uri: String, metadata: JSONObject)
    abstract fun pause()
    abstract fun resume()
    abstract fun updateVolume()
    abstract fun setNextMediaPlayer(wrapper: PlayerWrapper?)
    abstract fun dispose()
    abstract var position: Int
    abstract val duration: Int
    abstract val bufferedPercent: Int

    open fun setOnStateChangedListener(listener: OnStateChangedListener?) {
        Preconditions.throwIfNotOnMainThread()

        this.listener = listener

        if (listener != null) {
            this.listener!!.onStateChanged(this, this.state)
        }
    }

    companion object {
        private val TYPE = Type.ExoPlayer
        private val DUCK_COEF = 0.2f /* volume = 20% when ducked */
        private val DUCK_NONE = -1.0f

        private val activePlayers = HashSet<PlayerWrapper>()
        private var globalVolume = 1.0f
        private var globalMuted = false
        private var preDuckGlobalVolume = DUCK_NONE

        fun storeOffline(uri: String, json: JSONObject) {
            Single.fromCallable {
                val track = OfflineTrack()
                if (track.fromJSONObject(uri, json)) {
                    Application.offlineDb?.trackDao()?.insertTrack(track)
                    Application.offlineDb?.prune()
                }
            }
            .subscribeOn(Schedulers.io())
            .observeOn(AndroidSchedulers.mainThread())
            .subscribe()
        }

        fun duck() {
            Preconditions.throwIfNotOnMainThread()

            if (preDuckGlobalVolume == DUCK_NONE) {
                val lastVolume = globalVolume
                setVolume(globalVolume * DUCK_COEF)
                preDuckGlobalVolume = lastVolume
            }
        }

        fun unduck() {
            Preconditions.throwIfNotOnMainThread()

            if (preDuckGlobalVolume != DUCK_NONE) {
                val temp = preDuckGlobalVolume
                preDuckGlobalVolume = DUCK_NONE
                setVolume(temp)
            }
        }

        fun setVolume(volume: Float) {
            var volume = volume
            Preconditions.throwIfNotOnMainThread()

            if (preDuckGlobalVolume != DUCK_NONE) {
                preDuckGlobalVolume = volume
                volume *= DUCK_COEF
            }

            if (volume != globalVolume) {
                globalVolume = volume
                for (w in activePlayers) {
                    w.updateVolume()
                }
            }
        }

        fun getVolume(): Float {
            Preconditions.throwIfNotOnMainThread()

            if (globalMuted) {
                return 0f
            }

            return if (preDuckGlobalVolume == DUCK_NONE) globalVolume else preDuckGlobalVolume
        }

        fun setMute(muted: Boolean) {
            Preconditions.throwIfNotOnMainThread()

            if (PlayerWrapper.globalMuted != muted) {
                PlayerWrapper.globalMuted = muted

                for (w in activePlayers) {
                    w.updateVolume()
                }
            }
        }

        fun newInstance(): PlayerWrapper {
            return if (TYPE == Type.ExoPlayer)
                ExoPlayerWrapper() else MediaPlayerWrapper()
        }

        fun addActivePlayer(player: PlayerWrapper) {
            Preconditions.throwIfNotOnMainThread()
            activePlayers.add(player)
        }

        fun removeActivePlayer(player: PlayerWrapper) {
            Preconditions.throwIfNotOnMainThread()
            activePlayers.remove(player)
        }
    }
}
