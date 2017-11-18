package io.casey.musikcube.remote.ui.shared.mixin

import android.app.Activity
import android.app.Dialog
import android.content.DialogInterface
import android.content.Intent
import android.os.Bundle
import android.support.v7.app.AlertDialog
import android.support.v7.app.AppCompatActivity
import android.view.View
import android.widget.PopupMenu
import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.framework.MixinBase
import io.casey.musikcube.remote.injection.DaggerViewComponent
import io.casey.musikcube.remote.injection.DataModule
import io.casey.musikcube.remote.service.playback.PlaybackServiceFactory
import io.casey.musikcube.remote.service.websocket.Messages
import io.casey.musikcube.remote.service.websocket.model.ICategoryValue
import io.casey.musikcube.remote.service.websocket.model.IDataProvider
import io.casey.musikcube.remote.service.websocket.model.ITrack
import io.casey.musikcube.remote.ui.albums.activity.AlbumBrowseActivity
import io.casey.musikcube.remote.ui.category.activity.CategoryBrowseActivity
import io.casey.musikcube.remote.ui.shared.extension.showErrorSnackbar
import io.casey.musikcube.remote.ui.shared.extension.showSnackbar
import io.casey.musikcube.remote.ui.shared.fragment.BaseDialogFragment
import io.casey.musikcube.remote.ui.tracks.activity.TrackListActivity
import io.reactivex.Observable
import io.reactivex.rxkotlin.subscribeBy
import javax.inject.Inject

class ItemContextMenuMixin(private val activity: AppCompatActivity,
                           internal val listener: Listener? = null): MixinBase() {
    @Inject lateinit var provider: IDataProvider

    interface Listener {
        fun onPlaylistDeleted()
        fun onPlaylistCreated()
    }

    private var pendingCode = -1
    private var completion: ((Long) -> Unit)? = null

    init {
        DaggerViewComponent.builder()
            .appComponent(Application.appComponent)
            .dataModule(DataModule())
            .build()
            .inject(this)
    }

    override fun onCreate(bundle: Bundle) {
        super.onCreate(bundle)
        ConfirmDeletePlaylistDialog.rebind(activity, listener)
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

    fun addToPlaylist(track: ITrack) {
        addToPlaylist(listOf(track))
    }

    fun addToPlaylist(tracks: List<ITrack>) {
        showPlaylistChooser { id ->
            addWithErrorHandler(provider.appendToPlaylist(id, tracks))
        }
    }

    fun addToPlaylist(categoryType: String, categoryId: Long) {
        showPlaylistChooser { id ->
            addWithErrorHandler(provider.appendToPlaylist(id, categoryType, categoryId))
        }
    }

    fun addToPlaylist(category: ICategoryValue) {
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
            CategoryBrowseActivity.NavigationType.Select,
            activity.getString(R.string.playlist_edit_pick_playlist))

        activity.startActivityForResult(intent, pendingCode)
    }

    fun showForTrack(track: ITrack, anchorView: View) {
        val popup = PopupMenu(activity, anchorView)
        popup.inflate(R.menu.generic_item_context_menu)

        popup.menu.removeItem(R.id.menu_show_tracks)

        popup.setOnMenuItemClickListener { item ->
            val intent: Intent? = when (item.itemId) {
                R.id.menu_add_to_playlist -> {
                    addToPlaylist(track)
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

    fun showForPlaylist(playlistName: String, playlistId: Long, anchorView: View) {
        val popup = PopupMenu(activity, anchorView)
        popup.inflate(R.menu.playlist_item_context_menu)

        popup.setOnMenuItemClickListener { item ->
            when (item.itemId) {
                R.id.menu_playlist_delete -> {
                    ConfirmDeletePlaylistDialog.show(activity, listener, playlistName, playlistId)
                }
                R.id.menu_playlist_play -> {
                    val playback = PlaybackServiceFactory.instance(Application.instance!!)
                    playback.play(Messages.Category.PLAYLISTS, playlistId)
                }
            }
            true
        }

        popup.show()
    }

    fun showForCategory(value: ICategoryValue, anchorView: View) {
        if (value.type == Messages.Category.PLAYLISTS) {
            showForPlaylist(value.value, value.id, anchorView)
        }
        else {
            val popup = PopupMenu(activity, anchorView)
            popup.inflate(R.menu.generic_item_context_menu)

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
                        addToPlaylist(value)
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
    }

    private fun showSuccess() {
        showSnackbar(
            activity.findViewById(android.R.id.content),
            R.string.playlist_edit_add_success)
    }

    private fun showError(message: Int) {
        showErrorSnackbar(activity.findViewById(android.R.id.content), message)
    }

    class ConfirmDeletePlaylistDialog : BaseDialogFragment() {
        private var listener: Listener? = null

        override fun onCreate(savedInstanceState: Bundle?) {
            mixin(DataProviderMixin())
            super.onCreate(savedInstanceState)
        }

        override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
            val playlistName = arguments.getString(EXTRA_PLAYLIST_NAME, "")

            val dlg = AlertDialog.Builder(activity)
                .setTitle(R.string.playlist_confirm_delete_title)
                .setMessage(getString(R.string.playlist_confirm_delete_message, playlistName))
                .setNegativeButton(R.string.button_no, null)
                .setPositiveButton(R.string.button_yes, positiveListener)
                .create()

            dlg.setCancelable(false)
            return dlg
        }

        private val positiveListener = { dialog: DialogInterface, which: Int ->
            val playlistId = arguments.getLong(EXTRA_PLAYLIST_ID, -1)
            val provider = mixin(DataProviderMixin::class.java)?.provider
            if (provider != null && playlistId != -1L) {
                provider.deletePlaylist(playlistId).subscribeBy(
                    onNext = { listener?.onPlaylistDeleted() },
                    onError = { }
                )}
        }

        companion object {
            val TAG = "confirm_delete_playlist_dialog"
            private val EXTRA_PLAYLIST_ID = "extra_playlist_id"
            private val EXTRA_PLAYLIST_NAME = "extra_playlist_name"

            private fun find(activity: AppCompatActivity): ConfirmDeletePlaylistDialog? =
                activity.supportFragmentManager.findFragmentByTag(TAG) as ConfirmDeletePlaylistDialog?

            fun rebind(activity: AppCompatActivity, listener: Listener?) {
                val dlg = find(activity)
                if (dlg != null) {
                    dlg.listener = listener
                }
            }

            fun show(activity: AppCompatActivity, listener: Listener?, name: String, id: Long) {
                val existing = find(activity)
                existing?.dismiss()

                val args = Bundle()
                args.putString(EXTRA_PLAYLIST_NAME, name)
                args.putLong(EXTRA_PLAYLIST_ID, id)
                val result = ConfirmDeletePlaylistDialog()
                result.arguments = args
                result.listener = listener

                result.show(activity.supportFragmentManager, TAG)
            }
        }
    }

    companion object {
        private val REQUEST_ADD_TO_PLAYLIST = 128
    }
}