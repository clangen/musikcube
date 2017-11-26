package io.casey.musikcube.remote.service.playback

import android.content.SharedPreferences
import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.injection.*
import io.casey.musikcube.remote.service.playback.impl.player.ExoPlayerWrapper
import io.casey.musikcube.remote.service.playback.impl.player.GaplessExoPlayerWrapper
import io.casey.musikcube.remote.service.playback.impl.player.MediaPlayerWrapper
import io.casey.musikcube.remote.service.playback.impl.streaming.StreamProxy
import io.casey.musikcube.remote.service.playback.impl.streaming.db.OfflineDb
import io.casey.musikcube.remote.service.playback.impl.streaming.db.OfflineTrack
import io.casey.musikcube.remote.service.websocket.model.ITrack
import io.casey.musikcube.remote.ui.settings.constants.Prefs
import io.casey.musikcube.remote.util.Preconditions
import io.reactivex.Single
import io.reactivex.android.schedulers.AndroidSchedulers
import io.reactivex.schedulers.Schedulers
import java.util.*
import javax.inject.Inject

abstract class PlayerWrapper {
    @Inject lateinit var offlineDb: OfflineDb
    @Inject lateinit var streamProxy: StreamProxy

    init {
        DaggerPlaybackComponent.builder()
            .appComponent(Application.appComponent)
            .build().inject(this)
    }

    private enum class Type(prefIndex: Int) {
        ExoPlayer(0), ExoPlayerGapless(1), MediaPlayer(2);

        companion object {
            fun fromPrefIndex(index: Int): Type =
                when(index) {
                    2 -> MediaPlayer
                    1 -> ExoPlayerGapless
                    else -> ExoPlayer
                }
        }
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

    open var state = State.Stopped
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
    abstract val uri: String

    open fun setOnStateChangedListener(listener: ((PlayerWrapper, State) -> Unit)?) {
        Preconditions.throwIfNotOnMainThread()
        this.listener = listener
        this.listener?.invoke(this, this.state)
    }

    protected fun storeOffline(uri: String, metadata: ITrack) {
        Single.fromCallable {
            val track = OfflineTrack()
            if (track.fromJSONObject(uri, metadata.toJson())) {
                offlineDb.trackDao().insertTrack(track)
                offlineDb.prune()
            }
        }
        .subscribeOn(Schedulers.io())
        .observeOn(AndroidSchedulers.mainThread())
        .subscribe()
    }

    companion object {
        private val TYPE = Type.ExoPlayer
        private val DUCK_COEF = 0.2f /* volume = 20% when ducked */
        private val DUCK_NONE = -1.0f

        private val activePlayers = HashSet<PlayerWrapper>()
        private var globalVolume = 1.0f
        private var globalMuted = false
        private var preDuckGlobalVolume = DUCK_NONE

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

        fun newInstance(prefs: SharedPreferences): PlayerWrapper {
            val type = prefs.getInt(
                Prefs.Key.PLAYBACK_ENGINE_INDEX,
                Prefs.Default.PLAYBACK_ENGINE_INDEX)

            return when (Type.fromPrefIndex(type)) {
                Type.ExoPlayer -> ExoPlayerWrapper()
                Type.ExoPlayerGapless -> GaplessExoPlayerWrapper()
                Type.MediaPlayer -> MediaPlayerWrapper()
            }
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
