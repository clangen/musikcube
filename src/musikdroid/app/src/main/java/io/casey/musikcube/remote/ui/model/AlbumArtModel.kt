package io.casey.musikcube.remote.ui.model

import android.util.LruCache
import io.casey.musikcube.remote.util.Strings
import okhttp3.*
import org.json.JSONException
import org.json.JSONObject
import java.io.IOException
import java.net.SocketTimeoutException
import java.net.URLEncoder
import java.util.*
import java.util.concurrent.TimeUnit
import java.util.regex.Pattern

class AlbumArtModel(val track: String,
                    private val artist: String = "",
                    private val album: String = "",
                    desiredSize: AlbumArtModel.Size,
                    callback: AlbumArtModel.AlbumArtCallback?)
{
    private var callback: AlbumArtCallback? = null
    private var fetching: Boolean = false
    private var noart: Boolean = false

    val id: Int

    @get:Synchronized var url: String? = null
        private set

    enum class Size constructor(internal val key: String, internal val order: Int) {
        Small("small", 0),
        Medium("medium", 1),
        Large("large", 2),
        ExtraLarge("extralarge", 3),
        Mega("mega", 4);

        companion object {
            internal fun from(value: String): Size? {
                return values().find { it.key == value }
            }
        }
    }

    class Image(internal val size: Size, internal val url: String)

    init {
        this.callback = callback ?: DEFAULT_CALLBACK
        this.id = (artist + album + desiredSize.key).hashCode()

        synchronized(this) {
            this.url = URL_CACHE.get(id)
        }
    }

    fun destroy() {
        this.callback = DEFAULT_CALLBACK
    }

    fun matches(artist: String, album: String): Boolean {
        return this.artist.equals(artist, ignoreCase = true) &&
           this.album.equals(album, ignoreCase = true)
    }

    interface AlbumArtCallback { /* TODO: remove this after converting everything to Kotlin */
        fun onFinished(model: AlbumArtModel, url: String?)
    }

    @Synchronized fun fetch(): AlbumArtModel {
        if (this.fetching || this.noart) {
            return this
        }

        if (!Strings.empty(this.url)) {
            callback?.onFinished(this, this.url)
        }
        else if (Strings.notEmpty(this.artist) && Strings.notEmpty(this.album)) {
            val requestUrl: String

            try {
                val sanitizedAlbum = removeKnownJunkFromMetadata(album)
                val sanitizedArtist = removeKnownJunkFromMetadata(artist)

                requestUrl = String.format(
                    LAST_FM_ALBUM_INFO,
                    URLEncoder.encode(sanitizedArtist, "UTF-8"),
                    URLEncoder.encode(sanitizedAlbum, "UTF-8"))
            }
            catch (ex: Exception) {
                throw RuntimeException(ex)
            }

            this.fetching = true
            val request = Request.Builder().url(requestUrl).build()

            OK_HTTP.newCall(request).enqueue(object : Callback {
                override fun onFailure(call: Call, e: IOException) {
                    fetching = false
                    callback?.onFinished(this@AlbumArtModel, null)
                }

                @Throws(IOException::class)
                override fun onResponse(call: Call, response: Response) {
                    synchronized(this@AlbumArtModel) {
                        val imageList = ArrayList<Image>()

                        try {
                            val json = JSONObject(response.body()?.string())
                            val imagesJson = json.getJSONObject("album").getJSONArray("image")
                            for (i in 0..imagesJson.length() - 1) {
                                val imageJson = imagesJson.getJSONObject(i)
                                val size = Size.from(imageJson.optString("size", ""))
                                if (size != null) {
                                    val resolvedUrl = imageJson.optString("#text", "")
                                    if (Strings.notEmpty(resolvedUrl)) {
                                        imageList.add(Image(size, resolvedUrl))
                                    }
                                }
                            }

                            if (imageList.size > 0) {
                                /* find the image with the closest to the requested size.
                                exact match preferred. */
                                val desiredSize = Size.Mega
                                var closest = imageList[0]
                                var lastDelta = Integer.MAX_VALUE
                                for (check in imageList) {
                                    if (check.size == desiredSize) {
                                        closest = check
                                        break
                                    }
                                    else {
                                        val delta = Math.abs(desiredSize.order - check.size.order)
                                        if (lastDelta > delta) {
                                            closest = check
                                            lastDelta = delta
                                        }
                                    }
                                }

                                synchronized(this@AlbumArtModel) {
                                    URL_CACHE.put(id, closest.url)
                                }

                                fetching = false
                                this@AlbumArtModel.url = closest.url
                                callback?.onFinished(this@AlbumArtModel, closest.url)
                                return
                            }
                        }
                        catch (ex: JSONException) {
                        }

                        noart = true /* got a response, but it was invalid. we won't try again */
                        fetching = false
                    }

                    callback?.onFinished(this@AlbumArtModel, null)
                }
            })
        }
        else {
            callback?.onFinished(this, null)
        }

        return this
    }

    companion object {
        /* http://www.last.fm/group/Last.fm+Web+Services/forum/21604/_/522900 -- it's ok to
        put our key in the code */
        private val LAST_FM_ALBUM_INFO =
            "http://ws.audioscrobbler.com/2.0/?method=album.getinfo&api_key=" +
            "502c69bd3f9946e8e0beee4fcb28c4cd&artist=%s&album=%s&format=json"

        private var OK_HTTP: OkHttpClient
        private val URL_CACHE = LruCache<Int, String>(500)

        private val DEFAULT_CALLBACK = object : AlbumArtCallback {
            override fun onFinished(model: AlbumArtModel, url: String?) {
            }
        }

        init {
            OK_HTTP = OkHttpClient.Builder()
                .addInterceptor { chain: Interceptor.Chain ->
                    val request = chain.request()
                    var count = 0
                    var result: Response? = null
                    while (count++ < 3) {
                        try {
                            val response = chain.proceed(request)
                            if (response.isSuccessful) {
                                result = response
                                break
                            }
                        }
                        catch (ex: SocketTimeoutException) {
                            /* om nom nom */
                        }
                    }
                    result ?: throw IOException("retries exhausted")
                }
                .connectTimeout(3, TimeUnit.SECONDS)
                .build()
        }

        fun empty(): AlbumArtModel {
            return AlbumArtModel("", "", "", Size.Small, null)
        }

        private val BAD_PATTERNS = arrayOf(
            Pattern.compile("(?i)^" + Pattern.quote("[") + "CD" + Pattern.quote("]")),
            Pattern.compile("(?i)" + Pattern.quote("(") + "disc \\d*" + Pattern.quote(")") + "$"),
            Pattern.compile("(?i)" + Pattern.quote("[") + "disc \\d*" + Pattern.quote("]") + "$"),
            Pattern.compile("(?i)" + Pattern.quote("(+") + "video" + Pattern.quote(")") + "$"),
            Pattern.compile("(?i)" + Pattern.quote("[+") + "video" + Pattern.quote("]") + "$"),
            Pattern.compile("(?i)" + Pattern.quote("(") + "explicit" + Pattern.quote(")") + "$"),
            Pattern.compile("(?i)" + Pattern.quote("[") + "explicit" + Pattern.quote("]") + "$"),
            Pattern.compile("(?i)" + Pattern.quote("[+") + "digital booklet" + Pattern.quote("]") + "$"))

        private fun removeKnownJunkFromMetadata(album: String): String {
            var result = album
            for (pattern in BAD_PATTERNS) {
                result = pattern.matcher(result).replaceAll("")
            }
            return result.trim { it <= ' ' }
        }
    }
}
