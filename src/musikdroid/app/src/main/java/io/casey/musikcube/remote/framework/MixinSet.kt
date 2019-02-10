package io.casey.musikcube.remote.framework

import android.content.Intent
import android.os.Bundle

class MixinSet : MixinBase() {
    private data class ActivityResult (val request: Int, val result: Int, val data: Intent?)

    private var activityResult: ActivityResult? = null
    private val components: MutableMap<Class<out IMixin>, IMixin> = mutableMapOf()
    private var bundle = Bundle()

    fun <T> add(mixin: IMixin): T {
        components[mixin.javaClass] = mixin

        when (state) {
            State.Created ->
                mixin.onCreate(bundle)
            State.Started -> {
                mixin.onCreate(bundle)
                mixin.onStart()
            }
            State.Resumed -> {
                mixin.onCreate(bundle)
                mixin.onStart()
                mixin.onResume()
            }
            State.Paused -> {
                mixin.onCreate(bundle)
                mixin.onStart()
            }
            else -> {
            }
        }

        @Suppress("unchecked_cast")
        return mixin as T
    }

    @Suppress("unchecked_cast")
    fun <T: IMixin> get(cls: Class<out T>): T? = components[cls] as T?

    override fun onCreate(bundle: Bundle) {
        super.onCreate(bundle)
        this.bundle = bundle
        components.values.forEach { it.onCreate(bundle) }
    }

    override fun onStart() {
        super.onStart()
        components.values.forEach { it.onStart() }
    }

    override fun onResume() {
        super.onResume()
        components.values.forEach { it.onResume() }

        val ar = activityResult
        if (ar != null) {
            components.values.forEach { it.onActivityResult(ar.request, ar.result, ar.data) }
            activityResult = null
        }
    }

    override fun onPause() {
        super.onPause()
        components.values.forEach { it.onPause() }
    }

    override fun onStop() {
        super.onStop()
        components.values.forEach { it.onStop() }
    }

    override fun onSaveInstanceState(bundle: Bundle) {
        super.onSaveInstanceState(bundle)
        components.values.forEach { it.onSaveInstanceState(bundle) }
    }

    override fun onActivityResult(request: Int, result: Int, data: Intent?) {
        super.onActivityResult(request, result, data)
        if (active) {
            components.values.forEach { it.onActivityResult(request, result, data) }
        }
        else {
            activityResult = ActivityResult(request, result, data)
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        components.values.forEach { it.onDestroy() }
    }
}