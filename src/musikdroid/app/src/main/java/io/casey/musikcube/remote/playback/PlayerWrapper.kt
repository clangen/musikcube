package io.casey.musikcube.remote.playback

import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.data.ITrack
import io.casey.musikcube.remote.db.offline.OfflineTrack
import io.casey.musikcube.remote.util.Preconditions
import io.reactivex.Single
import io.reactivex.android.schedulers.AndroidSchedulers
import io.reactivex.schedulers.Schedulers
import org.json.JSONObject
import java.util.*

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

    private var listener: ((PlayerWrapper, State) -> Unit)? = null

    var state = State.Stopped
        protected set(state) {
            if (this.state != state) {
                field = state
                listener?.invoke(this,state)
            }
        }

    abstract fun play(uri: String, metadata: ITrack)
    abstract fun prefetch(uri: String, metadata: ITrack)
    abstract fun pause()
    abstract fun resume()
    abstract fun updateVolume()
    abstract fun setNextMediaPlayer(wrapper: PlayerWrapper?)
    abstract fun dispose()
    abstract var position: Int
    abstract val duration: Int
    abstract val bufferedPercent: Int

    open fun setOnStateChangedListener(listener: ((PlayerWrapper, State) -> Unit)?) {
        Preconditions.throwIfNotOnMainThread()
        this.listener = listener
        this.listener?.invoke(this, this.state)
    }

    companion object {
        private val TYPE = Type.ExoPlayer
        private val DUCK_COEF = 0.2f /* volume = 20% when ducked */
        private val DUCK_NONE = -1.0f

        private val activePlayers = HashSet<PlayerWrapper>()
        private var globalVolume = 1.0f
        private var globalMuted = false
        private var preDuckGlobalVolume = DUCK_NONE

        fun storeOffline(uri: String, metadata: ITrack) {
            Single.fromCallable {
                val track = OfflineTrack()
                if (track.fromJSONObject(uri, metadata.toJson())) {
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
            var computedVolume = volume
            Preconditions.throwIfNotOnMainThread()

            if (preDuckGlobalVolume != DUCK_NONE) {
                preDuckGlobalVolume = volume
                computedVolume *= DUCK_COEF
            }

            if (computedVolume != globalVolume) {
                globalVolume = computedVolume
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
