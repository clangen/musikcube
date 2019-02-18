package io.casey.musikcube.remote.ui.shared.activity

interface IFilterable {
    val addFilterToToolbar: Boolean
    fun setFilter(filter: String)
}
