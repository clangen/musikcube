package io.casey.musikcube.remote.ui.tracks.activity

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.support.v7.widget.RecyclerView
import android.support.v7.widget.helper.ItemTouchHelper
import android.view.Menu
import android.view.MenuItem
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.framework.ViewModel
import io.casey.musikcube.remote.ui.shared.activity.BaseActivity
import io.casey.musikcube.remote.ui.shared.extension.setupDefaultRecyclerView
import io.casey.musikcube.remote.ui.shared.mixin.DataProviderMixin
import io.casey.musikcube.remote.ui.shared.mixin.ViewModelMixin
import io.casey.musikcube.remote.ui.tracks.adapter.EditPlaylistAdapter
import io.casey.musikcube.remote.ui.tracks.model.EditPlaylistViewModel
import io.casey.musikcube.remote.ui.tracks.model.EditPlaylistViewModel.Status
import io.reactivex.rxkotlin.subscribeBy

class EditPlaylistActivity: BaseActivity() {
    private lateinit var viewModel: EditPlaylistViewModel
    private lateinit var data: DataProviderMixin
    private lateinit var adapter: EditPlaylistAdapter

    override fun onCreate(savedInstanceState: Bundle?) {
        mixin(ViewModelMixin(this))
        data = mixin(DataProviderMixin())
        super.onCreate(savedInstanceState)
        setContentView(R.layout.recycler_view_activity)
        viewModel = getViewModel()!!
        viewModel.attach(data.provider)
        val recycler = findViewById<RecyclerView>(R.id.recycler_view)
        val touchHelper = ItemTouchHelper(touchHelperCallback)
        touchHelper.attachToRecyclerView(recycler)
        adapter = EditPlaylistAdapter(viewModel, touchHelper)
        setupDefaultRecyclerView(recycler, adapter)
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        menuInflater.inflate(R.menu.edit_playlist_menu, menu)
        return true
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        if (item.itemId == R.id.action_save) {
            viewModel.save().subscribeBy(
                onNext = { playlistId ->
                    if (playlistId != -1L) {
                        finish()
                    }
                    else {
                        /* TODO ERROR SNACKBAR */
                    }
                },
                onError = {
                    /* TODO ERROR SNACKBAR */
                })
        }
        return super.onOptionsItemSelected(item)
    }

    override fun onResume() {
        super.onResume()

        disposables.add(viewModel.observe().subscribeBy(
            onNext = { status ->
                if (status == Status.Updated) {
                    adapter.notifyDataSetChanged()
                }
            },
            onError = { }
        ))
    }

    override fun <T: ViewModel<*>> createViewModel(): T? {
        return EditPlaylistViewModel(intent.extras.getLong(EXTRA_PLAYLIST_ID, -1L)) as T
    }

    private val touchHelperCallback = object:ItemTouchHelper.SimpleCallback(
            ItemTouchHelper.UP or ItemTouchHelper.DOWN,
            ItemTouchHelper.LEFT)
    {
        override fun onMove(recyclerView: RecyclerView?, viewHolder: RecyclerView.ViewHolder, target: RecyclerView.ViewHolder): Boolean {
            val from = viewHolder.adapterPosition
            val to = target.adapterPosition
            viewModel.move(from, to)
            adapter.notifyItemMoved(from, to)
            return true
        }

        override fun onSwiped(viewHolder: RecyclerView.ViewHolder, direction: Int) {
            viewModel.remove(viewHolder.adapterPosition)
            adapter.notifyItemRemoved(viewHolder.adapterPosition)
        }
    }

    companion object {
        private val EXTRA_PLAYLIST_ID = "extra_playlist_id"

        fun getStartIntent(context: Context, playlistId: Long): Intent {
            return Intent(context, EditPlaylistActivity::class.java)
                .putExtra(EXTRA_PLAYLIST_ID, playlistId)
        }
    }
}