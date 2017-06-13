package io.casey.musikcube.remote.playback

import android.content.Context
import android.content.SharedPreferences
import android.net.Uri
import android.util.Base64
import com.danikula.videocache.CacheListener
import com.danikula.videocache.HttpProxyCacheServer
import com.danikula.videocache.file.Md5FileNameGenerator
import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.util.NetworkUtil
import io.casey.musikcube.remote.util.Strings
import io.casey.musikcube.remote.websocket.Prefs
import java.io.File
import java.util.*

class StreamProxy private constructor(context: Context) {
    private val proxy: HttpProxyCacheServer
    private val prefs: SharedPreferences

    init {
        prefs = context.getSharedPreferences(Prefs.NAME, Context.MODE_PRIVATE)

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
            .headerInjector { url ->
                val headers = HashMap<String, String>()
                val userPass = "default:" + prefs.getString(Prefs.Key.PASSWORD, Prefs.Default.PASSWORD)!!
                val encoded = Base64.encodeToString(userPass.toByteArray(), Base64.NO_WRAP)
                headers.put("Authorization", "Basic " + encoded)
                headers
            }
            .fileNameGenerator gen@ { url ->
                try {
                    val uri = Uri.parse(url)
                    /* format is: audio/external_id/<id> */
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
        val CACHE_SETTING_TO_BYTES: MutableMap<Int, Long>
        private val DEFAULT_FILENAME_GENERATOR = Md5FileNameGenerator()

        init {
            CACHE_SETTING_TO_BYTES = HashMap<Int, Long>()
            CACHE_SETTING_TO_BYTES.put(0, MINIMUM_CACHE_SIZE_BYTES)
            CACHE_SETTING_TO_BYTES.put(1, BYTES_PER_GIGABYTE / 2)
            CACHE_SETTING_TO_BYTES.put(2, BYTES_PER_GIGABYTE)
            CACHE_SETTING_TO_BYTES.put(3, BYTES_PER_GIGABYTE * 2)
            CACHE_SETTING_TO_BYTES.put(4, BYTES_PER_GIGABYTE * 3)
            CACHE_SETTING_TO_BYTES.put(5, BYTES_PER_GIGABYTE * 4)
        }

        private var INSTANCE: StreamProxy? = null

        @Synchronized fun init(context: Context) {
            if (INSTANCE == null) {
                INSTANCE = StreamProxy(context.applicationContext)
            }
        }

        @Synchronized fun registerCacheListener(cl: CacheListener, uri: String) {
            if (INSTANCE != null) {
                INSTANCE!!.proxy.registerCacheListener(cl, uri) /* let it throw */
            }
        }

        @Synchronized fun unregisterCacheListener(cl: CacheListener) {
            if (INSTANCE != null) {
                INSTANCE!!.proxy.unregisterCacheListener(cl)
            }
        }

        @Synchronized fun isCached(url: String): Boolean {
            return INSTANCE != null && INSTANCE!!.proxy.isCached(url)
        }

        @Synchronized fun getProxyUrl(url: String): String {
            init(Application.instance!!)
            return if (ENABLED) INSTANCE!!.proxy.getProxyUrl(url) else url
        }

        @Synchronized fun reload() {
            if (INSTANCE != null) {
                INSTANCE!!.proxy.shutdown()
                INSTANCE = null
            }
        }
    }
}
