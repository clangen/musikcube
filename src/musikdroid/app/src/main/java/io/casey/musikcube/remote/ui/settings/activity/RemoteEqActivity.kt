package io.casey.musikcube.remote.ui.settings.activity

import android.os.Bundle
import android.view.View
import android.widget.SeekBar
import android.widget.TableLayout
import android.widget.TextView
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
    private lateinit var table: TableLayout

    override fun onCreate(savedInstanceState: Bundle?) {
        component.inject(this)
        data = mixin(DataProviderMixin())
        mixin(ViewModelMixin(this))

        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_remote_eq)
        this.loadingOverlay = findViewById(R.id.loading_overlay)
        this.table = findViewById(R.id.eq_table)

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
            table.removeAllViews()
            viewModel.sortedBands.forEach { hz ->
                val row = layoutInflater.inflate(R.layout.eq_band_row, table, false)
                val value = (viewModel.bands[hz] ?: 0.0).toInt()
                val progress = (value + 20.0).toInt()
                val valueText = row.findViewById<TextView>(R.id.value)
                row.findViewById<TextView>(R.id.label).text = getString(R.string.remote_settings_eq_label_format, hz)
                row.findViewById<SeekBar>(R.id.seekbar).apply {
                    this.progress = progress
                    this.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                        override fun onProgressChanged(seekBar: SeekBar, progress: Int, fromUser: Boolean) {
                            valueText.text = getString(R.string.remote_settings_eq_value_format, progress - 20)
                        }

                        override fun onStartTrackingTouch(seekBar: SeekBar) {
                        }

                        override fun onStopTrackingTouch(seekBar: SeekBar) {
                        }
                    })
                }
                valueText.text = getString(R.string.remote_settings_eq_value_format, value)
                table.addView(row)
            }
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
            BaseRemoteViewModel.State.Error,
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