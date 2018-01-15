package io.casey.musikcube.remote.ui.settings.activity

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.view.Menu
import android.view.MenuItem
import android.widget.ArrayAdapter
import android.widget.SeekBar
import android.widget.Spinner
import android.widget.TextView
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.framework.ViewModel
import io.casey.musikcube.remote.ui.settings.viewmodel.RemoteSettingsViewModel
import io.casey.musikcube.remote.ui.shared.activity.BaseActivity
import io.casey.musikcube.remote.ui.shared.mixin.DataProviderMixin
import io.casey.musikcube.remote.ui.shared.mixin.ViewModelMixin
import io.reactivex.rxkotlin.subscribeBy
import io.casey.musikcube.remote.ui.settings.viewmodel.RemoteSettingsViewModel.State as ViewModelState

class RemoteSettingsActivity: BaseActivity() {

    private lateinit var data: DataProviderMixin
    private lateinit var viewModel: RemoteSettingsViewModel
    private lateinit var driverSpinner: Spinner
    private lateinit var deviceSpinner: Spinner
    private lateinit var replayGainSpinner: Spinner
    private lateinit var preampSeekbar: SeekBar
    private lateinit var preampTextView: TextView
    private lateinit var reindexButton: TextView
    private lateinit var rebuildButton: TextView

    override fun onCreate(savedInstanceState: Bundle?) {
        component.inject(this)

        data = mixin(DataProviderMixin())
        mixin(ViewModelMixin(this))

        super.onCreate(savedInstanceState)

        title = getString(R.string.remote_settings_title)

        setContentView(R.layout.activity_remote_settings)
        driverSpinner = findViewById(R.id.output_driver_spinner)
        deviceSpinner = findViewById(R.id.output_device_spinner)
        replayGainSpinner = findViewById(R.id.replaygain_spinner)
        preampSeekbar = findViewById(R.id.gain_seekbar)
        preampTextView = findViewById(R.id.gain_textview)
        reindexButton = findViewById(R.id.reindex_button)
        rebuildButton = findViewById(R.id.rebuild_button)
        initListeners()

        viewModel = getViewModel()!!
    }

    override fun onResume() {
        super.onResume()
        initObservers()
        viewModel.attach(data.provider)
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        menuInflater.inflate(R.menu.remote_settings_menu, menu)
        return true
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        if (item.itemId == R.id.action_save) {
            saveAndFinish()
        }
        return super.onOptionsItemSelected(item)
    }

    override fun <T : ViewModel<*>> createViewModel(): T? {
        return RemoteSettingsViewModel() as T
    }

    private fun drawNormal() {
        loadValues()
    }

    private fun drawLoading() {

    }

    private fun saveAndFinish() {
        /* TODO */
    }

    private fun loadValues() {
        preampSeekbar.progress = ((viewModel.preampGain + 20.0f) * 100).toInt()
    }

    private fun initListeners() {
        /* metadata */
        reindexButton.setOnClickListener({
            reindexButton.isEnabled = false
            data.provider.reindexMetadata().subscribeBy(
                onNext = { if (!it) reindexButton.isEnabled = true },
                onError = { reindexButton.isEnabled = true }
            )
        })

        rebuildButton.setOnClickListener({
            rebuildButton.isEnabled = false
            data.provider.rebuildMetadata().subscribeBy(
                onNext = { if (!it) rebuildButton.isEnabled = true },
                onError = { rebuildButton.isEnabled = true }
            )
        })

        /* replaygain / preamp */
        val replayGainModes = ArrayAdapter.createFromResource(
            this, R.array.replaygain_mode_array, android.R.layout.simple_spinner_item)

        replayGainModes.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)

        replayGainSpinner.adapter = replayGainModes

        preampSeekbar.setOnSeekBarChangeListener(object:SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(v: SeekBar?, value: Int, user: Boolean) {
                val gain = (value.toFloat() / 100.0) - 20
                preampTextView.text = getString(
                    R.string.remote_settings_preamp_gain_format, gain)
            }

            override fun onStartTrackingTouch(p0: SeekBar?) {
            }

            override fun onStopTrackingTouch(p0: SeekBar?) {
            }
        })

        preampSeekbar.progress = 2000
    }

    private fun initObservers() {
//        disposables.add(data.provider.observeState().subscribeBy(
//            onNext = { },
//            onError = { /* error */ }
//        ))

        disposables.add(viewModel.observe().subscribeBy(
            onNext = {
                when (it) {
                    ViewModelState.Ready -> {
                        drawNormal()
                    }
                    ViewModelState.Disconnected,
                    ViewModelState.Loading,
                    ViewModelState.Saving -> {
                        drawLoading()
                    }
                    ViewModelState.Saved -> {
                        finish()
                    }
                }
            }
        ))
    }

    companion object {
        fun getStartIntent(context: Context):Intent =
            Intent(context, RemoteSettingsActivity::class.java)
    }
}