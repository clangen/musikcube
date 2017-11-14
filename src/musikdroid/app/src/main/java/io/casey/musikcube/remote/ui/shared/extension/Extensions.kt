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
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.ui.shared.activity.Filterable
import io.casey.musikcube.remote.ui.shared.fragment.TransportFragment
import io.casey.musikcube.remote.util.Strings

val EXTRA_ACTIVITY_TITLE = "extra_title"

fun AppCompatActivity.setupDefaultRecyclerView(
                             recyclerView: RecyclerView,
                             adapter: RecyclerView.Adapter<*>) {
    val layoutManager = LinearLayoutManager(this)

    recyclerView.layoutManager = LinearLayoutManager(this)
    recyclerView.adapter = adapter

    val dividerItemDecoration = DividerItemDecoration(this, layoutManager.orientation)

    recyclerView.addItemDecoration(dividerItemDecoration)
}

fun View.getColorCompat(resourceId: Int): Int {
    return ContextCompat.getColor(context, resourceId)
}

fun Fragment.getColorCompat(resourceId: Int): Int {
    return ContextCompat.getColor(activity, resourceId)
}

fun AppCompatActivity.getColorCompat(resourceId: Int): Int {
    return ContextCompat.getColor(this, resourceId)
}

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


fun AppCompatActivity.setTitleFromIntent(defaultId: Int) {
    val title = this.intent.getStringExtra(EXTRA_ACTIVITY_TITLE)
    if (Strings.notEmpty(title)) {
        this.title = title
    }
    else {
        this.setTitle(defaultId)
    }
}

fun AppCompatActivity.initSearchMenu(menu: Menu, filterable: Filterable?) {
    this.menuInflater.inflate(R.menu.search_menu, menu)

    val searchMenuItem = menu.findItem(R.id.action_search)
    val searchView = MenuItemCompat.getActionView(searchMenuItem) as SearchView

    searchView.maxWidth = Integer.MAX_VALUE

    if (filterable != null) {
        searchView.setOnQueryTextListener(object : SearchView.OnQueryTextListener {
            override fun onQueryTextSubmit(query: String): Boolean {
                return false
            }

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

fun AppCompatActivity.dpToPx(dp: Float): Float {
    return dp * this.resources.displayMetrics.density
}

fun showKeyboard(context: Context) {
    val imm = context.getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
    imm.toggleSoftInput(InputMethodManager.SHOW_FORCED, InputMethodManager.HIDE_IMPLICIT_ONLY)
}

fun hideKeyboard(context: Context, view: View) {
    val imm = context.getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
    imm.hideSoftInputFromWindow(view.windowToken, 0)
}

fun AppCompatActivity.showKeyboard() {
    showKeyboard(this)
}

fun AppCompatActivity.hideKeyboard(view: View? = null) {
    val v = view ?: this.findViewById(android.R.id.content)
    hideKeyboard(this, v)
}

fun DialogFragment.showKeyboard() {
    showKeyboard(activity)
}

fun DialogFragment.hideKeyboard() {
    hideKeyboard(activity, activity.findViewById(android.R.id.content))
}

fun AppCompatActivity.dialogVisible(tag: String): Boolean {
    return this.supportFragmentManager.findFragmentByTag(tag) != null
}

fun AppCompatActivity.showDialog(dialog: DialogFragment, tag: String) {
    dialog.show(this.supportFragmentManager, tag)
}

fun AppCompatActivity.showSnackbar(view: View, stringId: Int) {
    val sb = Snackbar.make(view, stringId, Snackbar.LENGTH_LONG)
    val sbView = sb.view
    sbView.setBackgroundColor(getColorCompat(R.color.color_primary))
    val tv = sbView.findViewById<TextView>(android.support.design.R.id.snackbar_text)
    tv.setTextColor(getColorCompat(R.color.theme_foreground))
    sb.show()
}

fun AppCompatActivity.showSnackbar(viewId: Int, stringId: Int) {
    this.showSnackbar(this.findViewById<View>(viewId), stringId)
}

fun fallback(input: String?, fallback: String): String {
    return if (input.isNullOrEmpty()) fallback else input!!
}