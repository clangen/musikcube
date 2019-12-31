package io.casey.musikcube.remote.service.gapless

import android.os.Handler
import android.os.HandlerThread
import android.os.Message
import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.injection.DaggerServiceComponent
import io.casey.musikcube.remote.service.gapless.db.GaplessDb
import io.casey.musikcube.remote.service.gapless.db.GaplessTrack
import io.casey.musikcube.remote.service.playback.impl.streaming.StreamProxy
import io.casey.musikcube.remote.ui.shared.extension.createHttpClient
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.Response
import java.io.File
import java.io.RandomAccessFile
import java.net.SocketTimeoutException
import javax.inject.Inject
import kotlin.math.min

/**
 * When MP3 files are transcoded on-demand, the required metadata for gapless playback isn't
 * available yet. So, once we know the transocded files have been downloaded, we run this
 * service to fetch and replace the first few bytes of the file, which should ensure the
 * required header is present, and subsequent plays are gapless.
 */
class GaplessHeaderService {
    @Inject lateinit var db: GaplessDb
    @Inject lateinit var streamProxy: StreamProxy

    private val thread: HandlerThread
    private val handler: Handler
    private val httpClient: OkHttpClient

    init {
        DaggerServiceComponent.builder()
            .appComponent(Application.appComponent)
            .build().inject(this)

        httpClient = createHttpClient(Application.instance)

        thread = HandlerThread("GaplessHeaderService")
        thread.start()

        handler = object : Handler(thread.looper) {
            override fun handleMessage(msg: Message?) {
                if (msg?.what == MESSAGE_PROCESS) {
                    db.dao().queryByState(GaplessTrack.DOWNLOADED).forEach { process(it) }
                }
            }
        }
    }

    fun schedule() {
        if (!handler.hasMessages(MESSAGE_PROCESS)) {
            handler.sendEmptyMessage(MESSAGE_PROCESS)
        }
    }

    private fun process(gaplessTrack: GaplessTrack) {
        val url = gaplessTrack.url
        val fn = File(streamProxy.getProxyFilename(gaplessTrack.url))
        var newState = -1

        if (fn.exists()) {
            /* the first few bytes should be more than enough to snag the
            LAME header that contains gapless playback metadata. just rewrite
            those bytes in the already-downloaded file */
            val req = Request.Builder()
                .addHeader("Range", "bytes=0-$HEADER_SIZE_BYTES")
                .url(url)
                .build()

            var res: Response? = null

            try {
                res = httpClient.newCall(req).execute()
            }
            catch (ex: Exception) {
                newState = GaplessTrack.DOWNLOADED
            }

            if (res?.code() == 206) {
                try {
                    val bytes = res.body()?.bytes()
                    if (bytes?.isNotEmpty() == true) {
                        RandomAccessFile(fn, "rw").use {
                            it.seek(0)
                            it.write(bytes, 0, min(bytes.size, HEADER_SIZE_BYTES))
                        }
                        newState = GaplessTrack.UPDATED
                    }
                }
                catch (ex: SocketTimeoutException) {
                    newState = GaplessTrack.DOWNLOADED
                }
            }
            else {
                newState = GaplessTrack.ERROR
            }
        }

        if (newState == -1) {
            db.dao().deleteByUrl(gaplessTrack.url)
        }
        else {
            db.dao().update(newState, url)
        }
    }

    companion object {
        const val MESSAGE_PROCESS = 1
        const val HEADER_SIZE_BYTES = 6400
    }
}