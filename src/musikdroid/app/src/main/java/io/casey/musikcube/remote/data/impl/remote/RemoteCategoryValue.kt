package io.casey.musikcube.remote.data.impl.remote

import io.casey.musikcube.remote.data.ICategoryValue
import io.casey.musikcube.remote.websocket.Messages
import org.json.JSONObject

class RemoteCategoryValue(private val categoryType: String,
                          private val json: JSONObject) : ICategoryValue
{
    override val id: Long
        get() = json.optLong(Messages.Key.ID)
    override val name: String
        get() = json.optString(Messages.Key.VALUE, "")
    override val type: String
        get() = categoryType
}