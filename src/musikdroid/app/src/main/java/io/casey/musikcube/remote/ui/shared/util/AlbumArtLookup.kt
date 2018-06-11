package io.casey.musikcube.remote.ui.shared.model.albumart

import android.content.Context
import android.util.LruCache
import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.service.websocket.model.IAlbum
import io.casey.musikcube.remote.service.websocket.model.ITrack
import io.casey.musikcube.remote.ui.settings.constants.Prefs
import io.casey.musikcube.remote.util.Strings
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.Response
import org.json.JSONException
import org.json.JSONObject
import java.io.IOException
import java.net.URLEncoder
import java.util.concurrent.CountDownLatch
import java.util.regex.Pattern

enum class Size constructor(internal val key: String, internal val order: Int) {
    Small("small", 0),
    Medium("medium", 1),
    Large("large", 2),
    ExtraLarge("extralarge", 3),
    Mega("mega", 4);

    companion object {
        internal fun from(value: String?): Size {
            return values().find { it.key == value } ?: Medium
        }
    }
}

/* used to strip extraneous tags */
private val badPatterns = arrayOf(
    Pattern.compile("(?i)^" + Pattern.quote("[") + "CD" + Pattern.quote("]")),
    Pattern.compile("(?i)" + Pattern.quote("(") + "disc \\d*" + Pattern.quote(")") + "$"),
    Pattern.compile("(?i)" + Pattern.quote("[") + "disc \\d*" + Pattern.quote("]") + "$"),
    Pattern.compile("(?i)" + Pattern.quote("(+") + "video" + Pattern.quote(")") + "$"),
    Pattern.compile("(?i)" + Pattern.quote("[+") + "video" + Pattern.quote("]") + "$"),
    Pattern.compile("(?i)" + Pattern.quote("(") + "explicit" + Pattern.quote(")") + "$"),
    Pattern.compile("(?i)" + Pattern.quote("[") + "explicit" + Pattern.quote("]") + "$"),
    Pattern.compile("(?i)" + Pattern.quote("[+") + "digital booklet" + Pattern.quote("]") + "$"))

/* http://www.last.fm/group/Last.fm+Web+Services/forum/21604/_/522900 -- it's ok to
put our key in the code */
private val lastFmFormatUrl =
    "http://ws.audioscrobbler.com/2.0/?method=album.getinfo&api_key=" +
    "502c69bd3f9946e8e0beee4fcb28c4cd&artist=%s&album=%s&format=json&size=%s"

private val urlCache = LruCache<String, String>(500)
private val badUrlCache = LruCache<String, Boolean>(100)
private val inFlight = mutableMapOf<String, CountDownLatch>()

private val prefs by lazy {
    Application.instance.getSharedPreferences(Prefs.NAME, Context.MODE_PRIVATE)
}

fun getUrl(album: IAlbum, size: Size = Size.Small): String? {
    return getThumbnailUrl(album.thumbnailId)
        ?: getUrl(album.albumArtist, album.name, size)
}

fun getUrl(track: ITrack, size: Size = Size.Small): String? {
    return getThumbnailUrl(track.thumbnailId)
        ?: getUrl(track.artist, track.album, size)
}

fun getUrl(artist: String = "", album: String = "", size: Size = Size.Small): String? {
    if (!prefs.getBoolean(Prefs.Key.LASTFM_ENABLED, Prefs.Default.LASTFM_ENABLED)) {
        return null
    }

    if (artist.isBlank() || album.isBlank()) {
        return null
    }

    return String.format(lastFmFormatUrl, dejunk(artist), dejunk(album), size.key)
}

fun canIntercept(request: Request): Boolean {
    return request.url().host() == "ws.audioscrobbler.com" &&
        request.url().queryParameter("method") == "album.getinfo"
}

private val httpClient = OkHttpClient.Builder().build()

private fun executeWithRetries(req: Request): Response? {
    var count = 0
    var result: Response? = null
    while (count++ < 3) {
        try {
            val response = httpClient.newCall(req).execute()
            if (response.isSuccessful) {
                result = response
                break
            }
        }
        catch (ex: IOException) {
            /* om nom nom */
        }
    }
    return result
}

fun intercept(req: Request): Request? {
    val url = req.url()

    var imageUrl = urlCache[url.toString()] ?: ""
    val desiredSize = Size.from(url.queryParameter("size"))
    var badUrl = false
    var pending: CountDownLatch? = null

    synchronized(urlCache) {
        badUrl = badUrlCache.get(url.toString()) ?: false
        pending = inFlight[url.toString()]
    }

    /* let's see if there's already another request for this URL in flight. if there is,
    let it finish, as it'll wind up in the cache and we won't have to make multiple
    requests against the backend */
    if (pending != null) {
        pending?.await()
        pending = null

        synchronized(urlCache) {
            imageUrl = urlCache[url.toString()] ?: ""
        }
    }

    /* depending on the above, we may have an imageUrl! if we do, we're good. otherwise,
    let's setup the countdown latch so subsequent requests wait... */
    if (imageUrl.isBlank() && !badUrl) {
        synchronized(urlCache) {
            pending = CountDownLatch(1)
            inFlight.put(url.toString(), pending!!)
        }
    }

    if (imageUrl.isBlank() && !badUrl) {
        val response = executeWithRetries(req)

        if (response != null) {
            val images = mutableListOf<Pair<Size, String>>()

            try {
                val json = JSONObject(response.body()?.string())
                val imagesJson = json.getJSONObject("album").getJSONArray("image")
                for (i in 0 until imagesJson.length()) {
                    val imageJson = imagesJson.getJSONObject(i)
                    val size = Size.from(imageJson.optString("size", ""))
                    val resolvedUrl = imageJson.optString("#text", "")
                    if (Strings.notEmpty(resolvedUrl)) {
                        images.add(Pair<Size, String>(size, resolvedUrl))
                    }
                }
            }
            catch (ex: JSONException) {
                badUrlCache.put(url.toString(), true)
            }

            if (images.size > 0) {
                /* find the image with the closest to the requested size.
                exact match preferred. */
                var closest = images[0]
                var lastDelta = Integer.MAX_VALUE
                for (check in images) {
                    if (check.first == desiredSize) {
                        closest = check
                        break
                    }
                    else {
                        val delta = Math.abs(desiredSize.order - check.first.order)
                        if (lastDelta > delta) {
                            closest = check
                            lastDelta = delta
                        }
                    }
                }

                imageUrl = closest.second
            }
        }
    }

    var result: Request? = null

    if (imageUrl.isNotBlank()) {
        synchronized(urlCache) {
            urlCache.put(url.toString(), imageUrl)
        }

        if (desiredSize == Size.Mega) {
            imageUrl = imageUrl.replace("/i/u/300x300", "/i/u/600x600")
        }

        result = Request.Builder().url(imageUrl).build()
    }

    synchronized(urlCache) {
        if (pending != null) {
            pending?.countDown()
            inFlight.remove(url.toString())
        }
    }

    return result
}

private fun getThumbnailUrl(id: Long): String? {
    if (id > 0) {
        val host = prefs.getString(Prefs.Key.ADDRESS, Prefs.Default.ADDRESS)
        val port = prefs.getInt(Prefs.Key.AUDIO_PORT, Prefs.Default.MAIN_PORT)
        val ssl = prefs.getBoolean(Prefs.Key.SSL_ENABLED, Prefs.Default.SSL_ENABLED)
        val scheme = if (ssl) "https" else "http"
        return "$scheme://$host:$port/thumbnail/$id"
    }
    return null
}

private fun dejunk(album: String): String {
    var result = album
    for (pattern in badPatterns) {
        result = pattern.matcher(result).replaceAll("")
    }
    return URLEncoder.encode(result.trim { it.isWhitespace() })
}