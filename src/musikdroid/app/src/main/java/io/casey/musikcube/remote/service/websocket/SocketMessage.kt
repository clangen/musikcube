package io.casey.musikcube.remote.service.websocket

import android.util.Log
import io.casey.musikcube.remote.Application
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject
import java.util.*
import java.util.concurrent.atomic.AtomicInteger

class SocketMessage private constructor(val name: String, val id: String, val type: SocketMessage.Type, options: JSONObject?) {
    enum class Type constructor(val rawType: String) {
        Request("request"),
        Response("response"),
        Broadcast("broadcast");

        companion object {
            fun fromString(str: String): Type =
                when (str) {
                    Request.rawType -> Request
                    Response.rawType -> Response
                    Broadcast.rawType -> Broadcast
                    else -> throw IllegalArgumentException("str")
                }
        }
    }

    private val options: JSONObject

    init {
        if (name.isEmpty() || id.isEmpty()) {
            throw IllegalArgumentException()
        }
        this.options = options ?: JSONObject()
    }

    @Suppress("UNCHECKED_CAST", "unused")
    fun <T> getOption(key: String): T? {
        if (options.has(key)) {
            try {
                return options.get(key) as T
            }
            catch (ex: JSONException) {
                /* swallow */
            }
        }
        return null
    }

    @JvmOverloads fun getStringOption(key: String, defaultValue: String = ""): String {
        if (options.has(key)) {
            try {
                return options.getString(key)
            }
            catch (ex: JSONException) {
                /* swallow */
            }
        }
        return defaultValue
    }

    @JvmOverloads fun getIntOption(key: String, defaultValue: Int = 0): Int {
        if (options.has(key)) {
            try {
                return options.getInt(key)
            }
            catch (ex: JSONException) {
                /* swallow */
            }
        }
        return defaultValue
    }

    @JvmOverloads fun getLongOption(key: String, defaultValue: Long = 0): Long {
        if (options.has(key)) {
            try {
                return options.getLong(key)
            }
            catch (ex: JSONException) {
                /* swallow */
            }
        }
        return defaultValue
    }

    @JvmOverloads fun getDoubleOption(key: String, defaultValue: Double = 0.0): Double {
        if (options.has(key)) {
            try {
                return options.getDouble(key)
            }
            catch (ex: JSONException) {
                /* swallow */
            }
        }
        return defaultValue
    }

    @JvmOverloads fun getBooleanOption(key: String, defaultValue: Boolean = false): Boolean {
        if (options.has(key)) {
            try {
                return options.getBoolean(key)
            }
            catch (ex: JSONException) {
                /* swallow */
            }
        }
        return defaultValue
    }

    fun getJsonObject(): JSONObject {
        return JSONObject(options.toString())
    }

    @JvmOverloads fun getJsonObjectOption(key: String, defaultValue: JSONObject? = null): JSONObject? {
        if (options.has(key)) {
            try {
                return options.getJSONObject(key)
            }
            catch (ex: JSONException) {
                /* swallow */
            }
        }
        return defaultValue
    }

    @JvmOverloads fun getJsonArrayOption(key: String, defaultValue: JSONArray? = null): JSONArray? {
        if (options.has(key)) {
            try {
                return options.getJSONArray(key)
            }
            catch (ex: JSONException) {
                /* swallow */
            }
        }
        return defaultValue
    }

    override fun toString(): String {
        try {
            val json = JSONObject()
            json.put("name", name)
            json.put("id", id)
            json.put("device_id", Application.deviceId)
            json.put("type", type.rawType)
            json.put("options", options)
            return json.toString()
        }
        catch (ex: JSONException) {
            throw RuntimeException("unable to generate output JSON!")
        }
    }

    @Suppress("unused")
    fun buildUpon(): Builder {
        try {
            val builder = Builder()
            builder.internalName = name
            builder.internalType = type
            builder.internalId = id
            builder.internalOptions = JSONObject(options.toString())
            return builder
        }
        catch (ex: JSONException) {
            throw RuntimeException(ex)
        }
    }

    class Builder internal constructor() {
        internal var internalName: String? = null
        internal var internalType: Type? = null
        internal var internalId: String? = null
        internal var internalOptions = JSONObject()

        fun withOptions(options: JSONObject?): Builder {
            internalOptions = options ?: JSONObject()
            return this
        }

        fun addOption(key: String, value: Any?): Builder {
            try {
                internalOptions.put(key, value)
            }
            catch (ex: JSONException) {
                throw RuntimeException("addOption failed??")
            }
            return this
        }

        @Suppress("unused")
        fun removeOption(key: String): Builder {
            internalOptions.remove(key)
            return this
        }

        fun build(): SocketMessage {
            return SocketMessage(internalName!!, internalId!!, internalType!!, internalOptions)
        }

        companion object {
            private val nextId = AtomicInteger()

            private fun newId(): String {
                return String.format(Locale.ENGLISH, "musikcube-android-client-%d", nextId.incrementAndGet())
            }

            @Suppress("unused")
            fun broadcast(name: String): Builder {
                val builder = Builder()
                builder.internalName = name
                builder.internalId = newId()
                builder.internalType = Type.Response
                return builder
            }

            fun respondTo(message: SocketMessage): Builder {
                val builder = Builder()
                builder.internalName = message.name
                builder.internalId = message.id
                builder.internalType = Type.Response
                return builder
            }

            fun request(name: String): Builder {
                val builder = Builder()
                builder.internalName = name
                builder.internalId = newId()
                builder.internalType = Type.Request
                return builder
            }

            fun request(name: Messages.Request): Builder {
                val builder = Builder()
                builder.internalName = name.toString()
                builder.internalId = newId()
                builder.internalType = Type.Request
                return builder
            }
        }
    }

    companion object {
        private val TAG = SocketMessage::class.java.canonicalName

        fun create(string: String): SocketMessage? =
            try {
                val `object` = JSONObject(string)
                val name = `object`.getString("name")
                val type = Type.fromString(`object`.getString("type"))
                val id = `object`.getString("id")
                val options = `object`.optJSONObject("options")
                SocketMessage(name, id, type, options)
            }
            catch (ex: Exception) {
                Log.e(TAG, ex.toString())
                null
            }
    }
}
