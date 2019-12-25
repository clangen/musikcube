package io.casey.musikcube.remote.ui.shared.view

import android.content.Context
import android.util.AttributeSet
import android.view.MotionEvent
import android.widget.FrameLayout

class InterceptTouchFrameLayout : FrameLayout {
    private var disallowIntercept: Boolean = false
    private var interceptor: OnInterceptTouchEventListener = DEFAULT_INTERCEPTOR

    interface OnInterceptTouchEventListener {
        fun onInterceptTouchEvent(view: InterceptTouchFrameLayout, event: MotionEvent, disallowIntercept: Boolean): Boolean
        fun onTouchEvent(view: InterceptTouchFrameLayout, event: MotionEvent): Boolean
    }

    constructor(context: Context) : super(context)

    constructor(context: Context, attrs: AttributeSet) : super(context, attrs)

    constructor(context: Context, attrs: AttributeSet, defStyleAttr: Int) : super(context, attrs, defStyleAttr)

    override fun requestDisallowInterceptTouchEvent(disallowIntercept: Boolean) {
        parent.requestDisallowInterceptTouchEvent(disallowIntercept)
        this.disallowIntercept = disallowIntercept
    }

    fun setOnInterceptTouchEventListener(interceptor: OnInterceptTouchEventListener?) {
        this.interceptor = interceptor ?: DEFAULT_INTERCEPTOR
    }

    override fun onInterceptTouchEvent(ev: MotionEvent): Boolean {
        val eatTouchEvent = interceptor.onInterceptTouchEvent(this, ev, disallowIntercept)
        return eatTouchEvent && !disallowIntercept || super.onInterceptTouchEvent(ev)
    }

    override fun onTouchEvent(ev: MotionEvent): Boolean {
        val handled = interceptor.onTouchEvent(this, ev)
        return handled || super.onTouchEvent(ev)
    }

    fun defaultOnTouchEvent(ev: MotionEvent): Boolean = super.onTouchEvent(ev)

    companion object {
        private val DEFAULT_INTERCEPTOR = object: OnInterceptTouchEventListener {
            override fun onInterceptTouchEvent(view: InterceptTouchFrameLayout, event: MotionEvent, disallowIntercept: Boolean): Boolean = false
            override fun onTouchEvent(view: InterceptTouchFrameLayout, event: MotionEvent): Boolean = false
        }
    }
}