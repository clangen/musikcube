package io.casey.musikcube.remote.util

import androidx.activity.ComponentActivity
import android.content.Intent
import android.os.Bundle
import android.os.Parcelable
import androidx.activity.result.ActivityResult
import androidx.activity.result.ActivityResultCallback
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.annotation.RequiresApi
import java.io.Serializable

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

@RequiresApi(33)
@Suppress("deprecation")
inline fun <reified T: Serializable> Bundle.getSerializableCompat(name: String): T? =
    if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.S) {
        this.getSerializable(name) as T?
    }
    else {
        this.getSerializable(name, T::class.java)
    }

fun ComponentActivity.launcher(callback: ActivityResultCallback<ActivityResult>): ActivityResultLauncher<Intent> =
    this.registerForActivityResult(ActivityResultContracts.StartActivityForResult(), callback)
