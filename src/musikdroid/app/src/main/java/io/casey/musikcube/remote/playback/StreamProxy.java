package io.casey.musikcube.remote.playback;

import android.content.Context;
import android.content.SharedPreferences;
import android.net.Uri;
import android.util.Base64;

import com.danikula.videocache.CacheListener;
import com.danikula.videocache.HttpProxyCacheServer;
import com.danikula.videocache.file.FileNameGenerator;
import com.danikula.videocache.file.Md5FileNameGenerator;

import java.io.File;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import io.casey.musikcube.remote.util.NetworkUtil;
import io.casey.musikcube.remote.websocket.Prefs;

public class StreamProxy {
    private static final long BYTES_PER_MEGABYTE = 1048576L;
    private static final long BYTES_PER_GIGABYTE = 1073741824L;
    private static final Map<Integer, Long> CACHE_SETTING_TO_BYTES;
    private static final FileNameGenerator DEFAULT_FILENAME_GENERATOR = new Md5FileNameGenerator();

    static {
        CACHE_SETTING_TO_BYTES = new HashMap<>();
        CACHE_SETTING_TO_BYTES.put(0, BYTES_PER_MEGABYTE * 32);
        CACHE_SETTING_TO_BYTES.put(1, BYTES_PER_GIGABYTE / 2);
        CACHE_SETTING_TO_BYTES.put(2, BYTES_PER_GIGABYTE);
        CACHE_SETTING_TO_BYTES.put(3, BYTES_PER_GIGABYTE * 2);
        CACHE_SETTING_TO_BYTES.put(4, BYTES_PER_GIGABYTE * 3);
        CACHE_SETTING_TO_BYTES.put(5, BYTES_PER_GIGABYTE * 4);
    }

    private static StreamProxy INSTANCE;

    private HttpProxyCacheServer proxy;
    private SharedPreferences prefs;

    private StreamProxy(final Context context) {
        prefs = context.getSharedPreferences(Prefs.NAME, Context.MODE_PRIVATE);

        if (this.prefs.getBoolean(Prefs.Key.CERT_VALIDATION_DISABLED, Prefs.Default.CERT_VALIDATION_DISABLED)) {
            NetworkUtil.disableCertificateValidation();
        }
        else {
            NetworkUtil.enableCertificateValidation();
        }

        int diskCacheIndex = this.prefs.getInt(
            Prefs.Key.DISK_CACHE_SIZE_INDEX, Prefs.Default.DISK_CACHE_SIZE_INDEX);

        if (diskCacheIndex < 0 || diskCacheIndex > CACHE_SETTING_TO_BYTES.size()) {
            diskCacheIndex = 0;
        }

        final File cachePath = new File(context.getExternalCacheDir(), "audio");

        proxy = new HttpProxyCacheServer.Builder(context.getApplicationContext())
            .cacheDirectory(cachePath)
            .maxCacheSize(CACHE_SETTING_TO_BYTES.get(diskCacheIndex))
            .headerInjector((url) -> {
                Map<String, String> headers = new HashMap<>();
                final String userPass = "default:" + prefs.getString(Prefs.Key.PASSWORD, Prefs.Default.PASSWORD);
                final String encoded = Base64.encodeToString(userPass.getBytes(), Base64.NO_WRAP);
                headers.put("Authorization", "Basic " + encoded);
                return headers;
            })
            .fileNameGenerator((url) -> {
                try {
                    final Uri uri = Uri.parse(url);
                    /* format is: audio/external_id/<id> */
                    final List<String> segments = uri.getPathSegments();
                    if (segments.size() == 3 && "external_id".equals(segments.get(1))) {
                        return segments.get(2); /* id, should be globally unique. */
                    }
                }
                catch (Exception ex) {
                    /* eh... */
                }
                return DEFAULT_FILENAME_GENERATOR.generate(url);
            })
            .build();
    }

    public static synchronized void init(final Context context) {
        if (INSTANCE == null) {
            INSTANCE = new StreamProxy(context.getApplicationContext());
        }
    }

    public static synchronized void registerCacheListener(final CacheListener cl, final String uri) {
        if (INSTANCE != null && cl != null) {
            INSTANCE.proxy.registerCacheListener(cl, uri); /* let it throw */
        }
    }

    public static synchronized void unregisterCacheListener(final CacheListener cl) {
        if (INSTANCE != null && cl != null) {
            INSTANCE.proxy.unregisterCacheListener(cl);
        }
    }

    public static synchronized String getProxyUrl(final Context context, final String url) {
        init(context);
        return INSTANCE.proxy.getProxyUrl(url);
    }

    public static synchronized void reload() {
        if (INSTANCE != null) {
            INSTANCE.proxy.shutdown();
            INSTANCE = null;
        }
    }

}
