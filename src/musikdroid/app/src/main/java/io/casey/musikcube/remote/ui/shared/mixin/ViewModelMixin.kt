package io.casey.musikcube.remote.ui.shared.mixin

import android.os.Bundle
import io.casey.musikcube.remote.framework.MixinBase
import io.casey.musikcube.remote.framework.ViewModel

class ViewModelMixin(private val provider: ViewModel.Provider): MixinBase() {
    private var viewModel: ViewModel<*>? = null

    fun <T: ViewModel<*>> get(): T? {
        if (viewModel == null) {
            viewModel = provider.createViewModel()
        }
        @Suppress("unchecked_cast")
        return viewModel as T?
    }

    override fun onCreate(bundle: Bundle) {
        super.onCreate(bundle)

        viewModel = ViewModel.restore(bundle.getLong(EXTRA_VIEW_MODEL_ID, -1))

        if (viewModel == null) {
            viewModel = provider.createViewModel()
        }
    }

    override fun onResume() {
        super.onResume()
        viewModel?.onResume()
    }

    override fun onPause() {
        super.onPause()
        viewModel?.onPause()
    }

    override fun onSaveInstanceState(bundle: Bundle) {
        super.onSaveInstanceState(bundle)

        if (viewModel != null) {
            bundle.putLong(EXTRA_VIEW_MODEL_ID, viewModel!!.id)
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        viewModel?.onDestroy()
    }

    companion object {
        const val EXTRA_VIEW_MODEL_ID = "extra_view_model_id"
    }
}