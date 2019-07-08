package io.casey.musikcube.remote.ui.tracks.activity

import android.app.Dialog
import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.view.Menu
import android.view.MenuItem
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.recyclerview.widget.ItemTouchHelper
import androidx.recyclerview.widget.RecyclerView
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.framework.ViewModel
import io.casey.musikcube.remote.ui.navigation.Transition
import io.casey.musikcube.remote.ui.shared.activity.BaseActivity
import io.casey.musikcube.remote.ui.shared.extension.setupDefaultRecyclerView
import io.casey.musikcube.remote.ui.shared.extension.showErrorSnackbar
import io.casey.musikcube.remote.ui.shared.fragment.BaseDialogFragment
import io.casey.musikcube.remote.ui.shared.mixin.MetadataProxyMixin
import io.casey.musikcube.remote.ui.shared.mixin.ViewModelMixin
import io.casey.musikcube.remote.ui.tracks.adapter.EditPlaylistAdapter
import io.casey.musikcube.remote.ui.tracks.model.EditPlaylistViewModel
import io.casey.musikcube.remote.ui.tracks.model.EditPlaylistViewModel.Status
import io.reactivex.rxkotlin.subscribeBy

class EditPlaylistActivity: BaseActivity() {
    private lateinit var viewModel: EditPlaylistViewModel
    private lateinit var data: MetadataProxyMixin
    private lateinit var adapter: EditPlaylistAdapter
    private var playlistName = ""

    override fun onCreate(savedInstanceState: Bundle?) {
        mixin(ViewModelMixin(this))
        data = mixin(MetadataProxyMixin())
        super.onCreate(savedInstanceState)
        playlistName = extras.getString(EXTRA_PLAYLIST_NAME, "-")
        title = getString(R.string.playlist_edit_activity, playlistName)
        setContentView(R.layout.edit_playlist_activity)
        viewModel = getViewModel()!!
        viewModel.attach(data.provider)
        val recycler = findViewById<RecyclerView>(R.id.recycler_view)
        val touchHelper = ItemTouchHelper(touchHelperCallback)
        touchHelper.attachToRecyclerView(recycler)
        adapter = EditPlaylistAdapter(viewModel, touchHelper, prefs)
        setupDefaultRecyclerView(recycler, adapter)
        setResult(RESULT_CANCELED)
    }

    override fun onBackPressed() {
        if (viewModel.modified) {
            ConfirmDiscardChangesDialog.show(this)
            return
        }
        super.onBackPressed()
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        menuInflater.inflate(R.menu.edit_playlist_menu, menu)
        return true
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        if (item.itemId == R.id.action_save) {
            saveAndFinish()
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
        @Suppress("unchecked_cast")
        return EditPlaylistViewModel(extras.getLong(EXTRA_PLAYLIST_ID, -1L)) as T
    }

    override val transitionType: Transition
        get() = Transition.Vertical

    private fun saveAndFinish() {
        if (viewModel.modified) {
            disposables.add(viewModel.save().subscribeBy(
                onNext = { playlistId ->
                    if (playlistId != -1L) {
                        val data = Intent()
                        data.putExtra(EXTRA_PLAYLIST_NAME, playlistName)
                        data.putExtra(EXTRA_PLAYLIST_ID, playlistId)
                        setResult(RESULT_OK, data)
                        finish()
                    } else {
                        showErrorSnackbar(R.string.playlist_edit_save_failed)
                    }
                },
                onError = {
                    showErrorSnackbar(R.string.playlist_edit_save_failed)
                }))
        }
        else {
            finish()
        }
    }

    private val touchHelperCallback = object:ItemTouchHelper.SimpleCallback(
            ItemTouchHelper.UP or ItemTouchHelper.DOWN,
            ItemTouchHelper.LEFT)
    {
        override fun onMove(recyclerView: RecyclerView, viewHolder: RecyclerView.ViewHolder, target: RecyclerView.ViewHolder): Boolean {
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

    class ConfirmDiscardChangesDialog : BaseDialogFragment() {
        override fun onCreateDialog(savedInstanceState: Bundle?): Dialog =
            (activity as EditPlaylistActivity).run {
                return AlertDialog.Builder(this)
                    .setTitle(R.string.playlist_edit_save_changes_title)
                    .setMessage(R.string.playlist_edit_save_changes_message)
                    .setNegativeButton(R.string.button_discard) { _, _ -> this.finish() }
                    .setPositiveButton(R.string.button_save) { _, _ -> this.saveAndFinish() }
                    .create()
            }

        companion object {
            const val TAG = "confirm_discard_playlist_changes"

            fun show(activity: AppCompatActivity) {
                dismiss(activity, TAG)
                val result = ConfirmDiscardChangesDialog()
                result.show(activity.supportFragmentManager, TAG)
            }
        }
    }

    companion object {
        const val EXTRA_PLAYLIST_ID = "extra_playlist_id"
        const val EXTRA_PLAYLIST_NAME = "extra_playlist_name"

        fun getStartIntent(context: Context, playlistName: String, playlistId: Long): Intent {
            return Intent(context, EditPlaylistActivity::class.java)
                .putExtra(EXTRA_PLAYLIST_ID, playlistId)
                .putExtra(EXTRA_PLAYLIST_NAME, playlistName)
        }
    }
}