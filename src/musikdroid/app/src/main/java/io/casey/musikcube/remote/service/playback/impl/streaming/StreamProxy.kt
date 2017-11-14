package io.casey.musikcube.remote.service.playback.impl.streaming

import android.content.Context
import android.content.SharedPreferences
import android.net.Uri
import android.util.Base64
import com.danikula.videocache.CacheListener
import com.danikula.videocache.HttpProxyCacheServer
import com.danikula.videocache.file.Md5FileNameGenerator
import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.ui.shared.util.NetworkUtil
import io.casey.musikcube.remote.util.Strings
import io.casey.musikcube.remote.ui.settings.constants.Prefs
import java.io.File
import java.util.*

class StreamProxy private constructor(context: Context) {
    private val proxy: HttpProxyCacheServer
    private val prefs: SharedPreferences = context.getSharedPreferences(Prefs.NAME, Context.MODE_PRIVATE)

    init {
        if (this.prefs.getBoolean(Prefs.Key.CERT_VALIDATION_DISABLED, Prefs.Default.CERT_VALIDATION_DISABLED)) {
            NetworkUtil.disableCertificateValidation()
        }
        else {
            NetworkUtil.enableCertificateValidation()
        }

        var diskCacheIndex = this.prefs.getInt(
                Prefs.Key.DISK_CACHE_SIZE_INDEX, Prefs.Default.DISK_CACHE_SIZE_INDEX)

        if (diskCacheIndex < 0 || diskCacheIndex > CACHE_SETTING_TO_BYTES.size) {
            diskCacheIndex = 0
        }

        val cachePath = File(context.externalCacheDir, "audio")

        proxy = HttpProxyCacheServer.Builder(context.applicationContext)
            .cacheDirectory(cachePath)
            .maxCacheSize(CACHE_SETTING_TO_BYTES[diskCacheIndex] ?: MINIMUM_CACHE_SIZE_BYTES)
            .headerInjector { _ ->
                val headers = HashMap<String, String>()
                val userPass = "default:" + prefs.getString(Prefs.Key.PASSWORD, Prefs.Default.PASSWORD)!!
                val encoded = Base64.encodeToString(userPass.toByteArray(), Base64.NO_WRAP)
                headers.put("Authorization", "Basic " + encoded)
                headers
            }
            .fileNameGenerator gen@ { url ->
                try {
                    val uri = Uri.parse(url)
                    /* format matches: audio/external_id/<id> */
                    val segments = uri.pathSegments
                    if (segments.size == 3 && "external_id" == segments[1]) {
                        /* url params, hyphen separated */
                        var params = uri.query
                        if (Strings.notEmpty(params)) {
                            params = "-" + params
                                .replace("?", "-")
                                .replace("&", "-")
                                .replace("=", "-")
                        }
                        else {
                            params = ""
                        }

                        return@gen "${segments[2]}$params"
                    }
                }
                catch (ex: Exception) {
                    /* eh... */
                }

                return@gen DEFAULT_FILENAME_GENERATOR.generate(url)
            }
            .build()
    }

    companion object {
        val ENABLED = true
        val BYTES_PER_MEGABYTE = 1048576L
        val BYTES_PER_GIGABYTE = 1073741824L
        val MINIMUM_CACHE_SIZE_BYTES = BYTES_PER_MEGABYTE * 128

        val CACHE_SETTING_TO_BYTES: MutableMap<Int, Long> = mutableMapOf(
            0 to MINIMUM_CACHE_SIZE_BYTES,
            1 to BYTES_PER_GIGABYTE / 2,
            2 to BYTES_PER_GIGABYTE,
            3 to BYTES_PER_GIGABYTE * 2,
            4 to BYTES_PER_GIGABYTE * 3,
            5 to BYTES_PER_GIGABYTE * 4)

        private val DEFAULT_FILENAME_GENERATOR = Md5FileNameGenerator()

        private var INSTANCE: StreamProxy? = null

        @Synchronized fun init(context: Context) {
            if (INSTANCE == null) {
                INSTANCE = StreamProxy(context.applicationContext)
            }
        }

        @Synchronized fun registerCacheListener(cl: CacheListener, uri: String) {
            INSTANCE?.proxy?.registerCacheListener(cl, uri) /* let it throw */
        }

        @Synchronized fun unregisterCacheListener(cl: CacheListener) {
            INSTANCE?.proxy?.unregisterCacheListener(cl)
        }

        @Synchronized fun isCached(url: String): Boolean {
            return INSTANCE?.proxy?.isCached(url)!!
        }

        @Synchronized fun getProxyUrl(url: String): String {
            init(Application.instance!!)
            return if (ENABLED) INSTANCE?.proxy?.getProxyUrl(url)!! else url
        }

        @Synchronized fun reload() {
            INSTANCE?.proxy?.shutdown()
            INSTANCE = null
        }
    }
}
