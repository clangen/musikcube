package io.casey.musikcube.remote.injection

import android.content.Context
import android.content.SharedPreferences
import android.util.Base64
import com.bumptech.glide.Glide
import com.bumptech.glide.Registry
import com.bumptech.glide.annotation.GlideModule
import com.bumptech.glide.integration.okhttp3.OkHttpUrlLoader
import com.bumptech.glide.load.model.GlideUrl
import com.bumptech.glide.module.AppGlideModule
import io.casey.musikcube.remote.ui.settings.constants.Prefs
import okhttp3.*
import java.io.InputStream
import io.casey.musikcube.remote.ui.shared.util.AlbumArtLookup.canIntercept as canInterceptArtwork
import io.casey.musikcube.remote.ui.shared.util.AlbumArtLookup.intercept as interceptArtwork

@GlideModule
class GlideModule : AppGlideModule() {
   override fun registerComponents(context: Context, glide: Glide, registry: Registry) {
        val prefs: SharedPreferences = context.getSharedPreferences(Prefs.NAME, Context.MODE_PRIVATE)

        /* intercept requests made against our server, and inject the auth token */
        val client = OkHttpClient.Builder().addInterceptor { chain ->
            var request: Request? = chain.request()
            request?.let { req ->
                val serverHost = prefs.getString(Prefs.Key.ADDRESS, "")
                val requestHost = req.url().host()
                if (serverHost == requestHost) {
                    val userPass = "default:" + prefs.getString(Prefs.Key.PASSWORD, Prefs.Default.PASSWORD)!!
                    val encoded = Base64.encodeToString(userPass.toByteArray(), Base64.NO_WRAP)
                    request = req.newBuilder().addHeader("Authorization", "Basic " + encoded).build()
                } else if (canInterceptArtwork(req)) {
                    request = interceptArtwork(req)
                }
            }
            if (request != null) chain.proceed(request!!) else error(chain)
        }.build()

        registry.replace(GlideUrl::class.java, InputStream::class.java, OkHttpUrlLoader.Factory(client))
    }

    private fun error(chain: Interceptor.Chain): Response {
        return Response.Builder()
            .request(chain.request())
            .code(404)
            .protocol(Protocol.HTTP_1_1)
            .body(ResponseBody.create(MediaType.parse("application/json"), "{ }"))
            .message("not found")
            .build()
    }
}