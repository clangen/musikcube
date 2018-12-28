package io.casey.musikcube.remote.ui.settings.activity

import android.os.Bundle
import android.view.View
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.framework.ViewModel
import io.casey.musikcube.remote.ui.settings.viewmodel.BaseRemoteViewModel
import io.casey.musikcube.remote.ui.settings.viewmodel.RemoteEqViewModel
import io.casey.musikcube.remote.ui.shared.activity.BaseActivity
import io.casey.musikcube.remote.ui.shared.extension.slideThisRight
import io.casey.musikcube.remote.ui.shared.mixin.DataProviderMixin
import io.casey.musikcube.remote.ui.shared.mixin.ViewModelMixin
import io.reactivex.rxkotlin.subscribeBy

class RemoteEqActivity: BaseActivity() {
    private var initialized = false
    private lateinit var data: DataProviderMixin
    private lateinit var viewModel: RemoteEqViewModel
    private lateinit var loadingOverlay: View

    override fun onCreate(savedInstanceState: Bundle?) {
        component.inject(this)
        data = mixin(DataProviderMixin())
        mixin(ViewModelMixin(this))

        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_remote_eq)
        this.loadingOverlay = findViewById(R.id.loading_overlay)

        viewModel = createViewModel()!!

        updateFromViewModelState()
    }

    override fun onResume() {
        super.onResume()
        initObservers()
        viewModel.attach(data.provider)
    }

    override fun finish() {
        super.finish()
        slideThisRight()
    }

    override fun <T : ViewModel<*>> createViewModel(): T? {
        return RemoteEqViewModel() as T
    }

    private fun drawNormal() {
        if (!initialized) {
            initialized = true
        }

        loadingOverlay.visibility = View.GONE
    }

    private fun drawLoading() {
        loadingOverlay.visibility = View.VISIBLE
    }

    private fun updateFromViewModelState(state: BaseRemoteViewModel.State = viewModel.state) {
        when (state) {
            BaseRemoteViewModel.State.Ready -> {
                drawNormal()
            }
            BaseRemoteViewModel.State.Disconnected,
            BaseRemoteViewModel.State.Loading,
            BaseRemoteViewModel.State.Saving -> {
                drawLoading()
            }
            BaseRemoteViewModel.State.Saved -> {
                finish()
            }
        }
    }

    private fun initObservers() {
        disposables.add(viewModel.observe().subscribeBy(
            onNext = { state ->
                updateFromViewModelState(state)
                invalidateOptionsMenu()
            }
        ))
    }
}