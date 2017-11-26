package io.casey.musikcube.remote.service.playback.impl.streaming

import android.content.Context
import android.content.SharedPreferences
import android.net.Uri
import android.util.Base64
import com.danikula.videocache.CacheListener
import com.danikula.videocache.HttpProxyCacheServer
import com.danikula.videocache.file.Md5FileNameGenerator
import io.casey.musikcube.remote.ui.settings.constants.Prefs
import io.casey.musikcube.remote.ui.shared.util.NetworkUtil
import io.casey.musikcube.remote.util.Strings
import java.io.File
import java.util.*

class StreamProxy(private val context: Context) {
    private lateinit var proxy: HttpProxyCacheServer
    private val prefs: SharedPreferences = context.getSharedPreferences(Prefs.NAME, Context.MODE_PRIVATE)

    init {
        restart()
    }

    @Synchronized fun registerCacheListener(cl: CacheListener, uri: String) {
        proxy.registerCacheListener(cl, uri) /* let it throw */
    }

    @Synchronized fun unregisterCacheListener(cl: CacheListener) {
        proxy.unregisterCacheListener(cl)
    }

    @Synchronized fun isCached(url: String): Boolean {
        return proxy.isCached(url)
    }

    @Synchronized fun getProxyUrl(url: String): String {
        return proxy.getProxyUrl(url)
    }

    @Synchronized fun getProxyFilename(url: String): String {
        return FILENAME_GENERATOR(url)
    }

    @Synchronized fun reload() {
        proxy.shutdown()
        restart()
    }

    private fun restart() {
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
            .fileNameGenerator(FILENAME_GENERATOR)
            .build()
    }

    companion object {
        private val BYTES_PER_MEGABYTE = 1048576L
        private val BYTES_PER_GIGABYTE = 1073741824L
        val MINIMUM_CACHE_SIZE_BYTES = BYTES_PER_MEGABYTE * 128

        val CACHE_SETTING_TO_BYTES: MutableMap<Int, Long> = mutableMapOf(
            0 to MINIMUM_CACHE_SIZE_BYTES,
            1 to BYTES_PER_GIGABYTE / 2,
            2 to BYTES_PER_GIGABYTE,
            3 to BYTES_PER_GIGABYTE * 2,
            4 to BYTES_PER_GIGABYTE * 3,
            5 to BYTES_PER_GIGABYTE * 4)

        private val DEFAULT_FILENAME_GENERATOR = Md5FileNameGenerator()

        private val FILENAME_GENERATOR: (String) -> String = gen@ { url ->
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
    }
}
