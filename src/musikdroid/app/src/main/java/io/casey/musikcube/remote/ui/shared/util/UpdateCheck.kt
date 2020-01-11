package io.casey.musikcube.remote.ui.shared.util

import android.util.Log
import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.util.Preconditions
import io.reactivex.Single
import io.reactivex.android.schedulers.AndroidSchedulers
import io.reactivex.schedulers.Schedulers
import okhttp3.OkHttpClient
import okhttp3.Request
import org.json.JSONObject

private const val UPDATE_CHECK_URL = "https://musikcube.com/version"
private const val LATEST = "latest"
private const val ANDROID = "android"
private const val MAJOR = "major"
private const val MINOR = "minor"
private const val PATCH = "patch"
private const val URL = "url"
private const val EXPIRY_MILLIS: Long = 1000 * 60 * 30 /* 30 minutes */

private data class Result(
    val required: Boolean,
    val version: String,
    val url: String,
    val time: Long)

private fun toLongVersion(str: String): Long {
    val parts = str.split(".")
    if (parts.count() == 3) {
        return toLongVersion(parts[0].toLong(), parts[1].toLong(), parts[2].toLong())
    }
    else if (parts.count() == 2) {
        return toLongVersion(parts[0].toLong(), parts[1].toLong(), 0)
    }
    return 0
}

private fun toLongVersion(major: Long, minor: Long, patch: Long): Long {
    var result: Long = major
    result = result.shl(16) or minor
    return result.shl(16) or patch
}

private fun toStringVersion(major: Long, minor: Long, patch: Long): String {
    return "$major.$minor.$patch"
}

class UpdateCheck {
    fun run(callback: (Boolean, String, String) -> Unit) {
        Preconditions.throwIfNotOnMainThread()

        if (!expired()) {
            callback(LAST_RESULT.required, LAST_RESULT.version, LAST_RESULT.url)
        }
        else {
            @Suppress Single.fromCallable {
                val request = Request.Builder().url(UPDATE_CHECK_URL).build()
                val response = HTTP_CLIENT.newCall(request).execute()
                val json = response.body()?.string() ?: ""
                response.close()
                JSONObject(json)
            }
            .map { json ->
                val android = json.optJSONObject(LATEST)?.optJSONObject(ANDROID)
                if (android != null) {
                    val url = android.getString(URL)
                    val major = android.optLong(MAJOR, 0)
                    val minor = android.optLong(MINOR, 0)
                    val patch = android.optLong(PATCH, 0)

                    val latest = toLongVersion(major, minor, patch)
                    val current = toLongVersion(VERSION)

                    Result(
                            latest > current,
                            toStringVersion(major, minor, patch),
                            url,
                            System.currentTimeMillis())
                }
                else {
                    throw IllegalArgumentException("invalid json")
                }
            }
            .subscribeOn(Schedulers.io())
            .observeOn(AndroidSchedulers.mainThread())
            .subscribe(
                { result ->
                    LAST_RESULT = result
                    callback(result.required, result.version, result.url)
                },
                { error ->
                    Log.e(TAG, "failed", error)
                })
        }
    }

    private fun expired(): Boolean {
        return System.currentTimeMillis() - LAST_RESULT.time > EXPIRY_MILLIS
    }

    companion object {
        private var TAG = "UpdateCheck"

        private var LAST_RESULT: Result = Result(false, "", "", 0)

        private val HTTP_CLIENT: OkHttpClient =
            OkHttpClient.Builder()
                .addInterceptor { chain ->
                    val request = chain.request().newBuilder()
                        .header("User-Agent", USER_AGENT).build()

                    chain.proceed(request)
                }
                .build()

        private val VERSION by lazy {
            val context = Application.instance
            context.packageManager.getPackageInfo(context.packageName, 0).versionName
        }

        private val USER_AGENT by lazy {
            "musikdroid ${VERSION}"
        }
    }
}