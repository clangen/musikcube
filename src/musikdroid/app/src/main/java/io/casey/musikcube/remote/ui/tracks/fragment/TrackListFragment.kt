package io.casey.musikcube.remote.ui.tracks.fragment

import android.os.Bundle
import io.casey.musikcube.remote.ui.shared.fragment.BaseFragment

class TrackListFragment: BaseFragment() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        component.inject(this)
    }
}