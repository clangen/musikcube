package io.casey.musikcube.remote.ui.tracks.activity

import android.content.Context
import android.content.Intent
import android.view.Menu
import android.view.MenuItem
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.service.playback.impl.remote.Metadata
import io.casey.musikcube.remote.ui.shared.activity.FragmentActivityWithTransport
import io.casey.musikcube.remote.ui.shared.fragment.BaseFragment
import io.casey.musikcube.remote.ui.tracks.constant.Track
import io.casey.musikcube.remote.ui.tracks.fragment.TrackListFragment

class TrackListActivity: FragmentActivityWithTransport() {
    private val tracks
        get() = content as TrackListFragment

    override fun onCreateOptionsMenu(menu: Menu): Boolean =
        tracks.createOptionsMenu(menu)

    override fun onOptionsItemSelected(item: MenuItem): Boolean =
        when (tracks.optionsItemSelected(item)) {
            true -> true
            false -> super.onOptionsItemSelected(item)
        }

    override fun createContentFragment(): BaseFragment = TrackListFragment.create(intent)
    override val contentFragmentTag: String = TrackListFragment.TAG

    companion object {
        fun getOfflineStartIntent(context: Context): Intent =
            getStartIntent(context, Metadata.Category.OFFLINE, 0).apply {
                putExtra(Track.Extra.TITLE_ID, R.string.offline_tracks_title)
            }

        fun getStartIntent(context: Context,
                           categoryType: String = "",
                           categoryId: Long = 0,
                           categoryValue: String = ""): Intent =
            Intent(context, TrackListActivity::class.java).apply {
                putExtra(
                    Track.Extra.FRAGMENT_ARGUMENTS,
                    TrackListFragment.arguments(context, categoryType, categoryId, categoryValue))
            }
    }
}
