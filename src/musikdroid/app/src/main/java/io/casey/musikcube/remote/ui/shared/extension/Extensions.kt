package io.casey.musikcube.remote.ui.shared.extension

import android.app.SearchManager
import android.content.Context
import android.support.design.widget.Snackbar
import android.support.v4.app.DialogFragment
import android.support.v4.app.Fragment
import android.support.v4.content.ContextCompat
import android.support.v4.view.MenuItemCompat
import android.support.v7.app.AppCompatActivity
import android.support.v7.widget.DividerItemDecoration
import android.support.v7.widget.LinearLayoutManager
import android.support.v7.widget.RecyclerView
import android.support.v7.widget.SearchView
import android.view.Menu
import android.view.View
import android.view.inputmethod.InputMethodManager
import android.widget.CheckBox
import android.widget.CompoundButton
import android.widget.EditText
import android.widget.TextView
import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.ui.shared.activity.Filterable
import io.casey.musikcube.remote.ui.shared.fragment.TransportFragment
import io.casey.musikcube.remote.util.Strings

val EXTRA_ACTIVITY_TITLE = "extra_title"

fun AppCompatActivity.setupDefaultRecyclerView(
    recyclerView: RecyclerView, adapter: RecyclerView.Adapter<*>)
{
    val layoutManager = LinearLayoutManager(this)
    val dividerItemDecoration = DividerItemDecoration(this, layoutManager.orientation)
    recyclerView.layoutManager = LinearLayoutManager(this)
    recyclerView.adapter = adapter
    recyclerView.addItemDecoration(dividerItemDecoration)
}

fun RecyclerView.ViewHolder.getColorCompat(resourceId: Int): Int =
    ContextCompat.getColor(itemView.context, resourceId)

fun View.getColorCompat(resourceId: Int): Int =
    ContextCompat.getColor(context, resourceId)

fun Fragment.getColorCompat(resourceId: Int): Int =
    ContextCompat.getColor(activity!!, resourceId)

fun AppCompatActivity.getColorCompat(resourceId: Int): Int =
    ContextCompat.getColor(this, resourceId)

fun AppCompatActivity.enableUpNavigation() {
    val ab = this.supportActionBar
    ab?.setDisplayHomeAsUpEnabled(true)
}

fun AppCompatActivity.addTransportFragment(
        listener: TransportFragment.OnModelChangedListener? = null): TransportFragment?
{
    val root = findViewById<View>(android.R.id.content)
    if (root != null) {
        if (root.findViewById<View>(R.id.transport_container) != null) {
            val fragment = TransportFragment.newInstance()

            this.supportFragmentManager
                    .beginTransaction()
                    .add(R.id.transport_container, fragment, TransportFragment.TAG)
                    .commit()

            fragment.modelChangedListener = listener
            return fragment
        }
    }
    return null
}


fun AppCompatActivity.setTitleFromIntent(defaultId: Int) =
    this.setTitleFromIntent(getString(defaultId))

fun AppCompatActivity.setTitleFromIntent(defaultTitle: String) {
    val title = this.intent.getStringExtra(EXTRA_ACTIVITY_TITLE)
    this.title = if (Strings.notEmpty(title)) title else defaultTitle
}

fun AppCompatActivity.setFabVisible(visible: Boolean, fab: View, recycler: RecyclerView) {
    if (visible) {
        val bottom = this.resources.getDimensionPixelSize(R.dimen.fab_plus_padding)
        fab.visibility = View.VISIBLE
        recycler.clipToPadding = false
        recycler.setPadding(0, 0, 0, bottom)
    }
    else {
        fab.visibility = View.GONE
        recycler.clipToPadding = true
        recycler.setPadding(0, 0, 0, 0)
    }
}

fun AppCompatActivity.initSearchMenu(menu: Menu, filterable: Filterable?) {
    this.menuInflater.inflate(R.menu.search_menu, menu)

    val searchMenuItem = menu.findItem(R.id.action_search)
    val searchView = MenuItemCompat.getActionView(searchMenuItem) as SearchView

    searchView.maxWidth = Integer.MAX_VALUE

    if (filterable != null) {
        searchView.setOnQueryTextListener(object : SearchView.OnQueryTextListener {
            override fun onQueryTextSubmit(query: String): Boolean = false

            override fun onQueryTextChange(newText: String): Boolean {
                filterable.setFilter(newText)
                return false
            }
        })

        searchView.setOnCloseListener {
            filterable.setFilter("")
            false
        }
    }

    val searchManager = this.getSystemService(Context.SEARCH_SERVICE) as SearchManager
    val searchableInfo = searchManager.getSearchableInfo(this.componentName)

    searchView.setSearchableInfo(searchableInfo)
    searchView.setIconifiedByDefault(true)
}

fun CheckBox.setCheckWithoutEvent(checked: Boolean,
                                  listener: (CompoundButton, Boolean) -> Unit) {
    this.setOnCheckedChangeListener(null)
    this.isChecked = checked
    this.setOnCheckedChangeListener(listener)
}

fun EditText.setTextAndMoveCursorToEnd(text: String) {
    this.setText(text)
    this.setSelection(this.text.length)
}

fun View.setVisible(visible: Boolean) {
    this.visibility = if (visible) View.VISIBLE else View.GONE
}

fun AppCompatActivity.dpToPx(dp: Float): Float = dp * this.resources.displayMetrics.density

fun showKeyboard(context: Context) {
    val imm = context.getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
    imm.toggleSoftInput(InputMethodManager.SHOW_FORCED, InputMethodManager.HIDE_IMPLICIT_ONLY)
}

fun hideKeyboard(context: Context, view: View) {
    val imm = context.getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
    imm.hideSoftInputFromWindow(view.windowToken, 0)
}

fun AppCompatActivity.showKeyboard() = showKeyboard(this)

fun AppCompatActivity.hideKeyboard(view: View? = null) {
    val v = view ?: this.findViewById(android.R.id.content)
    hideKeyboard(this, v)
}

fun DialogFragment.showKeyboard() = showKeyboard(activity!!)

fun DialogFragment.hideKeyboard() =
    hideKeyboard(activity!!, activity!!.findViewById(android.R.id.content))

fun AppCompatActivity.dialogVisible(tag: String): Boolean =
    this.supportFragmentManager.findFragmentByTag(tag) != null

fun AppCompatActivity.showDialog(dialog: DialogFragment, tag: String) {
    dialog.show(this.supportFragmentManager, tag)
}

fun showSnackbar(view: View, text: String, bgColor: Int, fgColor: Int, buttonText: String? = null, buttonCb: ((View) -> Unit)? = null) {
    val sb = Snackbar.make(view, text, Snackbar.LENGTH_LONG)

    if (buttonText != null && buttonCb != null) {
        sb.setAction(buttonText, buttonCb)
    }

    val sbView = sb.view
    val context = view.context
    sbView.setBackgroundColor(ContextCompat.getColor(context, bgColor))
    val tv = sbView.findViewById<TextView>(android.support.design.R.id.snackbar_text)
    tv.setTextColor(ContextCompat.getColor(context, fgColor))
    sb.show()
}

fun showSnackbar(view: View, stringId: Int, bgColor: Int, fgColor: Int, buttonText: String? = null, buttonCb: ((View) -> Unit)? = null) =
    showSnackbar(view, Application.instance.getString(stringId), bgColor, fgColor, buttonText, buttonCb)

fun showSnackbar(view: View, text: String, buttonText: String? = null, buttonCb: ((View) -> Unit)? = null) =
    showSnackbar(view, text, R.color.color_primary, R.color.theme_foreground, buttonText, buttonCb)

fun showSnackbar(view: View, stringId: Int, buttonText: String? = null, buttonCb: ((View) -> Unit)? = null) =
    showSnackbar(view, Application.instance.getString(stringId), buttonText, buttonCb)

fun showErrorSnackbar(view: View, text: String, buttonText: String? = null, buttonCb: ((View) -> Unit)? = null) =
    showSnackbar(view, text, R.color.theme_red, R.color.theme_foreground, buttonText, buttonCb)

fun showErrorSnackbar(view: View, stringId: Int, buttonText: String? = null, buttonCb: ((View) -> Unit)? = null) =
    showErrorSnackbar(view, Application.instance.getString(stringId), buttonText, buttonCb)

fun AppCompatActivity.showErrorSnackbar(stringId: Int, buttonText: String? = null, buttonCb: ((View) -> Unit)? = null) =
    showErrorSnackbar(this.findViewById<View>(android.R.id.content), stringId, buttonText, buttonCb)

fun AppCompatActivity.showSnackbar(stringId: Int, buttonText: String? = null, buttonCb: ((View) -> Unit)? = null) =
    showSnackbar(this.findViewById<View>(android.R.id.content), stringId, buttonText, buttonCb)

fun AppCompatActivity.showSnackbar(stringId: String, buttonText: String? = null, buttonCb: ((View) -> Unit)? = null) =
        showSnackbar(this.findViewById<View>(android.R.id.content), stringId, buttonText, buttonCb)

fun AppCompatActivity.showSnackbar(viewId: Int, stringId: Int, buttonText: String? = null, buttonCb: ((View) -> Unit)? = null) =
    showSnackbar(this.findViewById<View>(viewId), stringId, buttonText, buttonCb)

fun fallback(input: String?, fallback: String): String =
    if (input.isNullOrEmpty()) fallback else input!!

fun fallback(input: String?, fallback: Int): String =
    if (input.isNullOrEmpty()) Application.Companion.instance.getString(fallback) else input!!

fun AppCompatActivity.slideNextUp() = overridePendingTransition(R.anim.slide_up, R.anim.stay_put)

fun AppCompatActivity.slideThisDown() = overridePendingTransition(R.anim.stay_put, R.anim.slide_down)

fun <T1: Any, T2: Any, R: Any> letMany(p1: T1?, p2: T2?, block: (T1, T2) -> R?): R? {
    return if (p1 != null && p2 != null) block(p1, p2) else null
}

fun <T1: Any, T2: Any, T3: Any, R: Any> letMany(p1: T1?, p2: T2?, p3: T3?, block: (T1, T2, T3) -> R?): R? {
    return if (p1 != null && p2 != null && p3 != null) block(p1, p2, p3) else null
}

fun <T1: Any, T2: Any, T3: Any, T4: Any, R: Any> letMany(p1: T1?, p2: T2?, p3: T3?, p4: T4?, block: (T1, T2, T3, T4)->R?): R? {
    return if (p1 != null && p2 != null && p3 != null && p4 != null) block(p1, p2, p3, p4) else null
}
fun <T1: Any, T2: Any, T3: Any, T4: Any, T5: Any, R: Any> letMany(p1: T1?, p2: T2?, p3: T3?, p4: T4?, p5: T5?, block: (T1, T2, T3, T4, T5)->R?): R? {
    return if (p1 != null && p2 != null && p3 != null && p4 != null && p5 != null) block(p1, p2, p3, p4, p5) else null
}