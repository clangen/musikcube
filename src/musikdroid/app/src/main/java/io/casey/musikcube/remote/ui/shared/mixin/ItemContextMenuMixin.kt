package io.casey.musikcube.remote.ui.shared.mixin

import android.app.Activity
import android.content.Intent
import android.view.View
import android.widget.PopupMenu
import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.framework.MixinBase
import io.casey.musikcube.remote.injection.DaggerViewComponent
import io.casey.musikcube.remote.injection.DataModule
import io.casey.musikcube.remote.service.websocket.Messages
import io.casey.musikcube.remote.service.websocket.model.ICategoryValue
import io.casey.musikcube.remote.service.websocket.model.IDataProvider
import io.casey.musikcube.remote.service.websocket.model.ITrack
import io.casey.musikcube.remote.ui.albums.activity.AlbumBrowseActivity
import io.casey.musikcube.remote.ui.category.activity.CategoryBrowseActivity
import io.casey.musikcube.remote.ui.shared.extension.showErrorSnackbar
import io.casey.musikcube.remote.ui.shared.extension.showSnackbar
import io.casey.musikcube.remote.ui.tracks.activity.TrackListActivity
import io.reactivex.Observable
import io.reactivex.rxkotlin.subscribeBy
import javax.inject.Inject

class ItemContextMenuMixin(private val activity: Activity): MixinBase() {
    @Inject lateinit var provider: IDataProvider

    private var pendingCode = -1
    private var completion: ((Long) -> Unit)? = null

    init {
        DaggerViewComponent.builder()
            .appComponent(Application.appComponent)
            .dataModule(DataModule())
            .build()
            .inject(this)
    }

    override fun onResume() {
        super.onResume()
        provider.attach()
    }

    override fun onPause() {
        super.onPause()
        provider.detach()
    }

    override fun onDestroy() {
        super.onDestroy()
        provider.destroy()
    }

    override fun onActivityResult(request: Int, result: Int, data: Intent?) {
        if (pendingCode == request) {
            if (result == Activity.RESULT_OK && data != null) {
                val playlistId = data.getLongExtra(CategoryBrowseActivity.EXTRA_ID, -1L)
                if (playlistId != -1L) {
                    completion?.invoke(playlistId)
                }
            }
            pendingCode = -1
            completion = null
        }

        super.onActivityResult(request, result, data)
    }

    fun add(track: ITrack) {
        add(listOf(track))
    }

    fun add(tracks: List<ITrack>) {
        showPlaylistChooser { id ->
            addWithErrorHandler(provider.appendToPlaylist(id, tracks))
        }
    }

    fun add(categoryType: String, categoryId: Long) {
        showPlaylistChooser { id ->
            addWithErrorHandler(provider.appendToPlaylist(id, categoryType, categoryId))
        }
    }

    fun add(category: ICategoryValue) {
        showPlaylistChooser { id ->
            addWithErrorHandler(provider.appendToPlaylist(id, category))
        }
    }

    private fun addWithErrorHandler(observable: Observable<Boolean>) {
        val error = R.string.playlist_edit_add_error

        observable.subscribeBy(
            onNext = { success -> if (success) showSuccess() else showError(error) },
            onError = { showError(error) })
    }

    private fun showPlaylistChooser(callback: (Long) -> Unit) {
        completion = callback
        pendingCode = REQUEST_ADD_TO_PLAYLIST

        val intent = CategoryBrowseActivity.getStartIntent(
            activity,
            Messages.Category.PLAYLISTS,
            CategoryBrowseActivity.NavigationType.Select)

        activity.startActivityForResult(intent, pendingCode)
    }

    fun showForTrack(track: ITrack, anchorView: View)
    {
        val popup = PopupMenu(activity, anchorView)
        popup.inflate(R.menu.item_context_menu)

        popup.menu.removeItem(R.id.menu_show_tracks)

        popup.setOnMenuItemClickListener { item ->
            val intent: Intent? = when (item.itemId) {
                R.id.menu_add_to_playlist -> {
                    add(track)
                    null
                }
                R.id.menu_show_albums -> {
                    AlbumBrowseActivity.getStartIntent(
                        activity, Messages.Category.ARTIST, track.artistId)
                }
                R.id.menu_show_artists -> {
                    TrackListActivity.getStartIntent(
                    activity,
                    Messages.Category.ARTIST,
                    track.artistId)
                }
                R.id.menu_show_genres -> {
                    CategoryBrowseActivity.getStartIntent(
                        activity,
                        Messages.Category.GENRE,
                        Messages.Category.ARTIST,
                        track.artistId)
                }
                else -> {
                    null
                }
            }

            if (intent != null) {
                activity.startActivity(intent)
            }

            true
        }

        popup.show()
    }

    fun showForCategory(value: ICategoryValue, anchorView: View)
    {
        val popup = PopupMenu(activity, anchorView)
        popup.inflate(R.menu.item_context_menu)

        if (value.type != Messages.Category.GENRE) {
            popup.menu.removeItem(R.id.menu_show_artists)
        }

        when (value.type) {
            Messages.Category.ARTIST -> popup.menu.removeItem(R.id.menu_show_artists)
            Messages.Category.ALBUM -> popup.menu.removeItem(R.id.menu_show_albums)
            Messages.Category.GENRE -> popup.menu.removeItem(R.id.menu_show_genres)
        }

        popup.setOnMenuItemClickListener { item ->
            val intent: Intent? = when (item.itemId) {
                R.id.menu_add_to_playlist -> {
                    add(value)
                    null
                }
                R.id.menu_show_albums -> {
                    AlbumBrowseActivity.getStartIntent(activity, value.type, value.id)
                }
                R.id.menu_show_tracks -> {
                    TrackListActivity.getStartIntent(activity, value.type, value.id)
                }
                R.id.menu_show_genres -> {
                    CategoryBrowseActivity.getStartIntent(
                        activity, Messages.Category.GENRE, value.type, value.id)
                }
                R.id.menu_show_artists -> {
                    CategoryBrowseActivity.getStartIntent(
                        activity, Messages.Category.ARTIST, value.type, value.id)
                }
                else -> {
                    null
                }
            }

            if (intent != null) {
                activity.startActivity(intent)
            }

            true
        }

        popup.show()
    }

    private fun showSuccess() {
        showSnackbar(
            activity.findViewById(android.R.id.content),
            R.string.playlist_edit_add_success)
    }

    private fun showError(message: Int) {
        showErrorSnackbar(activity.findViewById(android.R.id.content), message)
    }

    companion object {
        private val REQUEST_ADD_TO_PLAYLIST = 128
    }
}