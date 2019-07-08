package io.casey.musikcube.remote.ui.shared.mixin

import android.app.Activity
import android.app.Dialog
import android.content.DialogInterface
import android.content.Intent
import android.os.Bundle
import android.view.View
import android.widget.EditText
import android.widget.PopupMenu
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.framework.MixinBase
import io.casey.musikcube.remote.injection.DaggerViewComponent
import io.casey.musikcube.remote.service.playback.PlaybackServiceFactory
import io.casey.musikcube.remote.service.playback.impl.remote.Metadata
import io.casey.musikcube.remote.service.websocket.model.IAlbum
import io.casey.musikcube.remote.service.websocket.model.ICategoryValue
import io.casey.musikcube.remote.service.websocket.model.IMetadataProxy
import io.casey.musikcube.remote.service.websocket.model.ITrack
import io.casey.musikcube.remote.ui.category.constant.Category
import io.casey.musikcube.remote.ui.navigation.Navigate
import io.casey.musikcube.remote.ui.shared.extension.hideKeyboard
import io.casey.musikcube.remote.ui.shared.extension.showErrorSnackbar
import io.casey.musikcube.remote.ui.shared.extension.showKeyboard
import io.casey.musikcube.remote.ui.shared.extension.showSnackbar
import io.casey.musikcube.remote.ui.shared.fragment.BaseDialogFragment
import io.casey.musikcube.remote.ui.shared.fragment.BaseFragment
import io.casey.musikcube.remote.ui.tracks.activity.EditPlaylistActivity
import io.reactivex.Observable
import io.reactivex.rxkotlin.subscribeBy
import javax.inject.Inject

class ItemContextMenuMixin(private val activity: AppCompatActivity,
                           internal val listener: EventListener? = null,
                           internal val fragment: BaseFragment? = null): MixinBase() {
    private enum class TrackType { Normal, Playlist }

    @Inject lateinit var provider: IMetadataProxy

    open class EventListener {
        open fun onPlaylistDeleted(id: Long, name: String) { }
        open fun onPlaylistCreated(id: Long, name: String) { }
        open fun onPlaylistUpdated(id: Long, name: String) { }
    }

    private var pendingCode = -1
    private var completion: ((Long, String) -> Unit)? = null

    init {
        DaggerViewComponent.builder()
            .appComponent(Application.appComponent)
            .build().inject(this)
    }

    override fun onCreate(bundle: Bundle) {
        super.onCreate(bundle)
        ConfirmDeletePlaylistDialog.rebind(activity, this)
        EnterPlaylistNameDialog.rebind(activity, this)
        ConfirmRemoveFromPlaylistDialog.rebind(activity, this)
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
                val playlistId = data.getLongExtra(Category.Extra.ID, -1L)
                val playlistName = data.getStringExtra(Category.Extra.NAME)
                if (playlistId != -1L) {
                    completion?.invoke(playlistId, playlistName)
                }
            }
            pendingCode = -1
            completion = null
        }
        else if (result == Activity.RESULT_OK && request == REQUEST_EDIT_PLAYLIST && data != null) {
            val playlistName = data.getStringExtra(EditPlaylistActivity.EXTRA_PLAYLIST_NAME)
            val playlistId = data.getLongExtra(EditPlaylistActivity.EXTRA_PLAYLIST_ID, -1L)

            showSnackbar(
                activity.findViewById(android.R.id.content),
                activity.getString(R.string.playlist_edit_add_success, playlistName),
                activity.getString(R.string.button_view),
                viewPlaylist(playlistId, playlistName))
        }

        super.onActivityResult(request, result, data)
    }

    fun createPlaylist() =
        EnterPlaylistNameDialog.showForCreate(activity, this)

    fun createPlaylist(playlistName: String) {
        @Suppress("ununsed")
        provider.createPlaylist(playlistName).subscribeBy(
            onNext = { id ->
                if (id > 0L) {
                    listener?.onPlaylistCreated(id, playlistName)
                    showSuccess(activity.getString(R.string.playlist_created, playlistName))
                }
                else {
                    showError(activity.getString(R.string.playlist_not_created, playlistName))
                }
            },
            onError = {
                showError(activity.getString(R.string.playlist_not_created, playlistName))
            })
    }

    fun renamePlaylist(newName: String, id: Long) {
        @Suppress("unused")
        provider.renamePlaylist(id, newName).subscribeBy(
            onNext = { success ->
                if (success) {
                    listener?.onPlaylistUpdated(id, newName)
                    showSuccess(activity.getString(R.string.playlist_renamed, newName))
                }
                else {
                    showError(activity.getString(R.string.playlist_not_renamed, newName))
                }
            },
            onError = {
                showError(activity.getString(R.string.playlist_not_renamed, newName))
            })
    }

    private fun addToPlaylist(track: ITrack) =
        addToPlaylist(listOf(track))

    private fun addToPlaylist(tracks: List<ITrack>) {
        showPlaylistChooser { id, name ->
            addWithErrorHandler(id, name, provider.appendToPlaylist(id, tracks))
        }
    }

    private fun addToPlaylist(category: ICategoryValue) {
        showPlaylistChooser { id, name ->
            addWithErrorHandler(id, name, provider.appendToPlaylist(id, category))
        }
    }

    private fun viewPlaylist(playlistId: Long, playlistName: String): ((View) -> Unit) = { _ ->
        Navigate.toTracks(Metadata.Category.PLAYLISTS, playlistId, playlistName, activity, fragment)
    }

    private fun addWithErrorHandler(playlistId: Long, playlistName: String, observable: Observable<Boolean>) {
        val error = R.string.playlist_edit_add_error

        @Suppress("unused")
        observable.subscribeBy(
            onNext = { success ->
                if (success) {
                    listener?.onPlaylistUpdated(playlistId, playlistName)

                    showSuccess(
                        activity.getString(R.string.playlist_edit_add_success, playlistName),
                        activity.getString(R.string.button_view),
                        viewPlaylist(playlistId, playlistName))
                }
                else {
                    showError(error)
                }
            },
            onError = { showError(error) })
    }

    private fun showPlaylistChooser(callback: (Long, String) -> Unit) {
        completion = callback
        pendingCode = REQUEST_ADD_TO_PLAYLIST
        Navigate.toPlaylistChooser(pendingCode, activity, fragment)
    }

    fun showForTrack(track: ITrack, anchorView: View) {
        showForTrack(track, -1, -1, "", anchorView, TrackType.Normal)
    }

    fun showForPlaylistTrack(track: ITrack, position: Int, playlistId: Long, playlistName: String, anchorView: View) {
        showForTrack(track, position, playlistId, playlistName, anchorView, TrackType.Playlist)
    }

    private fun showForTrack(track: ITrack, position: Int, categoryId: Long, categoryValue: String, anchorView: View, type: TrackType) {
        val popup = PopupMenu(activity, anchorView)
        popup.inflate(R.menu.track_item_context_menu)

        if (type != TrackType.Playlist) {
            popup.menu.removeItem(R.id.menu_remove_from_playlist)
        }

        popup.setOnMenuItemClickListener { item ->
            when (item.itemId) {
                R.id.menu_add_to_playlist -> {
                    addToPlaylist(track)
                }
                R.id.menu_remove_from_playlist -> {
                    ConfirmRemoveFromPlaylistDialog.show(
                        activity, this, categoryId, categoryValue, position, track)
                }
                R.id.menu_show_artist_albums -> {
                    Navigate.toAlbums(
                        Metadata.Category.ARTIST,
                        track.artistId,
                        track.artist,
                        activity,
                        fragment)
                }
                R.id.menu_show_artist_tracks -> {
                    Navigate.toTracks(
                        Metadata.Category.ARTIST,
                        track.artistId,
                        track.artist,
                        activity,
                        fragment)
                }
                R.id.menu_download_ringtone -> {
                    Navigate.toDownloadRingtone(track, activity)
                }
            }
            true
        }

        popup.show()
    }

    private fun showForPlaylist(playlistName: String, playlistId: Long, anchorView: View) {
        val popup = PopupMenu(activity, anchorView)
        popup.inflate(R.menu.playlist_item_context_menu)

        popup.setOnMenuItemClickListener { item ->
            when (item.itemId) {
                R.id.menu_playlist_delete -> {
                    ConfirmDeletePlaylistDialog.show(activity, this, playlistName, playlistId)
                }
                R.id.menu_playlist_edit -> {
                    Navigate.toPlaylistEditor(REQUEST_EDIT_PLAYLIST, playlistName, playlistId, activity, fragment)
                }
                R.id.menu_playlist_rename -> {
                    EnterPlaylistNameDialog.showForRename(activity, this, playlistName, playlistId)
                }
                R.id.menu_playlist_play -> {
                    val playback = PlaybackServiceFactory.instance(Application.instance)
                    playback.play(Metadata.Category.PLAYLISTS, playlistId)
                }
            }
            true
        }

        popup.show()
    }

    fun showForCategory(value: ICategoryValue, anchorView: View) {
        if (value.type == Metadata.Category.PLAYLISTS) {
            showForPlaylist(value.value, value.id, anchorView)
        }
        else {
            val menuId = when (value.type) {
                Metadata.Category.ARTIST -> R.menu.artist_item_context_menu
                Metadata.Category.ALBUM_ARTIST -> R.menu.artist_item_context_menu
                Metadata.Category.ALBUM -> R.menu.album_item_context_menu
                Metadata.Category.GENRE -> R.menu.genre_item_context_menu
                else -> -1
            }

            if (menuId != -1) {
                val popup = PopupMenu(activity, anchorView)
                popup.inflate(menuId)

                popup.setOnMenuItemClickListener { item ->
                    when (item.itemId) {
                        R.id.menu_add_to_playlist -> {
                            addToPlaylist(value)
                        }
                        R.id.menu_show_artist_albums,
                        R.id.menu_show_genre_albums -> {
                            if (value is IAlbum) {
                                Navigate.toAlbums(
                                    Metadata.Category.ALBUM_ARTIST,
                                    value.albumArtistId,
                                    value.albumArtist,
                                    activity,
                                    fragment)
                            }
                            else {
                                Navigate.toAlbums(value, activity, fragment)
                            }
                        }
                        R.id.menu_show_artist_tracks,
                        R.id.menu_show_genre_tracks -> {
                            if (value is IAlbum) {
                                Navigate.toTracks(
                                    Metadata.Category.ALBUM_ARTIST,
                                    value.albumArtistId,
                                    value.albumArtist,
                                    activity,
                                    fragment)
                            }
                            else {
                                Navigate.toTracks(value, activity, fragment)
                            }
                        }
                        R.id.menu_show_artist_genres -> {
                            Navigate.toCategoryList(
                                Metadata.Category.GENRE,
                                value.type,
                                value.id,
                                value.value,
                                activity,
                                fragment)
                        }
                        R.id.menu_show_genre_artists -> {
                            Navigate.toCategoryList(
                                Metadata.Category.ARTIST,
                                value.type,
                                value.id,
                                value.value,
                                activity,
                                fragment)
                        }
                    }
                    true
                }

                popup.show()
            }
        }
    }

    private fun deletePlaylistConfirmed(playlistId: Long, playlistName: String) {
        @Suppress("unused")
        if (playlistId != -1L) {
            provider.deletePlaylist(playlistId).subscribeBy(
                onNext = { success ->
                    if (success) {
                        listener?.onPlaylistDeleted(playlistId, playlistName)
                        showSuccess(activity.getString(R.string.playlist_deleted, playlistName))
                    }
                    else {
                        showSuccess(activity.getString(R.string.playlist_not_deleted, playlistName))
                    }
                },
                onError = {
                    showSuccess(activity.getString(R.string.playlist_not_deleted, playlistName))
                })
        }
    }

    private fun removeFromPlaylistConfirmed(playlistId: Long, playlistName: String, externalId: String, position: Int) {
        @Suppress("unused")
        provider
            .removeTracksFromPlaylist(playlistId, listOf(externalId), listOf(position))
            .subscribeBy(
                onNext = {
                    listener?.onPlaylistUpdated(playlistId, playlistName)
                },
                onError = {

                })
    }

    private fun showSuccess(message: String, button: String? = null, cb: ((View) -> Unit)? = null) =
        showSnackbar(activity.findViewById(android.R.id.content), message, buttonText = button, buttonCb = cb)

    private fun showError(message: Int) =
        showError(activity.getString(message))

    private fun showError(message: String) =
        showErrorSnackbar(activity.findViewById(android.R.id.content), message)

    class ConfirmRemoveFromPlaylistDialog : BaseDialogFragment() {
        private lateinit var mixin: ItemContextMenuMixin

        override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
            val args = this.arguments ?: Bundle()
            val playlistId = args.getLong(EXTRA_PLAYLIST_ID, -1)
            val playlistName = args.getString(EXTRA_PLAYLIST_NAME) ?: ""
            val trackTitle = args.getString(EXTRA_TRACK_TITLE, "")
            val trackExternalId = args.getString(EXTRA_TRACK_EXTERNAL_ID, "")
            val trackPosition = args.getInt(EXTRA_TRACK_POSITION, -1)

            return AlertDialog.Builder(activity!!)
                .setTitle(R.string.playlist_confirm_delete_title)
                .setMessage(getString(R.string.playlist_confirm_delete_message, trackTitle))
                .setNegativeButton(R.string.button_no, null)
                .setPositiveButton(R.string.button_yes) { _: DialogInterface, _: Int ->
                    mixin.removeFromPlaylistConfirmed(playlistId, playlistName, trackExternalId, trackPosition)
                }
                .create()
                .apply {
                    setCancelable(false)
                }
        }

        companion object {
            const val TAG = "confirm_delete_playlist_dialog"
            private const val EXTRA_PLAYLIST_ID = "extra_playlist_id"
            private const val EXTRA_PLAYLIST_NAME = "extra_playlist_name"
            private const val EXTRA_TRACK_TITLE = "extra_track_title"
            private const val EXTRA_TRACK_EXTERNAL_ID = "extra_track_external_id"
            private const val EXTRA_TRACK_POSITION = "extra_track_position"

            fun rebind(activity: AppCompatActivity, mixin: ItemContextMenuMixin) {
                find<ConfirmRemoveFromPlaylistDialog>(activity, TAG)?.mixin = mixin
            }

            fun show(activity: AppCompatActivity, mixin: ItemContextMenuMixin, playlistId: Long, playlistName: String, position: Int, track: ITrack) {
                dismiss(activity, TAG)
                val args = Bundle()
                args.putLong(EXTRA_PLAYLIST_ID, playlistId)
                args.putString(EXTRA_PLAYLIST_NAME, playlistName)
                args.putString(EXTRA_TRACK_TITLE, track.title)
                args.putString(EXTRA_TRACK_EXTERNAL_ID, track.externalId)
                args.putInt(EXTRA_TRACK_POSITION, position)
                val result = ConfirmRemoveFromPlaylistDialog()
                result.arguments = args
                result.mixin = mixin
                result.show(activity.supportFragmentManager, TAG)
            }
        }
    }

    class ConfirmDeletePlaylistDialog : BaseDialogFragment() {
        private lateinit var mixin: ItemContextMenuMixin

        override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
            val args = this.arguments ?: Bundle()
            val playlistName = args.getString(EXTRA_PLAYLIST_NAME, "") ?: ""

            val dlg = AlertDialog.Builder(activity!!)
                .setTitle(R.string.playlist_confirm_delete_title)
                .setMessage(getString(R.string.playlist_confirm_delete_message, playlistName))
                .setNegativeButton(R.string.button_no, null)
                .setPositiveButton(R.string.button_yes) { _: DialogInterface, _: Int ->
                    val playlistId = args.getLong(EXTRA_PLAYLIST_ID, -1)
                    mixin.deletePlaylistConfirmed(playlistId, playlistName)
                }
                .create()

            dlg.setCancelable(false)
            return dlg
        }

        companion object {
            const val TAG = "confirm_delete_playlist_dialog"
            private const val EXTRA_PLAYLIST_ID = "extra_playlist_id"
            private const val EXTRA_PLAYLIST_NAME = "extra_playlist_name"

            fun rebind(activity: AppCompatActivity, mixin: ItemContextMenuMixin) {
                find<ConfirmDeletePlaylistDialog>(activity, TAG)?.mixin = mixin
            }

            fun show(activity: AppCompatActivity, mixin: ItemContextMenuMixin, name: String, id: Long) {
                dismiss(activity, TAG)

                val args = Bundle()
                args.putString(EXTRA_PLAYLIST_NAME, name)
                args.putLong(EXTRA_PLAYLIST_ID, id)
                val result = ConfirmDeletePlaylistDialog()
                result.arguments = args
                result.mixin = mixin

                result.show(activity.supportFragmentManager, TAG)
            }
        }
    }

    class EnterPlaylistNameDialog: BaseDialogFragment() {
        enum class Action { Create, Rename }
        private lateinit var mixin: ItemContextMenuMixin

        override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
            val args = arguments ?: Bundle()
            val editText = EditText(activity)
            val action = args.getSerializable(EXTRA_ACTION)
            val name = args.getString(EXTRA_NAME, "")
            val id = args.getLong(EXTRA_ID, -1)

            val buttonId = if (action == Action.Rename)
                R.string.button_rename else R.string.button_create

            if (name.isNotBlank()) {
                editText.setText(name)
                editText.selectAll()
            }

            editText.requestFocus()

            val activity = this.activity!!

            val dlg = AlertDialog.Builder(activity)
                .setTitle(R.string.playlist_name_title)
                    .setNegativeButton(R.string.button_cancel) { _, _ -> hideKeyboard() }
                    .setOnCancelListener { hideKeyboard() }
                    .setPositiveButton(buttonId) { _: DialogInterface, _: Int ->
                    val playlistName = editText.text.toString()
                    if (playlistName.isNotBlank()) {
                        when (action) {
                            Action.Create -> mixin.createPlaylist(playlistName)
                            Action.Rename -> mixin.renamePlaylist(playlistName, id)
                        }
                    }
                    else {
                        mixin.showError(R.string.playlist_name_error_empty)
                    }
                }
                .create()

            val paddingX = activity.resources.getDimensionPixelSize(R.dimen.edit_text_dialog_padding_x)
            val paddingY = activity.resources.getDimensionPixelSize(R.dimen.edit_text_dialog_padding_y)
            dlg.setView(editText, paddingX, paddingY, paddingX, paddingY)

            dlg.setCancelable(false)
            return dlg
        }

        override fun onResume() {
            super.onResume()
            showKeyboard()
        }

        override fun onPause() {
            super.onPause()
            hideKeyboard()
        }

        companion object {
            const val TAG = "EnterPlaylistNameDialog"
            private const val EXTRA_ACTION = "extra_action"
            private const val EXTRA_NAME = "extra_name"
            private const val EXTRA_ID = "extra_id"

            fun rebind(activity: AppCompatActivity, mixin: ItemContextMenuMixin) {
                find<EnterPlaylistNameDialog>(activity, TAG)?.mixin = mixin
            }

            fun showForCreate(activity: AppCompatActivity, mixin: ItemContextMenuMixin) {
                dismiss(activity, TAG)
                val bundle = Bundle()
                bundle.putSerializable(EXTRA_ACTION, Action.Create)
                show(activity, mixin, bundle)
            }

            fun showForRename(activity: AppCompatActivity, mixin: ItemContextMenuMixin, name: String, id: Long) {
                dismiss(activity, TAG)
                val bundle = Bundle()
                bundle.putSerializable(EXTRA_ACTION, Action.Rename)
                bundle.putString(EXTRA_NAME, name)
                bundle.putLong(EXTRA_ID, id)
                show(activity, mixin, bundle)
            }

            private fun show(activity: AppCompatActivity, mixin: ItemContextMenuMixin, bundle: Bundle) {
                dismiss(activity, TAG)
                val dialog = EnterPlaylistNameDialog()
                dialog.arguments = bundle
                dialog.mixin = mixin
                dialog.show(activity.supportFragmentManager, TAG)
            }
        }
    }

    companion object {
        private const val REQUEST_ADD_TO_PLAYLIST = 32
        private const val REQUEST_EDIT_PLAYLIST = 33
    }
}