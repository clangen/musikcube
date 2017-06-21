package io.casey.musikcube.remote.ui.activity

import android.app.Dialog
import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.net.Uri
import android.os.Bundle
import android.support.v4.app.DialogFragment
import android.support.v7.app.AlertDialog
import android.support.v7.app.AppCompatActivity
import android.view.Menu
import android.view.MenuItem
import android.view.View
import android.widget.*
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.playback.PlaybackServiceFactory
import io.casey.musikcube.remote.playback.PlayerWrapper
import io.casey.musikcube.remote.playback.StreamProxy
import io.casey.musikcube.remote.ui.extension.enableUpNavigation
import io.casey.musikcube.remote.ui.extension.setCheckWithoutEvent
import io.casey.musikcube.remote.ui.extension.setTextAndMoveCursorToEnd
import io.casey.musikcube.remote.websocket.Prefs
import io.casey.musikcube.remote.websocket.WebSocketService
import java.util.*
import io.casey.musikcube.remote.websocket.Prefs.Default as Defaults
import io.casey.musikcube.remote.websocket.Prefs.Key as Keys

class SettingsActivity : AppCompatActivity() {
    private var addressText: EditText? = null
    private var portText: EditText? = null
    private var httpPortText: EditText? = null
    private var passwordText: EditText? = null
    private var albumArtCheckbox: CheckBox? = null
    private var messageCompressionCheckbox: CheckBox? = null
    private var softwareVolume: CheckBox? = null
    private var sslCheckbox: CheckBox? = null
    private var certCheckbox: CheckBox? = null
    private var playbackModeSpinner: Spinner? = null
    private var bitrateSpinner: Spinner? = null
    private var cacheSpinner: Spinner? = null
    private var prefs: SharedPreferences? = null
    private var wasStreaming: Boolean = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        prefs = this.getSharedPreferences(Prefs.NAME, Context.MODE_PRIVATE)
        setContentView(R.layout.activity_settings)
        setTitle(R.string.settings_title)
        wasStreaming = isStreamingEnabled
        bindEventListeners()
        rebindUi()
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        menuInflater.inflate(R.menu.settings_menu, menu)
        return true
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        if (item.itemId == android.R.id.home) {
            finish()
            return true
        } else if (item.itemId == R.id.action_save) {
            save()
            return true
        }

        return super.onOptionsItemSelected(item)
    }

    private fun rebindUi() {
        /* connection info */
        addressText?.setTextAndMoveCursorToEnd(prefs!!.getString(Keys.ADDRESS, Defaults.ADDRESS))
        
        portText?.setTextAndMoveCursorToEnd(String.format(
            Locale.ENGLISH, "%d", prefs!!.getInt(Keys.MAIN_PORT, Defaults.MAIN_PORT)))

        httpPortText?.setTextAndMoveCursorToEnd(String.format(
            Locale.ENGLISH, "%d", prefs!!.getInt(Keys.AUDIO_PORT, Defaults.AUDIO_PORT)))

        passwordText?.setTextAndMoveCursorToEnd(prefs!!.getString(Keys.PASSWORD, Defaults.PASSWORD))

        val playbackModes = ArrayAdapter.createFromResource(
            this, R.array.streaming_mode_array, android.R.layout.simple_spinner_item)

        /* playback mode */
        playbackModes.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        playbackModeSpinner?.adapter = playbackModes
        playbackModeSpinner?.setSelection(if (isStreamingEnabled) 1 else 0)

        /* bitrate */
        val bitrates = ArrayAdapter.createFromResource(
            this, R.array.transcode_bitrate_array, android.R.layout.simple_spinner_item)

        bitrates.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        bitrateSpinner?.adapter = bitrates
        bitrateSpinner?.setSelection(prefs!!.getInt(
            Keys.TRANSCODER_BITRATE_INDEX, Defaults.TRANSCODER_BITRATE_INDEX))

        val cacheSizes = ArrayAdapter.createFromResource(
            this, R.array.disk_cache_array, android.R.layout.simple_spinner_item)

        /* disk cache */
        cacheSizes.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        
        cacheSpinner?.adapter = cacheSizes
        cacheSpinner?.setSelection(prefs!!.getInt(
            Keys.DISK_CACHE_SIZE_INDEX, Defaults.DISK_CACHE_SIZE_INDEX))

        /* advanced */
        albumArtCheckbox?.isChecked = prefs!!.getBoolean(
            Keys.ALBUM_ART_ENABLED, Defaults.ALBUM_ART_ENABLED)
        
        messageCompressionCheckbox?.isChecked = prefs!!.getBoolean(
            Keys.MESSAGE_COMPRESSION_ENABLED, Defaults.MESSAGE_COMPRESSION_ENABLED)
        
        softwareVolume?.isChecked = prefs!!.getBoolean(Keys.SOFTWARE_VOLUME, Defaults.SOFTWARE_VOLUME)

        sslCheckbox?.setCheckWithoutEvent(
            this.prefs!!.getBoolean(Keys.SSL_ENABLED,Defaults.SSL_ENABLED), sslCheckChanged)

        certCheckbox?.setCheckWithoutEvent(
            this.prefs!!.getBoolean(Keys.CERT_VALIDATION_DISABLED, Defaults.CERT_VALIDATION_DISABLED),
            certValidationChanged)

        enableUpNavigation()
    }

    private val isStreamingEnabled: Boolean
        get() = this.prefs!!.getBoolean(Keys.STREAMING_PLAYBACK, Defaults.STREAMING_PLAYBACK)

    private val isStreamingSelected: Boolean
        get() = this.playbackModeSpinner?.selectedItemPosition == 1

    private fun onDisableSslFromDialog() {
        sslCheckbox?.setCheckWithoutEvent(false, sslCheckChanged)
    }

    private fun onDisableCertValidationFromDialog() {
        certCheckbox?.setCheckWithoutEvent(false, certValidationChanged)
    }

    private val sslCheckChanged = { _: CompoundButton, value:Boolean ->
        if (value) {
            if (supportFragmentManager.findFragmentByTag(SslAlertDialog.TAG) == null) {
                SslAlertDialog.newInstance().show(supportFragmentManager, SslAlertDialog.TAG)
            }
        }
    }

    private val certValidationChanged = { _: CompoundButton, value: Boolean ->
        if (value) {
            if (supportFragmentManager.findFragmentByTag(DisableCertValidationAlertDialog.TAG) == null) {
                DisableCertValidationAlertDialog.newInstance().show(
                    supportFragmentManager, DisableCertValidationAlertDialog.TAG)
            }
        }
    }

    private fun bindEventListeners() {
        this.addressText = this.findViewById(R.id.address) as EditText
        this.portText = this.findViewById(R.id.port) as EditText
        this.httpPortText = this.findViewById(R.id.http_port) as EditText
        this.passwordText = this.findViewById(R.id.password) as EditText
        this.albumArtCheckbox = findViewById(R.id.album_art_checkbox) as CheckBox
        this.messageCompressionCheckbox = findViewById(R.id.message_compression) as CheckBox
        this.softwareVolume = findViewById(R.id.software_volume) as CheckBox
        this.playbackModeSpinner = findViewById(R.id.playback_mode_spinner) as Spinner
        this.bitrateSpinner = findViewById(R.id.transcoder_bitrate_spinner) as Spinner
        this.cacheSpinner = findViewById(R.id.streaming_disk_cache_spinner) as Spinner
        this.sslCheckbox = findViewById(R.id.ssl_checkbox) as CheckBox
        this.certCheckbox = findViewById(R.id.cert_validation) as CheckBox

        this.playbackModeSpinner?.onItemSelectedListener = (object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(parent: AdapterView<*>?, view: View?, position: Int, id: Long) {
                val streaming = position == 1
                bitrateSpinner?.isEnabled = streaming
                cacheSpinner?.isEnabled = streaming
            }

            override fun onNothingSelected(parent: AdapterView<*>?) {
            }
        })
    }

    private fun save() {
        val addr = addressText?.text.toString()
        val port = portText?.text.toString()
        val httpPort = httpPortText?.text.toString()
        val password = passwordText?.text.toString()

        prefs!!.edit()
            .putString(Keys.ADDRESS, addr)
            .putInt(Keys.MAIN_PORT, if (port.isNotEmpty()) Integer.valueOf(port) else 0)
            .putInt(Keys.AUDIO_PORT, if (httpPort.isNotEmpty()) Integer.valueOf(httpPort) else 0)
            .putString(Keys.PASSWORD, password)
            .putBoolean(Keys.ALBUM_ART_ENABLED, albumArtCheckbox!!.isChecked)
            .putBoolean(Keys.MESSAGE_COMPRESSION_ENABLED, messageCompressionCheckbox!!.isChecked)
            .putBoolean(Keys.STREAMING_PLAYBACK, isStreamingSelected)
            .putBoolean(Keys.SOFTWARE_VOLUME, softwareVolume!!.isChecked)
            .putBoolean(Keys.SSL_ENABLED, sslCheckbox!!.isChecked)
            .putBoolean(Keys.CERT_VALIDATION_DISABLED, certCheckbox!!.isChecked)
            .putInt(Keys.TRANSCODER_BITRATE_INDEX, bitrateSpinner!!.selectedItemPosition)
            .putInt(Keys.DISK_CACHE_SIZE_INDEX, cacheSpinner!!.selectedItemPosition)
            .apply()

        if (!softwareVolume!!.isChecked) {
            PlayerWrapper.setVolume(1.0f)
        }

        if (wasStreaming && !isStreamingEnabled) {
            PlaybackServiceFactory.streaming(this).stop()
        }

        StreamProxy.reload()
        WebSocketService.getInstance(this).disconnect()

        finish()
    }

    class SslAlertDialog : DialogFragment() {
        override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
            val dlg = AlertDialog.Builder(activity)
                .setTitle(R.string.settings_ssl_dialog_title)
                .setMessage(R.string.settings_ssl_dialog_message)
                .setPositiveButton(R.string.button_enable, null)
                .setNegativeButton(R.string.button_disable) { _, _ ->
                    (activity as SettingsActivity).onDisableSslFromDialog()
                }
                .setNeutralButton(R.string.button_learn_more) { _, _ ->
                    try {
                        val intent = Intent(Intent.ACTION_VIEW, Uri.parse(LEARN_MORE_URL))
                        startActivity(intent)
                    }
                    catch (ex: Exception) {
                    }
                }
                .create()

            dlg.setCancelable(false)
            return dlg
        }

        companion object {
            private val LEARN_MORE_URL = "https://github.com/clangen/musikcube/wiki/ssl-server-setup"
            val TAG = "ssl_alert_dialog_tag"

            fun newInstance(): SslAlertDialog {
                return SslAlertDialog()
            }
        }
    }

    class DisableCertValidationAlertDialog : DialogFragment() {
        override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
            val dlg = AlertDialog.Builder(activity)
                .setTitle(R.string.settings_disable_cert_validation_title)
                .setMessage(R.string.settings_disable_cert_validation_message)
                .setPositiveButton(R.string.button_enable, null)
                .setNegativeButton(R.string.button_disable) { _, _ ->
                    (activity as SettingsActivity).onDisableCertValidationFromDialog()
                }
                .create()

            dlg.setCancelable(false)
            return dlg
        }

        companion object {
            val TAG = "disable_cert_verify_dialog"

            fun newInstance(): DisableCertValidationAlertDialog {
                return DisableCertValidationAlertDialog()
            }
        }
    }

    companion object {
        fun getStartIntent(context: Context): Intent {
            return Intent(context, SettingsActivity::class.java)
        }
    }
}
