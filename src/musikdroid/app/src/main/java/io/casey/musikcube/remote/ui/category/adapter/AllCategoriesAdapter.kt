package io.casey.musikcube.remote.ui.category.adapter

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.ui.category.constant.Category.toDisplayString

class AllCategoriesAdapter(private val listener: EventListener)
    : RecyclerView.Adapter<AllCategoriesAdapter.Holder>()
{
    private var categories: List<String> = listOf()

    interface EventListener {
        fun onItemClicked(category: String)
    }

    class Holder(itemView: View): RecyclerView.ViewHolder(itemView) {
        private val title: TextView = itemView.findViewById(R.id.title)

        init {
            itemView.findViewById<View>(R.id.action).visibility = View.GONE
            itemView.findViewById<View>(R.id.subtitle).visibility = View.GONE
        }

        internal fun bindView(category: String) {
            title.text = toDisplayString(itemView.context, category)
            itemView.tag = category
        }
    }

    override fun onBindViewHolder(holder: Holder, position: Int) {
        holder.bindView(categories[position])
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): Holder {
        val inflater = LayoutInflater.from(parent.context)
        val view = inflater.inflate(R.layout.simple_list_item, parent, false)
        view.setOnClickListener { v -> listener.onItemClicked(v.tag as String) }
        return Holder(view)
    }

    override fun getItemCount(): Int {
        return categories.size
    }

    fun setModel(model: List<String>) {
        this.categories = model.filter { !BLACKLIST.contains(it) }
        this.notifyDataSetChanged()
    }

    companion object {
        val BLACKLIST = setOf("channels", "encoder", "path_id", "totaltracks")
    }
}