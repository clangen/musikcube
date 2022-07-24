package io.casey.musikcube.remote.util

import android.content.Intent
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.os.Parcelable
import androidx.activity.ComponentActivity
import androidx.activity.result.ActivityResult
import androidx.activity.result.ActivityResultCallback
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.annotation.RequiresApi
import androidx.fragment.app.Fragment
import java.io.Serializable

@Suppress("deprecation")
inline fun <reified T: Parcelable> Intent.getParcelableExtraCompat(name: String): T? =
    if (Build.VERSION.SDK_INT >= 33) {
        this.getParcelableExtra(name, T::class.java)
    }
    else {
        this.getParcelableExtra(name)
    }

@Suppress("deprecation")
inline fun <reified T: Parcelable> Bundle.getParcelableCompat(name: String): T? =
    if (Build.VERSION.SDK_INT >= 33) {
        this.getParcelable(name, T::class.java)
    }
    else {
        this.getParcelable(name)
    }

@Suppress("deprecation")
inline fun <reified T: Serializable> Bundle.getSerializableCompat(name: String): T? =
    if (Build.VERSION.SDK_INT >= 33) {
        this.getSerializable(name, T::class.java)
    }
    else {
        this.getSerializable(name) as T?
    }

@Suppress("deprecation")
fun PackageManager.getPackageInfoCompat(name: String): PackageInfo =
    if (Build.VERSION.SDK_INT >= 33) {
        this.getPackageInfo(name, PackageManager.PackageInfoFlags.of(0))
    }
    else {
        this.getPackageInfo(name, 0)
    }

fun ComponentActivity.launcher(callback: ActivityResultCallback<ActivityResult>): ActivityResultLauncher<Intent> =
    this.registerForActivityResult(ActivityResultContracts.StartActivityForResult(), callback)

fun Fragment.launcher(callback: ActivityResultCallback<ActivityResult>): ActivityResultLauncher<Intent> =
    this.registerForActivityResult(ActivityResultContracts.StartActivityForResult(), callback)
