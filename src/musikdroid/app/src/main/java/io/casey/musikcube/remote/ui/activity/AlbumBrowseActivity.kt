package io.casey.musikcube.remote.ui.activity

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.support.v7.widget.RecyclerView
import android.view.LayoutInflater
import android.view.Menu
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import com.pluscubed.recyclerfastscroll.RecyclerFastScroller
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.data.IAlbum
import io.casey.musikcube.remote.data.ICategoryValue
import io.casey.musikcube.remote.data.IDataProvider
import io.casey.musikcube.remote.playback.Metadata
import io.casey.musikcube.remote.ui.extension.*
import io.casey.musikcube.remote.ui.fragment.TransportFragment
import io.casey.musikcube.remote.ui.view.EmptyListView
import io.casey.musikcube.remote.util.Debouncer
import io.casey.musikcube.remote.util.Navigation
import io.casey.musikcube.remote.util.Strings
import io.casey.musikcube.remote.websocket.Messages

class AlbumBrowseActivity : WebSocketActivityBase(), Filterable {
    private var adapter: Adapter = Adapter()
    private var categoryName: String = ""
    private var categoryId: Long = 0
    private var lastFilter = ""
    private lateinit var transport: TransportFragment
    private lateinit var emptyView: EmptyListView

    override fun onCreate(savedInstanceState: Bundle?) {
        component.inject(this)

        super.onCreate(savedInstanceState)

        categoryName = intent.getStringExtra(EXTRA_CATEGORY_NAME) ?: ""
        categoryId = intent.getLongExtra(EXTRA_CATEGORY_ID, categoryId)

        setContentView(R.layout.recycler_view_activity)

        setTitleFromIntent(R.string.albums_title)
        enableUpNavigation()

        val fastScroller = findViewById<RecyclerFastScroller>(R.id.fast_scroller)
        val recyclerView = findViewById<RecyclerView>(R.id.recycler_view)
        setupDefaultRecyclerView(recyclerView, fastScroller, adapter)

        emptyView = findViewById(R.id.empty_list_view)
        emptyView.capability = EmptyListView.Capability.OnlineOnly
        emptyView.emptyMessage = getString(R.string.empty_no_items_format, getString(R.string.browse_type_albums))
        emptyView.alternateView = recyclerView

        transport = addTransportFragment(object: TransportFragment.OnModelChangedListener {
            override fun onChanged(fragment: TransportFragment) {
                adapter.notifyDataSetChanged()
            }
        })!!
    }

    override fun onResume() {
        super.onResume()
        initObservables()
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        initSearchMenu(menu, this)
        return true
    }

    override fun setFilter(filter: String) {
        lastFilter = filter
        filterDebouncer.call(filter)
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        if (resultCode == Navigation.ResponseCode.PLAYBACK_STARTED) {
            setResult(Navigation.ResponseCode.PLAYBACK_STARTED)
            finish()
        }

        super.onActivityResult(requestCode, resultCode, data)
    }

    private fun initObservables() {
        disposables.add(dataProvider.observeState().subscribe(
            { state ->
                if (state.first == IDataProvider.State.Connected) {
                    filterDebouncer.call()
                    requery()
                }
                else {
                    emptyView.update(state.first, adapter.itemCount)
                }
            }, { /* error */ }))
    }

    private fun requery() {
        dataProvider.getAlbumForCategory(categoryName, categoryId)
            .subscribe({ albumList ->
                adapter.setModel(albumList)
                emptyView.update(dataProvider.state, adapter.itemCount)
            }, { /* error*/ })
    }

    private val filterDebouncer = object : Debouncer<String>(350) {
        override fun onDebounced(last: String?) {
            if (!isPaused()) {
                requery()
            }
        }
    }

    private val onItemClickListener = { view: View ->
        val album = view.tag as IAlbum

        val intent = TrackListActivity.getStartIntent(
            this@AlbumBrowseActivity, Messages.Category.ALBUM, album.id, album.value)

        startActivityForResult(intent, Navigation.RequestCode.ALBUM_TRACKS_ACTIVITY)
    }

    private inner class ViewHolder internal constructor(itemView: View) : RecyclerView.ViewHolder(itemView) {
        private val title = itemView.findViewById<TextView>(R.id.title)
        private val subtitle = itemView.findViewById<TextView>(R.id.subtitle)

        internal fun bind(album: IAlbum) {
            val playing = transport.playbackService!!.playingTrack
            val playingId = playing.albumId

            var titleColor = R.color.theme_foreground
            var subtitleColor = R.color.theme_disabled_foreground

            if (playingId != -1L && album.id == playingId) {
                titleColor = R.color.theme_green
                subtitleColor = R.color.theme_yellow
            }

            title.text = fallback(album.value, "-")
            title.setTextColor(getColorCompat(titleColor))

            subtitle.text = fallback(album.albumArtist, "-")
            subtitle.setTextColor(getColorCompat(subtitleColor))
            itemView.tag = album
        }
    }

    private inner class Adapter : RecyclerView.Adapter<ViewHolder>() {
        private var model: List<IAlbum> = listOf()

        internal fun setModel(model: List<IAlbum>) {
            this.model = model
            notifyDataSetChanged()
        }

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
            val inflater = LayoutInflater.from(parent.context)
            val view = inflater.inflate(R.layout.simple_list_item, parent, false)
            view.setOnClickListener(onItemClickListener)
            return ViewHolder(view)
        }

        override fun onBindViewHolder(holder: ViewHolder, position: Int) {
            holder.bind(model[position])
        }

        override fun getItemCount(): Int {
            return model.size
        }
    }

    companion object {
        private val EXTRA_CATEGORY_NAME = "extra_category_name"
        private val EXTRA_CATEGORY_ID = "extra_category_id"

        fun getStartIntent(context: Context): Intent {
            return Intent(context, AlbumBrowseActivity::class.java)
        }

        fun getStartIntent(context: Context, categoryName: String, categoryId: Long): Intent {
            return Intent(context, AlbumBrowseActivity::class.java)
                .putExtra(EXTRA_CATEGORY_NAME, categoryName)
                .putExtra(EXTRA_CATEGORY_ID, categoryId)
        }

        fun getStartIntent(context: Context, categoryName: String, categoryId: Long, categoryValue: String): Intent {
            val intent = getStartIntent(context, categoryName, categoryId)

            if (Strings.notEmpty(categoryValue)) {
                intent.putExtra(
                    EXTRA_ACTIVITY_TITLE,
                    context.getString(R.string.albums_by_title, categoryValue))
            }

            return intent
        }

        fun getStartIntent(context: Context, categoryName: String, categoryValue: ICategoryValue): Intent {
            return getStartIntent(context, categoryName, categoryValue.id, categoryValue.value)
        }
    }
}
