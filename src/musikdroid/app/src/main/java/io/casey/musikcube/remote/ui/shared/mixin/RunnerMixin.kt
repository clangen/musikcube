package io.casey.musikcube.remote.ui.shared.mixin

import android.os.Bundle
import com.uacf.taskrunner.Runner
import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.framework.MixinBase

class RunnerMixin(private val callbacks: Runner.TaskCallbacks,
                  private val callingType: Class<Any>): MixinBase()
{
    lateinit var runner: Runner
        private set

    override fun onCreate(bundle: Bundle) {
        super.onCreate(bundle)
        this.runner = Runner.attach(Application.instance, callingType, callbacks, bundle, null)
    }

    override fun onResume() {
        super.onResume()
        runner.resume()
    }

    override fun onPause() {
        super.onPause()
        runner.pause()
    }

    override fun onSaveInstanceState(bundle: Bundle) {
        super.onSaveInstanceState(bundle)
        runner.saveState(bundle)
    }

    override fun onDestroy() {
        super.onDestroy()
        runner.detach(callbacks)
    }
}

