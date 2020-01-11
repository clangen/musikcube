package io.casey.musikcube.remote.ui.navigation

import android.content.Intent
import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityOptionsCompat
import androidx.core.util.Pair
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.service.playback.impl.remote.Metadata
import io.casey.musikcube.remote.service.websocket.model.IAlbum
import io.casey.musikcube.remote.service.websocket.model.ICategoryValue
import io.casey.musikcube.remote.service.websocket.model.ITrack
import io.casey.musikcube.remote.ui.albums.activity.AlbumBrowseActivity
import io.casey.musikcube.remote.ui.albums.fragment.AlbumBrowseFragment
import io.casey.musikcube.remote.ui.browse.activity.BrowseActivity
import io.casey.musikcube.remote.ui.category.activity.AllCategoriesActivity
import io.casey.musikcube.remote.ui.category.activity.CategoryBrowseActivity
import io.casey.musikcube.remote.ui.category.constant.NavigationType
import io.casey.musikcube.remote.ui.category.fragment.CategoryBrowseFragment
import io.casey.musikcube.remote.ui.download.activity.TrackDownloadActivity
import io.casey.musikcube.remote.ui.home.activity.MainActivity
import io.casey.musikcube.remote.ui.playqueue.activity.PlayQueueActivity
import io.casey.musikcube.remote.ui.playqueue.fragment.PlayQueueFragment
import io.casey.musikcube.remote.ui.shared.extension.*
import io.casey.musikcube.remote.ui.shared.fragment.BaseFragment
import io.casey.musikcube.remote.ui.tracks.activity.EditPlaylistActivity
import io.casey.musikcube.remote.ui.tracks.activity.TrackListActivity
import io.casey.musikcube.remote.ui.tracks.fragment.TrackListFragment

object Navigate {
    /*
     *
     * browse root
     *
     */

    fun toBrowse(activity: AppCompatActivity,
                 initialCategoryType: String = "") =
        activity.startActivity(
            BrowseActivity.getStartIntent(activity, initialCategoryType),
            createMainTransition(activity))

    /*
     *
     * list of categories
     *
     */

    fun toAllCategories(activity: AppCompatActivity) =
        activity.startActivity(AllCategoriesActivity.getStartIntent(activity))

    /*
     *
     * list of albums
     *
     */
    fun toAlbums(categoryType: String,
                 categoryEntry: ICategoryValue,
                 activity: AppCompatActivity,
                 fragment: BaseFragment? = null) =
        toAlbums(categoryType, categoryEntry.id, categoryEntry.value, activity, fragment)

    fun toAlbums(categoryEntry: ICategoryValue,
                 activity: AppCompatActivity,
                 fragment: BaseFragment? = null) =
        toAlbums(categoryEntry.type, categoryEntry.id, categoryEntry.value, activity, fragment)

    fun toAlbums(categoryType: String,
                 categoryId: Long,
                 categoryValue: String,
                 activity: AppCompatActivity,
                 fragment: BaseFragment? = null) =
            when (fragment != null && fragment.pushContainerId > 0) {
                true ->
                    fragment.pushWithToolbar(
                        fragment.pushContainerId,
                        "AlbumsBy($categoryType-$categoryId-$categoryValue)",
                        AlbumBrowseFragment.create(activity, categoryType, categoryId, categoryValue))
                false ->
                    activity.startActivity(AlbumBrowseActivity
                        .getStartIntent(activity, categoryType, categoryId, categoryValue))
            }

    /*
     *
     * single album
     *
     */

    fun toAlbum(album: IAlbum,
                activity: AppCompatActivity,
                fragment: BaseFragment? = null) =
        when (fragment != null && fragment.pushContainerId > 0) {
            true ->
                fragment.pushWithToolbar(
                    fragment.pushContainerId,
                    "TracksForAlbum($album.id)",
                    TrackListFragment.create(
                        TrackListFragment.arguments(
                            activity, Metadata.Category.ALBUM, album.id, album.value)))
            false ->
                activity.startActivity(
                    TrackListActivity.getStartIntent(
                        activity, Metadata.Category.ALBUM, album.id, album.value))
            }

    /*
     *
     * list of categories
     *
     */

    fun toCategoryList(targetType: String,
                       sourceType: String,
                       sourceId: Long,
                       sourceValue: String,
                       activity: AppCompatActivity,
                       fragment: BaseFragment? = null) =
        when (fragment != null && fragment.pushContainerId > 0) {
            true ->
                fragment.pushWithToolbar(
                    fragment.pushContainerId,
                    "${targetType}By($sourceType-$sourceId-$sourceValue)",
                    CategoryBrowseFragment.create(activity, targetType, sourceType, sourceId, sourceValue))
            false ->
                activity.startActivity(CategoryBrowseActivity
                    .getStartIntent(activity, targetType, sourceType, sourceId, sourceValue))
        }

    fun toCategoryList(targetType: String,
                       activity: AppCompatActivity,
                       fragment: BaseFragment? = null) =
            when (fragment != null && fragment.pushContainerId > 0) {
                true ->
                    fragment.pushWithToolbar(
                        fragment.pushContainerId,
                        "$targetType)",
                        CategoryBrowseFragment.create(activity, targetType))
                false ->
                    activity.startActivity(CategoryBrowseActivity.getStartIntent(activity, targetType))
            }

    /*
     *
     * playlist-related
     *
     */

    fun toPlaylistChooser(requestCode: Int,
                          activity: AppCompatActivity,
                          fragment: BaseFragment? = null) =
        CategoryBrowseActivity.getStartIntent(
                activity,
                Metadata.Category.PLAYLISTS,
                NavigationType.Select,
                activity.getString(R.string.playlist_edit_pick_playlist))
            .withoutTransport()
            .withTransitionType(Transition.Vertical)
            .apply {
                startActivityForResult(this, requestCode, activity, fragment)
            }

    fun toPlaylistEditor(requestCode: Int,
                         playlistName: String,
                         playlistId: Long,
                         activity: AppCompatActivity,
                         fragment: BaseFragment? = null) =
        EditPlaylistActivity.getStartIntent(activity, playlistName, playlistId).apply {
            startActivityForResult(this, requestCode, activity, fragment)
        }

    /*
     *
     * list of tracks
     *
     */

    fun toTracks(categoryType: String,
                 categoryEntry: ICategoryValue,
                 activity: AppCompatActivity,
                 fragment: BaseFragment? = null) =
        toTracks(categoryType, categoryEntry.id, categoryEntry.value, activity, fragment)

    fun toTracks(categoryEntry: ICategoryValue,
                 activity: AppCompatActivity,
                 fragment: BaseFragment? = null) =
        toTracks(categoryEntry.type, categoryEntry.id, categoryEntry.value, activity, fragment)

    fun toTracks(categoryType: String,
                 categoryId: Long,
                 categoryValue: String,
                 activity: AppCompatActivity,
                 fragment: BaseFragment? = null) =
        when (fragment != null && fragment.pushContainerId > 0) {
            true ->
                fragment.pushWithToolbar(
                    fragment.pushContainerId,
                    "TracksBy($categoryType-$categoryId-$categoryValue)",
                    TrackListFragment.create(TrackListFragment.arguments(
                        activity, categoryType, categoryId, categoryValue)))
            false ->
                activity.startActivity(TrackListActivity.getStartIntent(
                    activity, categoryType, categoryId, categoryValue))
        }

    fun toPlayQueue(playingIndex: Int,
                    activity: AppCompatActivity,
                    fragment: BaseFragment? = null) {
        when (fragment != null && fragment.pushContainerId > 0) {
            true ->
                fragment.pushWithToolbar(
                    fragment.pushContainerId,
                    PlayQueueFragment.TAG,
                    PlayQueueFragment.create(playingIndex),
                    Transition.Vertical)
            false ->
                activity.startActivity(PlayQueueActivity
                    .getStartIntent(activity, playingIndex)
                    .addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP))
        }
    }

    /*
     *
     * download-related stuff
     *
     */

    fun toDownloadRingtone(track: ITrack, activity: AppCompatActivity) =
        activity.startActivity(TrackDownloadActivity.getStartIntent(activity, track))

    /*
     *
     * transition stuff
     *
     */

    private fun createMainTransition(activity: AppCompatActivity) =
        when (activity is MainActivity) {
            true ->
                ActivityOptionsCompat.makeSceneTransitionAnimation(
                    activity,
                    Pair(activity.findViewById(R.id.PlayControls), "play_controls_transition"),
//                    Pair(activity.findViewById(R.id.middle_container), "middle_content_transition"),
//                    Pair(activity.findViewById(R.id.button_play_queue), "play_queue_transition"),
                    Pair(activity.findViewById(R.id.toolbar), "toolbar_transition")).toBundle()
            false ->
                Bundle()
        }
}