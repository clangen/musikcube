package io.casey.musikcube.remote.ui.playqueue.activity

import android.content.Context
import android.content.Intent
import io.casey.musikcube.remote.ui.navigation.Transition
import io.casey.musikcube.remote.ui.playqueue.constant.PlayQueue
import io.casey.musikcube.remote.ui.playqueue.fragment.PlayQueueFragment
import io.casey.musikcube.remote.ui.shared.activity.FragmentActivityWithTransport
import io.casey.musikcube.remote.ui.shared.fragment.BaseFragment

class PlayQueueActivity : FragmentActivityWithTransport() {
    override fun createContentFragment(): BaseFragment =
        PlayQueueFragment.create(extras.getInt(
            PlayQueue.Extra.PLAYING_INDEX, 0))

    override val contentFragmentTag: String =
        PlayQueueFragment.TAG

    override val transitionType: Transition
        get() = Transition.Vertical

    companion object {
        fun getStartIntent(context: Context, playingIndex: Int): Intent {
            return Intent(context, PlayQueueActivity::class.java)
                .putExtra(PlayQueue.Extra.PLAYING_INDEX, playingIndex)
        }
    }
}
