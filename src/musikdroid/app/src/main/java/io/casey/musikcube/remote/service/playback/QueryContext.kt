package io.casey.musikcube.remote.service.playback

import io.casey.musikcube.remote.service.websocket.Messages

class QueryContext {
    val category: String?
    val categoryId: Long
    val filter: String
    val type: Messages.Request?

    constructor(type: Messages.Request? = null)
        : this("", type)

    constructor(filter: String, type: Messages.Request? = null)
        : this("", -1L, filter, type)

    constructor(category: String, categoryId: Long, filter: String, type: Messages.Request? = null) {
        this.category = category
        this.categoryId = categoryId
        this.filter = filter
        this.type = type
    }

    fun hasCategory(): Boolean = (category?.isNotBlank() ?: false && (categoryId >= 0))
}