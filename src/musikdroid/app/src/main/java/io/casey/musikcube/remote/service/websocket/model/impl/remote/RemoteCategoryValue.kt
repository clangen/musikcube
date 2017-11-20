package io.casey.musikcube.remote.service.websocket.model.impl.remote

import io.casey.musikcube.remote.service.websocket.Messages
import io.casey.musikcube.remote.service.websocket.model.ICategoryValue
import org.json.JSONObject

class RemoteCategoryValue(private val categoryType: String,
                          private val json: JSONObject) : ICategoryValue
{
    override val id: Long
        get() = json.optLong(Messages.Key.ID, -1)
    override val value: String
        get() = json.optString(Messages.Key.VALUE, "")
    override val type: String
        get() = categoryType
}