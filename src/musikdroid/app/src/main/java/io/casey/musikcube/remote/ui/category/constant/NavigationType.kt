package io.casey.musikcube.remote.ui.category.constant

enum class NavigationType {
    Tracks, Albums, Select;

    companion object {
        fun get(ordinal: Int) = values()[ordinal]
    }
}