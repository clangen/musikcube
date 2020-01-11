package io.casey.musikcube.remote.ui.shared.extension

import android.app.SearchManager
import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.content.res.Resources
import android.os.Bundle
import android.os.Handler
import android.text.TextUtils
import android.util.Base64
import android.util.Log
import android.view.Menu
import android.view.View
import android.view.inputmethod.InputMethodManager
import android.widget.CheckBox
import android.widget.CompoundButton
import android.widget.EditText
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.SearchView
import androidx.appcompat.widget.Toolbar
import androidx.core.content.ContextCompat
import androidx.fragment.app.DialogFragment
import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentManager
import androidx.recyclerview.widget.DividerItemDecoration
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.snackbar.Snackbar
import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.ui.navigation.Transition
import io.casey.musikcube.remote.ui.settings.constants.Prefs
import io.casey.musikcube.remote.ui.shared.activity.IFilterable
import io.casey.musikcube.remote.ui.shared.activity.IMenuProvider
import io.casey.musikcube.remote.ui.shared.constant.Shared
import io.casey.musikcube.remote.ui.shared.fragment.BaseFragment
import io.casey.musikcube.remote.ui.shared.util.NetworkUtil
import okhttp3.OkHttpClient
import java.io.*

/*
 *
 * SharedPreferences
 *
 */

fun SharedPreferences.getString(key: String): String? =
    when (!this.contains(key)) {
        true -> null
        else -> this.getString(key, "")
    }

/*
 *
 * Toolbar
 *
 */

fun Toolbar.setTitleFromIntent(defaultTitle: String) {
    val extras = (context as? AppCompatActivity)?.intent?.extras ?: Bundle()
    val title = extras.getString(Shared.Extra.TITLE_OVERRIDE) ?: ""
    this.title = if (title.isNotEmpty()) title else defaultTitle
}

fun Toolbar.collapseActionViewIfExpanded(): Boolean {
    (menu.findItem(R.id.action_search)?.actionView as? SearchView)?.let {
        if (!it.isIconified) {
            it.isIconified = true
            return true
        }
    }
    if (this.hasExpandedActionView()) {
        this.collapseActionView()
        return true
    }
    return false
}

/*
 *
 * AppCompatActivity
 *
 */

fun AppCompatActivity.setupDefaultRecyclerView(
        recyclerView: RecyclerView, adapter: RecyclerView.Adapter<*>)
{
    val layoutManager = LinearLayoutManager(this)
    val dividerItemDecoration = DividerItemDecoration(this, layoutManager.orientation)
    recyclerView.layoutManager = LinearLayoutManager(this)
    recyclerView.adapter = adapter
    recyclerView.addItemDecoration(dividerItemDecoration)
}


val AppCompatActivity.toolbar: Toolbar?
    get() = findViewById(R.id.toolbar)

fun AppCompatActivity.enableUpNavigation() {
    val ab = this.supportActionBar
    ab?.setDisplayHomeAsUpEnabled(true)
}

fun AppCompatActivity.setTitleFromIntent(defaultTitle: String) {
    val title = this.intent.getStringExtra(Shared.Extra.TITLE_OVERRIDE)
    this.title = if (title.isNotEmpty()) title else defaultTitle
}

fun BaseFragment.addFilterAction(menu: Menu, filterable: IFilterable?): Boolean {
    appCompatActivity.menuInflater.inflate(R.menu.search_menu, menu)

    val searchMenuItem = menu.findItem(R.id.action_search)
    val searchView = searchMenuItem.actionView as SearchView

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

    val searchManager = appCompatActivity.getSystemService(Context.SEARCH_SERVICE) as SearchManager
    val searchableInfo = searchManager.getSearchableInfo(appCompatActivity.componentName)

    searchView.setSearchableInfo(searchableInfo)
    searchView.setIconifiedByDefault(true)

    return true
}

fun AppCompatActivity.dialogVisible(tag: String): Boolean =
    this.supportFragmentManager.findFragmentByTag(tag) != null

@Suppress("unchecked_cast")
fun <T: DialogFragment> AppCompatActivity.findDialog(tag: String): T? =
    this.supportFragmentManager.findFragmentByTag(tag) as? T

fun AppCompatActivity.showDialog(dialog: DialogFragment, tag: String) {
    dialog.show(this.supportFragmentManager, tag)
}

fun AppCompatActivity.slideNextUp() = overridePendingTransition(R.anim.slide_up, R.anim.stay_put_350)

fun AppCompatActivity.slideThisDown() = overridePendingTransition(R.anim.stay_put_350, R.anim.slide_down)

fun AppCompatActivity.slideNextLeft() = overridePendingTransition(R.anim.slide_left, R.anim.slide_left_bg)

fun AppCompatActivity.slideThisRight() = overridePendingTransition(R.anim.slide_right_bg, R.anim.slide_right)

inline fun <reified T> AppCompatActivity.findFragment(tag: String): T {
    return this.supportFragmentManager.find(tag)
}

/*
 *
 * FragmentManager
 *
 */
val FragmentManager.topOfStack: String?
    get() {
        if (this.backStackEntryCount > 0) {
            return this.getBackStackEntryAt(this.backStackEntryCount - 1).name
        }
        return null
    }

inline fun <reified T> FragmentManager.find(tag: String): T {
    return findFragmentByTag(tag) as T
}

/*
 *
 * Bundle
 *
 */

fun Bundle.withElevation(fm: FragmentManager): Bundle {
    this.putFloat(Shared.Extra.ELEVATION, (fm.backStackEntryCount + 1) * 128.0f)
    return this
}

val Bundle.elevation: Float
    get() = this.getFloat(Shared.Extra.ELEVATION, 0.0f)

/*
 *
 * Intent
 *
 */

fun Intent.withoutTransport() =
    this.apply {
        putExtra(Shared.Extra.WITHOUT_TRANSPORT, true)
    }

fun Intent.withTransitionType(type: Transition) =
    this.apply {
        putExtra(Shared.Extra.TRANSITION_TYPE, type.name)
    }

val Bundle.transitionType
    get() = Transition.from(getString(Shared.Extra.TRANSITION_TYPE, ""))

/*
 *
 * BaseFragment
 *
 */

val BaseFragment.pushContainerId: Int
    get() = this.extras.getInt(Shared.Extra.PUSH_CONTAINER_ID, -1)

inline fun <reified T: BaseFragment> T.pushTo(containerId: Int): T {
    if (containerId > 0) {
        this.extras.putInt(Shared.Extra.PUSH_CONTAINER_ID, containerId)
    }
    return this
}

fun BaseFragment.pushWithToolbar(
        containerId: Int,
        backstackId: String,
        fragment: BaseFragment,
        transition: Transition = Transition.Horizontal) {
    fragment.pushTo(containerId)
    appCompatActivity.supportFragmentManager
        .beginTransaction().apply {
            if (transition == Transition.Horizontal) {
                setCustomAnimations(
                    R.anim.slide_left, R.anim.slide_left_bg,
                    R.anim.slide_right_bg, R.anim.slide_right)
            }
            else {
                setCustomAnimations(
                    R.anim.slide_up, R.anim.stay_put_350,
                    R.anim.stay_put_350, R.anim.slide_down)
            }
            replace(
                containerId,
                fragment
                    .withToolbar()
                    .addElevation(appCompatActivity.supportFragmentManager),
                backstackId)
            addToBackStack(backstackId)
            commit()
        }
}

inline fun <reified T: BaseFragment> T.withToolbar(): T {
    this.extras.putBoolean(Shared.Extra.WITH_TOOLBAR, true)
    return this
}

inline fun <reified T: BaseFragment> T.withTitleOverride(activity: AppCompatActivity): T {
    activity.intent?.getStringExtra(Shared.Extra.TITLE_OVERRIDE)?.let {
        if (it.isNotEmpty()) {
            this.extras.putString(Shared.Extra.TITLE_OVERRIDE, it)
        }
    }
    return this
}

fun BaseFragment.initToolbarIfNecessary(view: View, showFilter: Boolean = true) {
    view.findViewById<Toolbar>(R.id.toolbar)?.let {
        it.navigationIcon = appCompatActivity.getDrawable(R.drawable.ic_back)
        it.setNavigationOnClickListener {
            appCompatActivity.onBackPressed()
        }
        if (showFilter) {
            this.addFilterAction(it.menu, this as? IFilterable)
        }
        if (this is IMenuProvider) {
            this.createOptionsMenu(it.menu)
            it.setOnMenuItemClickListener {
                menuItem -> this.optionsItemSelected(menuItem)
            }
        }
    }
}

fun BaseFragment.getLayoutId(): Int =
    when (this.extras.getBoolean(Shared.Extra.WITH_TOOLBAR)) {
        true -> R.layout.recycler_view_with_empty_state_and_toolbar_and_fab
        false -> R.layout.recycler_view_with_empty_state
    }

inline fun <reified T: BaseFragment> T.addElevation(fm: FragmentManager): T {
    this.extras.withElevation(fm)
    return this
}

fun BaseFragment.setupDefaultRecyclerView(
        recyclerView: RecyclerView, adapter: RecyclerView.Adapter<*>)
{
    this.appCompatActivity.setupDefaultRecyclerView(recyclerView, adapter)
}

fun BaseFragment.getTitleOverride(defaultTitle: String): String {
    val title = this.extras.getString(Shared.Extra.TITLE_OVERRIDE) ?: ""
    return if (title.isNotEmpty()) title else defaultTitle
}

/*
 *
 * Snackbar
 *
 */

fun showSnackbar(view: View, text: String, bgColor: Int, fgColor: Int, buttonText: String? = null, buttonCb: ((View) -> Unit)? = null) {
    val sb = Snackbar.make(view, text, Snackbar.LENGTH_LONG)

    if (buttonText != null && buttonCb != null) {
        sb.setAction(buttonText, buttonCb)
    }

    val sbView = sb.view
    val context = view.context
    sbView.setBackgroundColor(ContextCompat.getColor(context, bgColor))
    val tv = sbView.findViewById<TextView>(com.google.android.material.R.id.snackbar_text)
    tv.setTextColor(ContextCompat.getColor(context, fgColor))
    sb.show()
}

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

/*
 *
 * View 1-offs
 *
 */

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

/*
 *
 * Colors
 *
 */

fun RecyclerView.ViewHolder.getColorCompat(resourceId: Int): Int =
    ContextCompat.getColor(itemView.context, resourceId)

fun View.getColorCompat(resourceId: Int): Int =
    ContextCompat.getColor(context, resourceId)

fun Fragment.getColorCompat(resourceId: Int): Int =
    ContextCompat.getColor(activity!!, resourceId)

fun AppCompatActivity.getColorCompat(resourceId: Int): Int =
    ContextCompat.getColor(this, resourceId)

/*
 *
 * Keyboard
 *
 */

fun showKeyboard(context: Context) {
    val imm = context.getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
    imm.toggleSoftInput(InputMethodManager.SHOW_FORCED, InputMethodManager.HIDE_IMPLICIT_ONLY)
}

fun hideKeyboard(context: Context, view: View) {
    val imm = context.getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
    imm.hideSoftInputFromWindow(view.windowToken, 0)
}

fun AppCompatActivity.hideKeyboard(view: View? = null) {
    val v = view ?: this.findViewById(android.R.id.content)
    hideKeyboard(this, v)
}

fun DialogFragment.showKeyboard() =
    showKeyboard(activity!!)

fun DialogFragment.hideKeyboard() {
    val fragmentActivity = activity!! /* keep it in the closure so it doesn't get gc'd */
    Handler().postDelayed({
        hideKeyboard(
            fragmentActivity,
            fragmentActivity.findViewById(android.R.id.content))
    }, 350)
}

/*
 *
 * http client
 *
 */

fun createHttpClient(context: Context): OkHttpClient {
    val prefs = context.getSharedPreferences(Prefs.NAME, Context.MODE_PRIVATE)

    val builder = OkHttpClient.Builder()
        .addInterceptor { chain ->
            var request = chain.request()
            val userPass = "default:" + prefs.getString(Prefs.Key.PASSWORD, Prefs.Default.PASSWORD)!!
            val encoded = Base64.encodeToString(userPass.toByteArray(), Base64.NO_WRAP)
            request = request.newBuilder().addHeader("Authorization", "Basic $encoded").build()
            chain.proceed(request)
        }

    if (prefs.getBoolean(Prefs.Key.CERT_VALIDATION_DISABLED, Prefs.Default.CERT_VALIDATION_DISABLED)) {
        NetworkUtil.disableCertificateValidation(builder)
    }

    return builder.build()
}

/*
 *
 * input/output stream
 *
 */

fun InputStream.toFile(path: String, progress: ((Long) -> Unit)? = null): Boolean {
    try {
        File(path).parentFile.mkdirs()
        File(path).delete()
        FileOutputStream(path, false).use { out ->
            val reader = BufferedInputStream(this)
            val buffer = ByteArray(4096)
            var iterations = 0
            var count: Int
            var total = 0L
            do {
                count = reader.read(buffer)
                total += count
                if (count > 0) {
                    out.write(buffer)
                }
                ++iterations
                /* post progress every ~(4096 * 25 = 102400) bytes */
                if (iterations % 25 == 0) {
                    progress?.invoke(total)
                }
            } while (count > 0)
        }
    }
    catch (exception: IOException) {
        Log.e("InputStream.toFile", "failed to write to $path", exception)
        return false
    }
    return true
}

/*
 *
 * misc
 *
 */

fun dpToPx(dp: Float): Float = dp * Resources.getSystem().displayMetrics.density

fun dpToPx(dp: Int): Float = dpToPx(dp.toFloat())

@Suppress("unused")
fun <T1: Any, T2: Any, R: Any> letMany(p1: T1?, p2: T2?, block: (T1, T2) -> R?): R? {
    return if (p1 != null && p2 != null) block(p1, p2) else null
}

fun <T1: Any, T2: Any, T3: Any, R: Any> letMany(p1: T1?, p2: T2?, p3: T3?, block: (T1, T2, T3) -> R?): R? {
    return if (p1 != null && p2 != null && p3 != null) block(p1, p2, p3) else null
}

@Suppress("unused")
fun <T1: Any, T2: Any, T3: Any, T4: Any, R: Any> letMany(p1: T1?, p2: T2?, p3: T3?, p4: T4?, block: (T1, T2, T3, T4) -> R?): R? {
    return if (p1 != null && p2 != null && p3 != null && p4 != null) block(p1, p2, p3, p4) else null
}
@Suppress("unused")
fun <T1: Any, T2: Any, T3: Any, T4: Any, T5: Any, R: Any> letMany(p1: T1?, p2: T2?, p3: T3?, p4: T4?, p5: T5?, block: (T1, T2, T3, T4, T5) -> R?): R? {
    return if (p1 != null && p2 != null && p3 != null && p4 != null && p5 != null) block(p1, p2, p3, p4, p5) else null
}

fun titleEllipsizeMode(prefs: SharedPreferences): TextUtils.TruncateAt {
    val modeIndex = prefs.getInt(
        Prefs.Key.TITLE_ELLIPSIS_MODE_INDEX,
        Prefs.Default.TITLE_ELLIPSIS_SIZE_INDEX)

    return when(modeIndex) {
        0 -> TextUtils.TruncateAt.START
        1 -> TextUtils.TruncateAt.MIDDLE
        else -> TextUtils.TruncateAt.END
    }
}

fun fallback(input: String?, fallback: String): String =
    if (input.isNullOrEmpty()) fallback else input

fun fallback(input: String?, fallback: Int): String =
    if (input.isNullOrEmpty()) Application.instance.getString(fallback) else input

fun startActivityForResult(intent: Intent,
                           requestCode: Int,
                           activity: AppCompatActivity?,
                           fragment: BaseFragment? = null) =
    when {
        fragment != null ->
            fragment.startActivityForResult(intent, requestCode)
        activity != null ->
            activity.startActivityForResult(intent, requestCode)
        else ->
            throw IllegalArgumentException("")
    }