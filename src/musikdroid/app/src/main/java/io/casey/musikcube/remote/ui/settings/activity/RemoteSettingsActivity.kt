package io.casey.musikcube.remote.ui.settings.activity

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.view.*
import android.widget.*
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.framework.ViewModel
import io.casey.musikcube.remote.service.websocket.model.IDevice
import io.casey.musikcube.remote.service.websocket.model.IOutput
import io.casey.musikcube.remote.service.websocket.model.ReplayGainMode
import io.casey.musikcube.remote.service.websocket.model.TransportType
import io.casey.musikcube.remote.ui.navigation.Transition
import io.casey.musikcube.remote.ui.settings.viewmodel.RemoteSettingsViewModel
import io.casey.musikcube.remote.ui.shared.activity.BaseActivity
import io.casey.musikcube.remote.ui.shared.mixin.MetadataProxyMixin
import io.casey.musikcube.remote.ui.shared.mixin.ViewModelMixin
import io.reactivex.rxkotlin.subscribeBy
import io.casey.musikcube.remote.ui.settings.viewmodel.BaseRemoteViewModel.State as ViewModelState

class RemoteSettingsActivity: BaseActivity() {
    private var initialized = false
    private lateinit var data: MetadataProxyMixin
    private lateinit var viewModel: RemoteSettingsViewModel
    private lateinit var loadingOverlay: View
    private lateinit var driverSpinner: Spinner
    private lateinit var deviceSpinner: Spinner
    private lateinit var transportSpinner: Spinner
    private lateinit var replayGainSpinner: Spinner
    private lateinit var preampSeekbar: SeekBar
    private lateinit var preampTextView: TextView
    private lateinit var configureEq: TextView
    private lateinit var reindexButton: TextView
    private lateinit var rebuildButton: TextView

    override fun onCreate(savedInstanceState: Bundle?) {
        component.inject(this)

        data = mixin(MetadataProxyMixin())
        mixin(ViewModelMixin(this))

        super.onCreate(savedInstanceState)

        title = getString(R.string.remote_settings_title)

        setContentView(R.layout.remote_settings_activity)
        loadingOverlay = findViewById(R.id.loading_overlay)
        driverSpinner = findViewById(R.id.output_driver_spinner)
        deviceSpinner = findViewById(R.id.output_device_spinner)
        transportSpinner = findViewById(R.id.transport_spinner)
        replayGainSpinner = findViewById(R.id.replaygain_spinner)
        preampSeekbar = findViewById(R.id.gain_seekbar)
        preampTextView = findViewById(R.id.gain_textview)
        configureEq = findViewById(R.id.configure_eq_button)
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
            save()
        }
        return super.onOptionsItemSelected(item)
    }

    override fun onPrepareOptionsMenu(menu: Menu?): Boolean {
        menu?.findItem(R.id.action_save)?.isEnabled =
            viewModel.state == ViewModelState.Ready

        return super.onPrepareOptionsMenu(menu)
    }

    override fun <T : ViewModel<*>> createViewModel(): T? {
        @Suppress("unchecked_cast")
        return RemoteSettingsViewModel() as T
    }

    override val transitionType: Transition
        get() = Transition.Vertical

    private fun drawNormal() {
        if (!initialized) {
            preampSeekbar.progress = ((viewModel.preampGain + 20.0f) * 100).toInt()
            driverSpinner.onItemSelectedListener = null
            driverSpinner.adapter = DriverAdapter(viewModel.outputDrivers)
            driverSpinner.setSelection(viewModel.selectedDriverIndex)
            driverSpinner.onItemSelectedListener = driverChangeListener
            deviceSpinner.adapter = DevicesAdapter(viewModel.devicesAt(viewModel.selectedDriverIndex))
            deviceSpinner.setSelection(viewModel.selectedDeviceIndex)
            transportSpinner.setSelection(TRANSPORT_TYPE_TO_INDEX.getValue(viewModel.transportType))
            replayGainSpinner.setSelection(REPLAYGAIN_MODE_TO_INDEX.getValue(viewModel.replayGainMode))
            initialized = true
        }

        loadingOverlay.visibility = View.GONE
    }

    private fun drawLoading() {
        loadingOverlay.visibility = View.VISIBLE
    }

    private fun save() {
        val replayGainMode = indexToReplayGain(replayGainSpinner.selectedItemPosition)
        val preampGain = (preampSeekbar.progress.toFloat() / 100.0f) - 20.0f

        var driverName = ""
        var deviceId = ""

        val selectedDriver = driverSpinner.selectedItemPosition
        val selectedDevice = deviceSpinner.selectedItemPosition
        if (selectedDriver >= 0 && driverSpinner.adapter.count > selectedDriver) {
            driverName = viewModel.outputDrivers[selectedDriver].name

            if (selectedDevice >= 0 && deviceSpinner.adapter.count > selectedDevice) {
                deviceId = viewModel.devicesAt(selectedDriver)[selectedDevice].id
            }
        }

        val transport = indexToTransportType(transportSpinner.selectedItemPosition)

        viewModel.save(replayGainMode, preampGain, transport, driverName, deviceId)
    }

    private fun initListeners() {
        /* metadata */
        reindexButton.setOnClickListener {
            reindexButton.isEnabled = false
            data.provider.reindexMetadata().subscribeBy(
                onNext = { if (!it) reindexButton.isEnabled = true },
                onError = { reindexButton.isEnabled = true }
            )
        }

        rebuildButton.setOnClickListener {
            rebuildButton.isEnabled = false
            data.provider.rebuildMetadata().subscribeBy(
                onNext = { if (!it) rebuildButton.isEnabled = true },
                onError = { rebuildButton.isEnabled = true }
            )
        }

        /* devices */
        driverSpinner.onItemSelectedListener = driverChangeListener

        /* replaygain / preamp */
        val replayGainModes = ArrayAdapter.createFromResource(
            this, R.array.replaygain_mode_array,
                android.R.layout.simple_spinner_dropdown_item)

        replayGainModes.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        replayGainSpinner.adapter = replayGainModes

        preampSeekbar.setOnSeekBarChangeListener(object:SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(v: SeekBar?, value: Int, user: Boolean) {
                val gain = (value.toFloat() / 100.0) - 20
                preampTextView.text = getString(
                    R.string.remote_settings_preamp_gain_format, gain)
            }

            override fun onStartTrackingTouch(view: SeekBar?) {
            }

            override fun onStopTrackingTouch(view: SeekBar?) {
            }
        })

        preampSeekbar.progress = 2000

        /* equalizer */
        configureEq.setOnClickListener {
            val intent = Intent(this, RemoteEqActivity::class.java)
            startActivity(intent)
        }

        /* transport */
        val transportModes = ArrayAdapter.createFromResource(
            this, R.array.transport_type_array,
            android.R.layout.simple_spinner_dropdown_item)

        transportModes.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        transportSpinner.adapter = transportModes
    }

    private fun initObservers() {
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
                    else -> {
                    }
                }
                invalidateOptionsMenu()
            }
        ))
    }

    private val driverChangeListener = object:AdapterView.OnItemSelectedListener {
        override fun onNothingSelected(parent: AdapterView<*>?) {
        }

        override fun onItemSelected(parent: AdapterView<*>?, view: View?, pos: Int, id: Long) {
            deviceSpinner.adapter = DevicesAdapter(viewModel.devicesAt(driverSpinner.selectedItemPosition))
            if (pos == viewModel.selectedDriverIndex) {
                deviceSpinner.setSelection(viewModel.selectedDeviceIndex)
            }
            else {
                deviceSpinner.setSelection(0)
            }
        }
    }

    private class DriverAdapter(private val outputs: List<IOutput>): BaseAdapter() {
        override fun getView(pos: Int, view: View?, parent: ViewGroup?): View {
            var bind = view
            if (view == null) {
                bind = LayoutInflater.from(parent?.context).inflate(
                    android.R.layout.simple_spinner_dropdown_item, parent, false)
            }
            (bind as TextView).text = outputs[pos].name
            return bind
        }

        override fun getItem(pos: Int): Any = outputs[pos]
        override fun getItemId(pos: Int): Long = pos.toLong()
        override fun getCount(): Int = outputs.size
    }

    private class DevicesAdapter(private val devices: List<IDevice>): BaseAdapter() {
        override fun getView(pos: Int, view: View?, parent: ViewGroup?): View {
            var bind = view
            if (view == null) {
                bind = LayoutInflater.from(parent?.context).inflate(
                    android.R.layout.simple_spinner_dropdown_item, parent, false)
            }
            (bind as TextView).text = devices[pos].name
            return bind
        }

        override fun getItem(pos: Int): Any = devices[pos]
        override fun getItemId(pos: Int): Long = pos.toLong()
        override fun getCount(): Int = devices.size
    }

    companion object {
        val REPLAYGAIN_MODE_TO_INDEX = mapOf(
            ReplayGainMode.Disabled to 0,
                ReplayGainMode.Track to 1,
                ReplayGainMode.Album to 2)

        val TRANSPORT_TYPE_TO_INDEX = mapOf(
        TransportType.Gapless to 0,
            TransportType.Crossfade to 1)

        fun indexToReplayGain(index: Int): ReplayGainMode {
            REPLAYGAIN_MODE_TO_INDEX.forEach {
                if (it.value == index) {
                    return it.key
                }
            }
            return ReplayGainMode.Disabled
        }

        fun indexToTransportType(index: Int): TransportType {
            TRANSPORT_TYPE_TO_INDEX.forEach {
                if (it.value == index) {
                    return it.key
                }
            }
            return TransportType.Gapless
        }

        fun getStartIntent(context: Context):Intent =
            Intent(context, RemoteSettingsActivity::class.java)
    }
}