package io.casey.musikcube.remote.ui.activity

import android.app.Activity
import android.app.Dialog
import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.support.v4.app.DialogFragment
import android.support.v7.app.AlertDialog
import android.support.v7.widget.DividerItemDecoration
import android.support.v7.widget.LinearLayoutManager
import android.support.v7.widget.RecyclerView
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import com.uacf.taskrunner.Task
import com.uacf.taskrunner.Tasks
import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.db.connections.Connection
import io.casey.musikcube.remote.ui.extension.dialogVisible
import io.casey.musikcube.remote.ui.extension.enableUpNavigation
import io.casey.musikcube.remote.ui.extension.showDialog
import io.casey.musikcube.remote.websocket.WebSocketService

class ConnectionsActivity : WebSocketActivityBase() {
    private lateinit var recycler: RecyclerView
    private lateinit var emptyText: View
    private lateinit var adapter: Adapter

    override fun onCreate(savedInstanceState: Bundle?) {
        Application.mainComponent.inject(this)

        super.onCreate(savedInstanceState)
        setResult(Activity.RESULT_CANCELED)
        setContentView(R.layout.activity_connections)

        enableUpNavigation()

        recycler = findViewById(R.id.recycler_view)
        emptyText = findViewById(R.id.empty_text)

        adapter = Adapter(itemClickListener)
        val layoutManager = LinearLayoutManager(this)
        recycler.layoutManager = layoutManager
        recycler.addItemDecoration(DividerItemDecoration(this, layoutManager.orientation))
        recycler.adapter = adapter
    }

    override fun onResume() {
        super.onResume()
        runner.run(LoadTask.NAME, LoadTask())
    }

    override val webSocketServiceClient: WebSocketService.Client?
        get() = null

    override val playbackServiceEventListener: (() -> Unit)?
        get() = null

    override fun onTaskCompleted(taskName: String, taskId: Long, task: Task<*, *>, result: Any) {
        if (taskName == LoadTask.NAME || taskName == DeleteTask.NAME) {
            adapter.items = (result!! as List<Connection>)
            adapter.notifyDataSetChanged()
            updateViewState()
        }
    }

    fun updateViewState() {
        val count = adapter.itemCount
        recycler.visibility = if (count == 0) View.GONE else View.VISIBLE
        emptyText.visibility = if (count == 0) View.VISIBLE else View.GONE
    }

    fun onConfirmDelete(connection: Connection) {
        runner.run(DeleteTask.NAME, DeleteTask(connection))
    }

    private val itemClickListener: (View) -> Unit = { view: View ->
        val connection = view.tag as Connection
        if (view.id == R.id.button_del) {
            if (!dialogVisible(ConfirmDeleteDialog.EXTRA_CONNECTION)) {
                showDialog(ConfirmDeleteDialog.newInstance(connection), ConfirmDeleteDialog.TAG)
            }
        }
        else {
            val intent = Intent().putExtra(EXTRA_SELECTED_CONNECTION, connection)
            setResult(RESULT_OK, intent)
            finish()
        }
    }

    companion object {
        val EXTRA_SELECTED_CONNECTION = "extra_selected_connection"
        fun getStartIntent(context: Context): Intent {
            return Intent(context, ConnectionsActivity::class.java)
        }
    }
}

private class ViewHolder(itemView: View, listener: (View) -> Unit) : RecyclerView.ViewHolder(itemView) {
    var name: TextView = itemView.findViewById(R.id.name)
    var address: TextView = itemView.findViewById(R.id.hostname)
    var delete: TextView = itemView.findViewById(R.id.button_del)

    init {
        itemView.setOnClickListener(listener)
        delete.setOnClickListener(listener)
    }

    fun rebind(connection: Connection) {
        itemView.tag = connection
        delete.tag = connection
        name.text = connection.name
        address.text = connection.hostname
    }
}

private class Adapter(val listener: (View) -> Unit) : RecyclerView.Adapter<ViewHolder>() {
    var items = listOf<Connection>()

    override fun onBindViewHolder(holder: ViewHolder?, position: Int) {
        holder?.rebind(items[position])
    }

    override fun onCreateViewHolder(parent: ViewGroup?, viewType: Int): ViewHolder {
        val view = LayoutInflater.from(parent?.context)
            .inflate(R.layout.connection_row, parent, false)

        return ViewHolder(view, listener)
    }

    override fun getItemCount(): Int {
        return items.size
    }
}

private class LoadTask : Tasks.Blocking<List<Connection>, Exception>() {
    override fun exec(context: Context?): List<Connection> {
        return Application.connectionsDb?.connectionsDao()?.query()!!
    }

    companion object {
        val NAME = "LoadTask"
    }
}

private class DeleteTask(val connection: Connection) : Tasks.Blocking<List<Connection>, Exception>() {
    override fun exec(context: Context?): List<Connection> {
        val dao = Application.connectionsDb?.connectionsDao()!!
        dao.delete(connection.name)
        return dao.query()
    }

    companion object {
        val NAME = "DeleteTask"
    }
}

class ConfirmDeleteDialog : DialogFragment() {
    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val connection = arguments.getParcelable<Connection>(EXTRA_CONNECTION)
        val message = getString(R.string.settings_confirm_delete_message, connection.name)

        val dlg = AlertDialog.Builder(activity)
            .setTitle(R.string.settings_confirm_delete_title)
            .setMessage(message)
            .setNegativeButton(R.string.button_no, null)
            .setPositiveButton(R.string.button_yes) { _, _ ->
                (activity as ConnectionsActivity).onConfirmDelete(connection)
            }
            .create()

        dlg.setCancelable(false)
        return dlg
    }

    companion object {
        val TAG = "confirm_delete_dialog"
        val EXTRA_CONNECTION = "extra_connection"
        fun newInstance(connection: Connection): ConfirmDeleteDialog {
            val result = ConfirmDeleteDialog()
            result.arguments = Bundle()
            result.arguments.putParcelable(EXTRA_CONNECTION, connection)
            return result
        }
    }
}