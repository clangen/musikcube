package io.casey.musikcube.remote.ui.settings.activity

import android.os.Bundle
import android.view.View
import android.widget.CheckBox
import android.widget.SeekBar
import android.widget.TableLayout
import android.widget.TextView
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.framework.ViewModel
import io.casey.musikcube.remote.ui.settings.viewmodel.BaseRemoteViewModel
import io.casey.musikcube.remote.ui.settings.viewmodel.RemoteEqViewModel
import io.casey.musikcube.remote.ui.shared.activity.BaseActivity
import io.casey.musikcube.remote.ui.shared.mixin.MetadataProxyMixin
import io.casey.musikcube.remote.ui.shared.mixin.ViewModelMixin
import io.reactivex.rxkotlin.subscribeBy

class RemoteEqActivity: BaseActivity() {
    private lateinit var data: MetadataProxyMixin
    private lateinit var viewModel: RemoteEqViewModel
    private lateinit var loadingOverlay: View
    private lateinit var table: TableLayout
    private lateinit var revertButton: View
    private lateinit var zeroButton: View
    private lateinit var enabledCb: CheckBox

    override fun onCreate(savedInstanceState: Bundle?) {
        component.inject(this)
        data = mixin(MetadataProxyMixin())
        mixin(ViewModelMixin(this))

        title = getString(R.string.remote_settings_eq_title)

        super.onCreate(savedInstanceState)
        setContentView(R.layout.remote_eq_activity)
        this.loadingOverlay = findViewById(R.id.loading_overlay)
        this.table = findViewById(R.id.eq_table)
        this.revertButton = findViewById(R.id.button_revert)
        this.zeroButton = findViewById(R.id.button_zero)
        this.enabledCb = findViewById(R.id.enabled_checkbox)
        initListeners()

        viewModel = createViewModel()!!

        updateFromViewModelState()
    }

    override fun onResume() {
        super.onResume()
        initObservers()
        viewModel.attach(data.provider)
    }

    override fun <T : ViewModel<*>> createViewModel(): T? {
        @Suppress("unchecked_cast")
        return RemoteEqViewModel() as T
    }

    private fun initListeners() {
        revertButton.setOnClickListener { viewModel.revert() }
        zeroButton.setOnClickListener { viewModel.zero() }
        enabledCb.setOnCheckedChangeListener { _, checked -> viewModel.enabled = checked }
    }

    private fun drawNormal() {
        enabledCb.isChecked = viewModel.enabled
        rebuildSeekbars()
        loadingOverlay.visibility = View.GONE
    }

    private fun rebuildSeekbars() {
        table.removeAllViews()
        viewModel.sortedBands.forEach { hz ->
            val row = layoutInflater.inflate(R.layout.eq_band_row, table, false)
            val value = (viewModel.bands[hz] ?: 0.0).toInt()
            val progress = (value + 20.0).toInt()
            val valueText = row.findViewById<TextView>(R.id.value)
            row.tag = hz
            row.findViewById<TextView>(R.id.label).text = getString(R.string.remote_settings_eq_label_format, hz)
            row.findViewById<SeekBar>(R.id.seekbar).apply {
                this.progress = progress
                this.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                    override fun onProgressChanged(seekBar: SeekBar, progress: Int, fromUser: Boolean) {
                        val db = progress - 20
                        valueText.text = getString(R.string.remote_settings_eq_value_format, db)
                        viewModel.update(hz, db.toDouble())
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

        disposables.add(viewModel.bandUpdates.subscribeBy(
            onNext = { rebuildSeekbars() }
        ))
    }
}