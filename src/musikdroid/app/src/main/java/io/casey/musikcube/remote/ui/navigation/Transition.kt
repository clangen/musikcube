package io.casey.musikcube.remote.ui.navigation

enum class Transition {
    Horizontal, Vertical;
    companion object {
        fun from(name: String, fallback: Transition = Horizontal): Transition =
            values().firstOrNull { it.name == name } ?: fallback
    }
}