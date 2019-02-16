package io.casey.musikcube.remote.ui.navigation

import android.content.Intent
import android.support.v7.app.AppCompatActivity
import io.casey.musikcube.remote.service.playback.impl.remote.Metadata
import io.casey.musikcube.remote.service.websocket.model.IAlbum
import io.casey.musikcube.remote.service.websocket.model.ICategoryValue
import io.casey.musikcube.remote.ui.albums.activity.AlbumBrowseActivity
import io.casey.musikcube.remote.ui.albums.fragment.AlbumBrowseFragment
import io.casey.musikcube.remote.ui.playqueue.activity.PlayQueueActivity
import io.casey.musikcube.remote.ui.playqueue.fragment.PlayQueueFragment
import io.casey.musikcube.remote.ui.shared.extension.pushContainerId
import io.casey.musikcube.remote.ui.shared.extension.pushWithToolbar
import io.casey.musikcube.remote.ui.shared.fragment.BaseFragment
import io.casey.musikcube.remote.ui.tracks.activity.TrackListActivity
import io.casey.musikcube.remote.ui.tracks.fragment.TrackListFragment

object Navigate {
    fun toAlbums(category: String,
                 entry: ICategoryValue,
                 activity: AppCompatActivity,
                 fragment: BaseFragment? = null) =
        when (fragment != null && fragment.pushContainerId > 0) {
            true ->
                fragment.pushWithToolbar(
                    fragment.pushContainerId,
                    "AlbumsBy($entry.value)",
                    AlbumBrowseFragment.create(activity, category, entry.id, entry.value))
            false ->
                activity.startActivity(AlbumBrowseActivity
                    .getStartIntent(activity, category, entry))

        }

    fun toAlbums(activity: AppCompatActivity,
                 fragment: BaseFragment? = null) =
        when (fragment != null && fragment.pushContainerId > 0) {
            true ->
                fragment.pushWithToolbar(
                    fragment.pushContainerId,
                    "AllAlbums",
                    AlbumBrowseFragment.create(activity),
                    Transition.Vertical)
            false ->
                activity.startActivity(AlbumBrowseActivity.getStartIntent(activity))
        }

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

    fun toTracks(category: String,
                 entry: ICategoryValue,
                 activity: AppCompatActivity,
                 fragment: BaseFragment? = null) =
        when (fragment != null && fragment.pushContainerId > 0) {
            true ->
                fragment.pushWithToolbar(
                    fragment.pushContainerId,
                    "TracksBy($entry.value)",
                    TrackListFragment.create(TrackListFragment.arguments(activity, entry.type, entry.id)))
            false ->
                activity.startActivity(TrackListActivity.getStartIntent(
                    activity, category, entry.id, entry.value))
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
}