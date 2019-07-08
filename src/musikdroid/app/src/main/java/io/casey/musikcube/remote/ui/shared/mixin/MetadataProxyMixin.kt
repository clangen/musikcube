package io.casey.musikcube.remote.ui.shared.mixin

import android.os.Bundle
import io.casey.musikcube.remote.Application
import io.casey.musikcube.remote.framework.MixinBase
import io.casey.musikcube.remote.injection.DaggerViewComponent
import io.casey.musikcube.remote.service.websocket.WebSocketService
import io.casey.musikcube.remote.service.websocket.model.IMetadataProxy
import javax.inject.Inject

class MetadataProxyMixin : MixinBase() {
    @Inject lateinit var wss: WebSocketService
    @Inject lateinit var provider: IMetadataProxy

    override fun onCreate(bundle: Bundle) {
        super.onCreate(bundle)

        DaggerViewComponent.builder()
            .appComponent(Application.appComponent)
            .build().inject(this)
    }

    override fun onResume() {
        super.onResume()
        provider.attach()
    }

    override fun onPause() {
        super.onPause()
        provider.detach()
    }

    override fun onDestroy() {
        super.onDestroy()
        provider.destroy()
    }
}