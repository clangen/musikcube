package io.casey.musikcube.remote.ui.view

import android.content.Context
import android.content.SharedPreferences
import android.util.Base64
import com.bumptech.glide.Glide
import com.bumptech.glide.Registry
import com.bumptech.glide.annotation.GlideModule
import com.bumptech.glide.integration.okhttp3.OkHttpUrlLoader
import com.bumptech.glide.load.model.GlideUrl
import com.bumptech.glide.module.AppGlideModule
import io.casey.musikcube.remote.websocket.Prefs
import okhttp3.OkHttpClient
import java.io.InputStream

@GlideModule
class ApplicationGlideModule : AppGlideModule() {
    override fun registerComponents(context: Context?, glide: Glide?, registry: Registry?) {
        val prefs: SharedPreferences = context!!.getSharedPreferences(Prefs.NAME, Context.MODE_PRIVATE)

        /* intercept requests made against our server, and inject the auth token */
        val client = OkHttpClient.Builder().addInterceptor({ chain ->
            var req = chain.request()
            val serverHost = prefs.getString(Prefs.Key.ADDRESS, "")
            val requestHost = req.url().host()
            if (serverHost == requestHost) {
                val userPass = "default:" + prefs.getString(Prefs.Key.PASSWORD, Prefs.Default.PASSWORD)!!
                val encoded = Base64.encodeToString(userPass.toByteArray(), Base64.NO_WRAP)
                req = req.newBuilder().addHeader("Authorization", "Basic " + encoded).build()
            }
            chain.proceed(req)
        }).build()

        registry?.replace(GlideUrl::class.java, InputStream::class.java, OkHttpUrlLoader.Factory(client))
    }
}