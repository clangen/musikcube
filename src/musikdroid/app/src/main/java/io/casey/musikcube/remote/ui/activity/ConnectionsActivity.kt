package io.casey.musikcube.remote.ui.activity

import android.content.Context
import android.content.Intent
import android.os.Bundle
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
import io.casey.musikcube.remote.websocket.WebSocketService

class ConnectionsActivity : WebSocketActivityBase() {
    private lateinit var recycler: RecyclerView
    private lateinit var adapter: Adapter

    override fun onCreate(savedInstanceState: Bundle?) {
        Application.mainComponent.inject(this)
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_conneections)
        recycler = findViewById(R.id.recycler_view)
        recycler.layoutManager = LinearLayoutManager(this)
        adapter = Adapter()
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
        if (taskName == LoadTask.NAME) {
            adapter.items = (result!! as List<Connection>)
            adapter.notifyDataSetChanged()
        }
    }

    companion object {
        fun getStartIntent(context: Context): Intent {
            return Intent(context, ConnectionsActivity::class.java)
        }
    }
}

private class ViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
    var name: TextView = itemView.findViewById(R.id.name)
    var address: TextView = itemView.findViewById(R.id.hostname)

    fun rebind(connection: Connection) {
        name.text = connection.name
        address.text = connection.hostname
    }
}

private class Adapter : RecyclerView.Adapter<ViewHolder>() {
    var items = listOf<Connection>()

    override fun onBindViewHolder(holder: ViewHolder?, position: Int) {
        holder?.rebind(items[position])
    }

    override fun onCreateViewHolder(parent: ViewGroup?, viewType: Int): ViewHolder {
        return ViewHolder(LayoutInflater.from(parent?.context)
            .inflate(R.layout.connection_row, parent, false))
    }

    override fun getItemCount(): Int {
        return items.size
    }
}

private class LoadTask : Tasks.Blocking<List<Connection>, Exception>() {
    override fun exec(context: Context?): List<Connection> {
        return Application.connectionsDb?.connectionsDao()?.queryConnections()!!
    }

    companion object {
        val NAME = "LoadTask"
    }
}