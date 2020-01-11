package io.casey.musikcube.remote.service.playback

import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.injection.DaggerPlaybackComponent
import io.casey.musikcube.remote.service.gapless.GaplessHeaderService
import io.casey.musikcube.remote.service.gapless.db.GaplessDb
import io.casey.musikcube.remote.service.gapless.db.GaplessTrack
import io.casey.musikcube.remote.service.playback.impl.player.GaplessExoPlayerWrapper
import io.casey.musikcube.remote.service.playback.impl.streaming.StreamProxy
import io.casey.musikcube.remote.service.playback.impl.streaming.db.OfflineDb
import io.casey.musikcube.remote.service.playback.impl.streaming.db.OfflineTrack
import io.casey.musikcube.remote.service.websocket.model.ITrack
import io.casey.musikcube.remote.util.Preconditions
import io.reactivex.Single
import io.reactivex.android.schedulers.AndroidSchedulers
import io.reactivex.rxkotlin.subscribeBy
import io.reactivex.schedulers.Schedulers
import java.util.*
import javax.inject.Inject

abstract class PlayerWrapper {
    @Inject lateinit var offlineDb: OfflineDb
    @Inject lateinit var gaplessDb: GaplessDb
    @Inject lateinit var streamProxy: StreamProxy
    @Inject lateinit var gaplessService: GaplessHeaderService

    init {
        DaggerPlaybackComponent.builder()
            .appComponent(Application.appComponent)
            .build().inject(this)
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

    abstract fun play(uri: String, metadata: ITrack, offsetMs: Int = 0)
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
        @Suppress
        Single.fromCallable {
            val track = OfflineTrack()
            if (track.fromJSONObject(uri, metadata.toJson())) {
                offlineDb.trackDao().insertTrack(track)
                offlineDb.prune()
            }

            gaplessDb.dao().queryByUrl(uri).forEach {
                if (it.state != GaplessTrack.UPDATED) {
                    gaplessDb.dao().update(GaplessTrack.DOWNLOADED, it.url)
                    gaplessService.schedule()
                }
            }
        }
        .subscribeOn(Schedulers.io())
        .observeOn(AndroidSchedulers.mainThread())
        .subscribeBy(onError = { })
    }

    companion object {
        private const val DUCK_COEF = 0.2f /* volume = 20% when ducked */
        private const val DUCK_NONE = -1.0f

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

            if (globalMuted != muted) {
                globalMuted = muted
                for (w in activePlayers) {
                    w.updateVolume()
                }
            }
        }

        fun newInstance(): PlayerWrapper = GaplessExoPlayerWrapper()

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
