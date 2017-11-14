package io.casey.musikcube.remote.framework.components

import android.os.Bundle

class ComponentSet : ComponentBase() {
    private val components: MutableMap<Class<out IComponent>, IComponent> = mutableMapOf()
    private var bundle = Bundle()

    fun add(component: IComponent) {
        components.put(component.javaClass, component)

        when (state) {
            State.Created ->
                component.onCreate(bundle)
            State.Started -> {
                component.onCreate(bundle)
                component.onStart()
            }
            State.Resumed -> {
                component.onCreate(bundle)
                component.onStart()
                component.onResume()
            }
            State.Paused -> {
                component.onCreate(bundle)
                component.onStart()
            }
            else -> {
            }
        }
    }

    fun <T> get(cls: Class<out IComponent>): T? = components.get(cls) as T

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

    override fun onDestroy() {
        super.onDestroy()
        components.values.forEach { it.onDestroy() }
    }
}