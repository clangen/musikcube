package io.casey.musikcube.remote.ui.albums.activity

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.view.Menu
import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.service.websocket.model.ICategoryValue
import io.casey.musikcube.remote.ui.albums.constant.Album
import io.casey.musikcube.remote.ui.albums.fragment.AlbumBrowseFragment
import io.casey.musikcube.remote.ui.shared.activity.FragmentActivityWithTransport
import io.casey.musikcube.remote.ui.shared.constant.Shared
import io.casey.musikcube.remote.ui.shared.fragment.BaseFragment

class AlbumBrowseActivity: FragmentActivityWithTransport() {
    private val albums
        get() = content as AlbumBrowseFragment

    override fun onCreateOptionsMenu(menu: Menu): Boolean = albums.createOptionsMenu(menu)
    override val contentFragmentTag: String = AlbumBrowseFragment.TAG

    override fun createContentFragment(): BaseFragment =
        (intent.extras ?: Bundle()).run {
            AlbumBrowseFragment.create(
                applicationContext,
                getString(Album.Extra.CATEGORY_NAME, ""),
                getLong(Album.Extra.CATEGORY_ID, -1))
        }

    companion object {
        fun getStartIntent(context: Context): Intent =
            Intent(context, AlbumBrowseActivity::class.java)

        fun getStartIntent(context: Context, categoryName: String, categoryId: Long): Intent {
            return Intent(context, AlbumBrowseActivity::class.java)
                .putExtra(Album.Extra.CATEGORY_NAME, categoryName)
                .putExtra(Album.Extra.CATEGORY_ID, categoryId)
        }

        fun getStartIntent(context: Context, categoryName: String, categoryId: Long, categoryValue: String): Intent =
            getStartIntent(context, categoryName, categoryId).apply {
                if (categoryValue.isNotEmpty()) {
                    putExtra(Shared.Extra.TITLE_OVERRIDE, context.getString(R.string.albums_by_title, categoryValue))
                }
            }

        fun getStartIntent(context: Context, categoryName: String, categoryValue: ICategoryValue): Intent =
            getStartIntent(context, categoryName, categoryValue.id, categoryValue.value)
    }
}
