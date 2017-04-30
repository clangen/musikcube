package io.casey.musikcube.remote.ui.util;

import android.app.SearchManager;
import android.app.SearchableInfo;
import android.content.Context;
import android.support.v4.view.MenuItemCompat;
import android.support.v7.app.ActionBar;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.DividerItemDecoration;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.SearchView;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewPropertyAnimator;
import android.widget.CheckBox;
import android.widget.EditText;

import io.casey.musikcube.remote.R;
import io.casey.musikcube.remote.ui.activity.Filterable;
import io.casey.musikcube.remote.ui.fragment.TransportFragment;
import io.casey.musikcube.remote.util.Strings;

public final class Views {
    public static String EXTRA_TITLE = "extra_title";

    public static void setCheckWithoutEvent(final CheckBox cb,
                                            final boolean checked,
                                            final CheckBox.OnCheckedChangeListener listener) {
        cb.setOnCheckedChangeListener(null);
        cb.setChecked(checked);
        cb.setOnCheckedChangeListener(listener);
    }

    public static void setupDefaultRecyclerView(final Context context,
                                                final RecyclerView recyclerView,
                                                final RecyclerView.Adapter adapter) {
        final LinearLayoutManager layoutManager = new LinearLayoutManager(context);

        recyclerView.setLayoutManager(new LinearLayoutManager(context));
        recyclerView.setAdapter(adapter);

        final DividerItemDecoration dividerItemDecoration =
            new DividerItemDecoration(context, layoutManager.getOrientation());

        recyclerView.addItemDecoration(dividerItemDecoration);
    }

    public static TransportFragment addTransportFragment(final AppCompatActivity activity) {
        return addTransportFragment(activity, null);
    }

    public static TransportFragment addTransportFragment(
        final AppCompatActivity activity, TransportFragment.OnModelChangedListener listener)
    {
        final View root = activity.findViewById(android.R.id.content);
        if (root != null) {
            if (root.findViewById(R.id.transport_container) != null) {
                final TransportFragment fragment = TransportFragment.newInstance();

                activity.getSupportFragmentManager()
                    .beginTransaction()
                    .add(R.id.transport_container, fragment, TransportFragment.TAG)
                    .commit();

                fragment.setModelChangedListener(listener);
                return fragment;
            }
        }
        return null;
    }

    public static void initSearchMenu(final AppCompatActivity activity,
                                      final Menu menu,
                                      final Filterable filterable) {
        activity.getMenuInflater().inflate(R.menu.search_menu, menu);

        final MenuItem searchMenuItem = menu.findItem(R.id.action_search);

        final SearchView searchView =
            (SearchView) MenuItemCompat .getActionView(searchMenuItem);

        searchView.setMaxWidth(Integer.MAX_VALUE);

        if (filterable != null) {
            searchView.setOnQueryTextListener(new SearchView.OnQueryTextListener() {
                @Override
                public boolean onQueryTextSubmit(String query) {
                    return false;
                }

                @Override
                public boolean onQueryTextChange(String newText) {
                    filterable.setFilter(newText);
                    return false;
                }
            });

            searchView.setOnCloseListener(() -> {
                filterable.setFilter("");
                return false;
            });
        }

        final SearchManager searchManager = (SearchManager)
            activity.getSystemService(Context.SEARCH_SERVICE);

        final SearchableInfo searchableInfo = searchManager
            .getSearchableInfo(activity.getComponentName());

        searchView.setSearchableInfo(searchableInfo);
        searchView.setIconifiedByDefault(true);
    }

    public static void setTextAndMoveCursorToEnd(final EditText editText, final String text) {
        editText.setText(text);
        editText.setSelection(editText.getText().length());
    }

    public static ViewPropertyAnimator animateAlpha(final View view, final float value) {
        final ViewPropertyAnimator animator = view.animate().alpha(value).setDuration(300);
        animator.start();
        return animator;
    }

    public static void enableUpNavigation(final AppCompatActivity activity) {
        final ActionBar ab = activity.getSupportActionBar();
        if (ab != null) {
            ab.setDisplayHomeAsUpEnabled(true);
        }
    }

    public static void setTitle(final AppCompatActivity activity, int defaultId) {
        final String title = activity.getIntent().getStringExtra(EXTRA_TITLE);
        if (Strings.notEmpty(title)) {
            activity.setTitle(title);
        }
        else {
            activity.setTitle(defaultId);
        }
    }

    private Views() {

    }
}
