package io.casey.musikcube.remote.framework

import android.content.Intent
import android.os.Bundle

abstract class MixinBase : IMixin {
    enum class State {
        Unknown, Created, Started, Resumed, Paused, Stopped, Destroyed
    }

    protected var state = State.Unknown
        private set

    protected val active
        get() = state == State.Resumed

    override fun onCreate(bundle: Bundle) {
        state = State.Created
    }

    override fun onStart() {
        state = State.Started
    }

    override fun onResume() {
        state = State.Resumed
    }

    override fun onPause() {
        state = State.Paused
    }

    override fun onStop() {
        state = State.Stopped
    }

    override fun onActivityResult(request: Int, result: Int, data: Intent?) {
    }

    override fun onSaveInstanceState(bundle: Bundle) {
    }

    override fun onDestroy() {
        state = State.Destroyed
    }
}