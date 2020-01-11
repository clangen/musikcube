package io.casey.musikcube.remote.ui.download.activity

import android.Manifest
import android.app.Dialog
import android.content.Intent
import android.content.pm.PackageManager
import android.media.MediaScannerConnection
import android.net.Uri
import android.os.Bundle
import android.os.Environment
import android.os.Handler
import android.view.View
import android.widget.TextView
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.fragment.app.DialogFragment
import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.service.websocket.model.ITrack
import io.casey.musikcube.remote.ui.settings.constants.Prefs
import io.casey.musikcube.remote.ui.shared.activity.BaseActivity
import io.casey.musikcube.remote.ui.shared.extension.*
import me.zhanghai.android.materialprogressbar.MaterialProgressBar
import okhttp3.Call
import okhttp3.Callback
import okhttp3.Request
import okhttp3.Response
import java.io.File
import java.io.IOException

class TrackDownloadActivity: BaseActivity() {
    private val httpClient = createHttpClient(Application.instance)
    private val mutex = Object()
    private var pendingCall: Call? = null
    private lateinit var spinner: MaterialProgressBar
    private lateinit var progress: MaterialProgressBar
    private val handler = Handler()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(R.layout.track_download_activity)

        findViewById<TextView>(R.id.title).text = getString(
            R.string.downloading_please_wait,
            intent.getStringExtra(EXTRA_TRACK_TITLE))
        progress = findViewById(R.id.progress)
        spinner = findViewById(R.id.spinner)

        if (savedInstanceState != null) {
            findDialog<RetryDialog>(RetryDialog.TAG)?.onRetry = this::download
        }

        val canWrite = ContextCompat.checkSelfPermission(
            this,
            Manifest.permission.WRITE_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED

        if (!canWrite) {
            if (ActivityCompat.shouldShowRequestPermissionRationale(
                this, Manifest.permission.WRITE_EXTERNAL_STORAGE))
            {
                finish()
            }
            else {
                ActivityCompat.requestPermissions(
                    this,
                    arrayOf(Manifest.permission.WRITE_EXTERNAL_STORAGE),
                    PERMISSION_REQUEST_CODE)
            }
        }
        else {
            download()
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int, permissions: Array<String>, grantResults: IntArray)
    {
        if (requestCode == PERMISSION_REQUEST_CODE) {
            if ((grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED)) {
                download()
            }
            else {
                finish()
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        cancel()
    }

    override fun onBackPressed() {
        confirmCancelDialog()
    }

    private fun cancel() {
        synchronized(mutex) {
            pendingCall?.let {
                it.cancel()
                pendingCall = null
                File(outputFilename).delete()
            }
        }
    }

    private fun updateProgress(percent: Int) {
        handler.post {
            if (percent > 0) {
                spinner.visibility = View.GONE
                progress.visibility = View.VISIBLE
                progress.progress = percent
            }
            else {
                spinner.visibility = View.VISIBLE
                progress.visibility = View.GONE
            }
        }
    }

    private fun processResponse(response: Response) {
        var success = false

        try {
            response.body()?.let {
                val total = it.contentLength()
                var lastPercent = 0

                updateProgress(0)

                val onBytesReceived = { bytes: Long ->
                    if (total > 0) {
                        val updatedPercent = ((bytes * 100) / total).toInt()
                        if (updatedPercent != lastPercent) {
                            updateProgress(updatedPercent)
                            lastPercent = updatedPercent
                        }
                    }
                }

                if (it.byteStream().toFile(outputFilename, onBytesReceived)) {
                    MediaScannerConnection.scanFile(
                        this@TrackDownloadActivity,
                        arrayOf(outputFilename),
                        null)
                    { _, _ ->
                        finish()
                    }

                    success = true
                }
            }
        }
        catch (ex: Exception) {
            /* move on... */
        }
        finally {
            synchronized (mutex) {
                pendingCall = null
            }
            response.close()
        }

        if (!success) {
            showRetryDialog()
        }
    }

    private fun download() {
        synchronized (mutex) {
            if (pendingCall == null) {
                val request = Request.Builder().url(trackUrl.toString()).build()
                httpClient.newCall(request).enqueue(object : Callback {
                    override fun onFailure(call: Call, e: IOException) {
                        synchronized (mutex) {
                            pendingCall = null
                        }
                    }

                    override fun onResponse(call: Call, response: Response) {
                        processResponse(response)
                    }
                })
            }
        }
    }

    private fun confirmCancelDialog() {
        if (!dialogVisible(ConfirmCancelDialog.TAG)) {
            showDialog(ConfirmCancelDialog.create(),ConfirmCancelDialog.TAG)
        }
    }

    private fun showRetryDialog() {
        if (!dialogVisible(RetryDialog.TAG)) {
            showDialog(RetryDialog.create().apply {
                onRetry = this@TrackDownloadActivity::download
            }, RetryDialog.TAG)
        }
    }

    private val outputFilename: String
        get() {
            val dir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS)
            val title = intent.getStringExtra(EXTRA_TRACK_TITLE)
            val externalId = intent.getStringExtra(EXTRA_EXTERNAL_ID)
            var extension = intent.extras?.getString(EXTRA_EXTENSION, "mp3")
            extension = if (extension.isNullOrBlank()) "mp3" else extension
            return "$dir/musikdroid/$title-$externalId.$extension"
        }

    private val trackUrl: Uri
        get() {
            val ssl = prefs.getBoolean(Prefs.Key.SSL_ENABLED, Prefs.Default.SSL_ENABLED)
            val protocol = if (ssl) "https" else "http"
            val port = prefs.getInt(Prefs.Key.AUDIO_PORT, Prefs.Default.AUDIO_PORT)
            val host = prefs.getString(Prefs.Key.ADDRESS, Prefs.Default.ADDRESS)

            return Uri.Builder()
                .scheme(protocol)
                .encodedAuthority("$host:$port")
                .appendPath("audio")
                .appendPath("external_id")
                .appendPath(intent.getStringExtra(EXTRA_EXTERNAL_ID)).build()
        }

    class ConfirmCancelDialog: DialogFragment() {
        override fun onCreateDialog(savedInstanceState: Bundle?): Dialog =
            AlertDialog.Builder(activity!!)
                .setTitle(R.string.download_cancel_title)
                .setMessage(R.string.download_cancel_message)
                .setNegativeButton(R.string.button_no, null)
                .setPositiveButton(R.string.button_yes) { _, _ ->
                    activity?.finish()
                }
                .create().apply {
                    setCancelable(false)
                }

        companion object {
            const val TAG = "confirm_cancel_dialog"
            fun create() = ConfirmCancelDialog()
        }
    }

    class RetryDialog: DialogFragment() {
        override fun onCreateDialog(savedInstanceState: Bundle?): Dialog =
            AlertDialog.Builder(activity!!)
                .setTitle(R.string.download_failed_title)
                .setMessage(R.string.download_failed_message)
                .setNegativeButton(R.string.button_no) { _, _ ->
                    activity?.finish()
                }
                .setPositiveButton(R.string.button_yes) { _, _ ->
                    onRetry?.invoke()
                }
                .create().apply {
                    setCancelable(false)
                }

        var onRetry: (() -> Unit)? = null

        companion object {
            const val TAG = "retry_dialog"
            fun create() = RetryDialog()
        }
    }

    companion object {
        private const val PERMISSION_REQUEST_CODE = 32
        private const val EXTRA_EXTERNAL_ID = "extra_external_id"
        private const val EXTRA_TRACK_TITLE = "extra_track_title"
        private const val EXTRA_EXTENSION = "extra_extension"

        fun getStartIntent(activity: AppCompatActivity, track: ITrack): Intent {
            return Intent(activity, TrackDownloadActivity::class.java).apply {
                putExtra(EXTRA_EXTERNAL_ID, track.externalId)
                putExtra(EXTRA_EXTENSION, track.uri.split(".").lastOrNull())
                putExtra(EXTRA_TRACK_TITLE, track.title)
            }
        }
    }
}