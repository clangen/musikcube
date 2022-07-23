package io.casey.musikcube.remote.util

import android.content.Intent
import android.os.Bundle
import android.os.Parcelable
import androidx.annotation.RequiresApi

@RequiresApi(33)
@Suppress("deprecation")
inline fun <reified T: Parcelable> Intent.getParcelableExtraCompat(name: String): T? =
    if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.S) {
        this.getParcelableExtra(name)
    }
    else {
        this.getParcelableExtra(name, T::class.java)
    }

@RequiresApi(33)
@Suppress("deprecation")
inline fun <reified T: Parcelable> Bundle.getParcelableCompat(name: String): T? =
    if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.S) {
        this.getParcelable(name)
    }
    else {
        this.getParcelable(name, T::class.java)
    }