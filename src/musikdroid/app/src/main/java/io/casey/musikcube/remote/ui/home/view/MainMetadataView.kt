package io.casey.musikcube.remote.ui.home.view

import android.content.Context
import android.content.SharedPreferences
import android.graphics.Color
import android.graphics.drawable.Drawable
import android.text.SpannableStringBuilder
import android.text.TextPaint
import android.text.method.LinkMovementMethod
import android.text.style.ClickableSpan
import android.text.style.ForegroundColorSpan
import android.util.AttributeSet
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import android.widget.ImageView
import android.widget.TextView
import androidx.annotation.AttrRes
import com.bumptech.glide.load.DataSource
import com.bumptech.glide.load.engine.DiskCacheStrategy
import com.bumptech.glide.load.engine.GlideException
import com.bumptech.glide.request.RequestListener
import com.bumptech.glide.request.RequestOptions
import com.bumptech.glide.request.target.Target
import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.injection.DaggerViewComponent
import io.casey.musikcube.remote.injection.GlideApp
import io.casey.musikcube.remote.service.playback.IPlaybackService
import io.casey.musikcube.remote.service.playback.PlaybackServiceFactory
import io.casey.musikcube.remote.service.playback.PlaybackState
import io.casey.musikcube.remote.service.playback.impl.remote.Metadata
import io.casey.musikcube.remote.service.playback.impl.streaming.StreamingPlaybackService
import io.casey.musikcube.remote.service.websocket.Messages
import io.casey.musikcube.remote.service.websocket.SocketMessage
import io.casey.musikcube.remote.service.websocket.WebSocketService
import io.casey.musikcube.remote.ui.albums.activity.AlbumBrowseActivity
import io.casey.musikcube.remote.ui.settings.constants.Prefs
import io.casey.musikcube.remote.ui.shared.extension.fallback
import io.casey.musikcube.remote.ui.shared.extension.getColorCompat
import io.casey.musikcube.remote.ui.shared.util.Size
import io.casey.musikcube.remote.ui.tracks.activity.TrackListActivity
import org.json.JSONArray
import javax.inject.Inject
import io.casey.musikcube.remote.ui.shared.util.AlbumArtLookup.getUrl as getAlbumArtUrl

class MainMetadataView : FrameLayout {
    @Inject lateinit var wss: WebSocketService
    private lateinit var prefs: SharedPreferences

    private var paused = true
    private lateinit var title: TextView
    private lateinit var artist: TextView
    private lateinit var album: TextView
    private lateinit var volume: TextView
    private lateinit var titleWithArt: TextView
    private lateinit var artistAndAlbumWithArt: TextView
    private lateinit var volumeWithArt: TextView
    private lateinit var mainTrackMetadataWithAlbumArt: View
    private lateinit var mainTrackMetadataNoAlbumArt: View
    private lateinit var buffering: View
    private lateinit var albumArtImageView: ImageView

    private enum class DisplayMode {
        Artwork, NoArtwork, Stopped
    }

    private var lastDisplayMode = DisplayMode.Stopped
    private var loadedAlbumArtUrl: String? = null

    constructor(context: Context) : super(context) {
        init()
    }

    constructor(context: Context, attrs: AttributeSet?) : super(context, attrs) {
        init()
    }

    constructor(context: Context, attrs: AttributeSet?, @AttrRes defStyleAttr: Int) : super(context, attrs, defStyleAttr) {
        init()
    }

    fun onResume() {
        this.wss.addClient(wssClient)
        paused = false
    }

    fun onPause() {
        this.wss.removeClient(wssClient)
        paused = true
    }

    fun clear() {
        if (!paused) {
            loadedAlbumArtUrl = null
            updateAlbumArt()
        }
    }

    fun hide() {
        visibility = View.GONE
    }

    fun refresh() {
        if (!paused) {
            visibility = View.VISIBLE

            val playback = playbackService
            val playing = playbackService.playingTrack

            val buffering = playback.state == PlaybackState.Buffering
            val streaming = playback is StreamingPlaybackService

            val artist = fallback(playing.artist, "")
            val album = fallback(playing.album, "")
            val title = fallback(playing.title, "")

            /* we don't display the volume amount when we're streaming -- the system has
            overlays for drawing volume. */
            if (streaming) {
                volume.visibility = View.GONE
                volumeWithArt.visibility = View.GONE
            }
            else {
                val volume = getString(R.string.status_volume, Math.round(playback.volume * 100))
                this.volume.visibility = View.VISIBLE
                this.volumeWithArt.visibility = View.VISIBLE
                this.volume.text = volume
                this.volumeWithArt.text = volume
            }

            this.title.text = if (title.isEmpty()) getString(if (buffering) R.string.buffering else R.string.unknown_title) else title
            this.artist.text = if (artist.isEmpty()) getString(if (buffering) R.string.buffering else R.string.unknown_artist) else artist
            this.album.text = if (album.isEmpty()) getString(if (buffering) R.string.buffering else R.string.unknown_album) else album

            this.rebindAlbumArtistWithArtTextView(playback)
            this.titleWithArt.text = if (title.isEmpty()) getString(if (buffering) R.string.buffering else R.string.unknown_title) else title
            this.buffering.visibility = if (buffering) View.VISIBLE else View.GONE

            if (artist.isEmpty() || album.isEmpty()) {
                setMetadataDisplayMode(DisplayMode.NoArtwork)
                loadedAlbumArtUrl = null
            }
            else {
                val newUrl = getAlbumArtUrl(playing, Size.Mega) ?: ""
                if (newUrl != loadedAlbumArtUrl) {
                    updateAlbumArt(newUrl)
                }
            }
        }
    }

    private val playbackService: IPlaybackService
        get() = PlaybackServiceFactory.instance(context)

    private fun getString(resId: Int): String {
        return context.getString(resId)
    }

    private fun getString(resId: Int, vararg args: Any): String {
        return context.getString(resId, *args)
    }

    private fun setMetadataDisplayMode(mode: DisplayMode) {
        lastDisplayMode = mode
        when (mode) {
            DisplayMode.Stopped -> {
                albumArtImageView.setImageDrawable(null)
                mainTrackMetadataWithAlbumArt.visibility = View.GONE
                mainTrackMetadataNoAlbumArt.visibility = View.GONE
            }
            DisplayMode.Artwork -> {
                mainTrackMetadataWithAlbumArt.visibility = View.VISIBLE
                mainTrackMetadataNoAlbumArt.visibility = View.GONE
            }
            else -> {
                albumArtImageView.setImageDrawable(null)
                mainTrackMetadataWithAlbumArt.visibility = View.GONE
                mainTrackMetadataNoAlbumArt.visibility = View.VISIBLE
            }
        }
    }

    private fun rebindAlbumArtistWithArtTextView(playback: IPlaybackService) {
        val playing = playback.playingTrack
        val buffering = playback.state == PlaybackState.Buffering

        val artist = fallback(
            playing.artist,
            getString(if (buffering) R.string.buffering else R.string.unknown_artist))

        val album = fallback(
            playing.album,
            getString(if (buffering) R.string.buffering else R.string.unknown_album))

        val albumColor = ForegroundColorSpan(getColorCompat(R.color.theme_orange))

        val artistColor = ForegroundColorSpan(getColorCompat(R.color.theme_yellow))

        val builder = SpannableStringBuilder().append(album).append(" - ").append(artist)

        val albumClickable = object : ClickableSpan() {
            override fun onClick(widget: View) {
                navigateToCurrentAlbum()
            }

            override fun updateDrawState(ds: TextPaint) {}
        }

        val artistClickable = object : ClickableSpan() {
            override fun onClick(widget: View) {
                navigateToCurrentArtist()
            }

            override fun updateDrawState(ds: TextPaint) {}
        }

        val artistOffset = album.length + 3

        builder.setSpan(albumColor, 0, album.length, 0)
        builder.setSpan(albumClickable, 0, album.length, 0)
        builder.setSpan(artistColor, artistOffset, artistOffset + artist.length, 0)
        builder.setSpan(artistClickable, artistOffset, artistOffset + artist.length, 0)

        artistAndAlbumWithArt.movementMethod = LinkMovementMethod.getInstance()
        artistAndAlbumWithArt.highlightColor = Color.TRANSPARENT
        artistAndAlbumWithArt.text = builder
    }

    private fun updateAlbumArt(albumArtUrl: String = "") {
        if (playbackService.state == PlaybackState.Stopped) {
            setMetadataDisplayMode(DisplayMode.NoArtwork)
        }

        if (albumArtUrl.isEmpty()) {
            loadedAlbumArtUrl = null
            setMetadataDisplayMode(DisplayMode.NoArtwork)
        }
        else if (albumArtUrl != loadedAlbumArtUrl || lastDisplayMode == DisplayMode.Stopped) {
            loadedAlbumArtUrl = albumArtUrl

            GlideApp.with(context)
                .load(albumArtUrl)
                .apply(BITMAP_OPTIONS)
                .listener(object: RequestListener<Drawable> {
                    override fun onResourceReady(resource: Drawable?,
                                                 model: Any?,
                                                 target: Target<Drawable>?,
                                                 dataSource: DataSource?,
                                                 isFirstResource: Boolean): Boolean {
                        if (!paused) {
                            preloadNextImage()
                        }

                        setMetadataDisplayMode(DisplayMode.Artwork)
                        return false
                    }

                    override fun onLoadFailed(e: GlideException?,
                                              model: Any?,
                                              target: Target<Drawable>?,
                                              isFirstResource: Boolean): Boolean {
                        setMetadataDisplayMode(DisplayMode.NoArtwork)
                        loadedAlbumArtUrl = null
                        return false
                    }
                })
                .into(albumArtImageView)
        }
        else {
            setMetadataDisplayMode(lastDisplayMode)
        }
    }

    private fun preloadNextImage() {
        val request = SocketMessage.Builder
            .request(Messages.Request.QueryPlayQueueTracks)
            .addOption(Messages.Key.OFFSET, playbackService.queuePosition + 1)
            .addOption(Messages.Key.LIMIT, 1)
            .build()

        this.wss.send(request, wssClient) { response: SocketMessage ->
            val data = response.getJsonArrayOption(Messages.Key.DATA, JSONArray())
            if (data != null && data.length() > 0) {
                val track = data.optJSONObject(0)
                val artist = track.optString(Metadata.Track.ARTIST, "")
                val album = track.optString(Metadata.Track.ALBUM, "")

                val newUrl = getAlbumArtUrl(artist, album, Size.Mega)
                if (loadedAlbumArtUrl != newUrl) {
                    GlideApp.with(context).load(newUrl).apply(BITMAP_OPTIONS).submit(width, height)
                }
            }
        }
    }

    private fun init() {
        DaggerViewComponent.builder()
            .appComponent(Application.appComponent)
            .build().inject(this)

        this.prefs = context.getSharedPreferences(Prefs.NAME, Context.MODE_PRIVATE)

        val child = LayoutInflater.from(context).inflate(R.layout.main_metadata, this, false)

        addView(child, ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT)

        this.title = findViewById(R.id.track_title)
        this.artist = findViewById(R.id.track_artist)
        this.album = findViewById(R.id.track_album)
        this.volume = findViewById(R.id.volume)
        this.buffering = findViewById(R.id.buffering)

        this.titleWithArt = findViewById(R.id.with_art_track_title)
        this.artistAndAlbumWithArt = findViewById(R.id.with_art_artist_and_album)
        this.volumeWithArt = findViewById(R.id.with_art_volume)

        this.mainTrackMetadataWithAlbumArt = findViewById(R.id.main_track_metadata_with_art)
        this.mainTrackMetadataNoAlbumArt = findViewById(R.id.main_track_metadata_without_art)
        this.albumArtImageView = findViewById(R.id.album_art)

        this.album.setOnClickListener { navigateToCurrentAlbum() }
        this.artist.setOnClickListener { navigateToCurrentArtist() }
    }

    private fun navigateToCurrentArtist() {
        val context = context
        val playing = playbackService.playingTrack

        val artistId = playing.artistId
        if (artistId != -1L) {
            val artistName = fallback(playing.artist, "")
            context.startActivity(AlbumBrowseActivity.getStartIntent(
                context, Metadata.Category.ARTIST, artistId, artistName))
        }
    }

    private fun navigateToCurrentAlbum() {
        val context = context
        val playing = playbackService.playingTrack

        val albumId = playing.albumId
        if (albumId != -1L) {
            val albumName = fallback(playing.album, "")
            context.startActivity(TrackListActivity.getStartIntent(
                context, Metadata.Category.ALBUM, albumId, albumName))
        }
    }

    private val wssClient = object : WebSocketService.Client {
        override fun onStateChanged(newState: WebSocketService.State, oldState: WebSocketService.State) {}
        override fun onMessageReceived(message: SocketMessage) {}
        override fun onInvalidPassword() {}
    }

    private companion object {
        val BITMAP_OPTIONS = RequestOptions().diskCacheStrategy(DiskCacheStrategy.ALL)
    }
}
