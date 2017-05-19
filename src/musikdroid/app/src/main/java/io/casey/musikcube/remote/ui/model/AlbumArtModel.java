package io.casey.musikcube.remote.ui.model;

import android.util.LruCache;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.IOException;
import java.net.SocketTimeoutException;
import java.net.URLEncoder;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.regex.Pattern;

import io.casey.musikcube.remote.util.Strings;
import okhttp3.Call;
import okhttp3.Callback;
import okhttp3.Interceptor;
import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.Response;

public final class AlbumArtModel {
    /* http://www.last.fm/group/Last.fm+Web+Services/forum/21604/_/522900 -- it's ok to
    put our key in the code */
    private static final String LAST_FM_ALBUM_INFO =
        "http://ws.audioscrobbler.com/2.0/?method=album.getinfo&api_key=" +
        "502c69bd3f9946e8e0beee4fcb28c4cd&artist=%s&album=%s&format=json";

    private static OkHttpClient OK_HTTP;

    private static LruCache<Integer, String> URL_CACHE = new LruCache<>(500);

    static {
        OK_HTTP = new OkHttpClient.Builder()
            .addInterceptor((Interceptor.Chain chain) -> {
                Request request = chain.request();

                int count = 0;
                while (count++ < 3) {
                    try {
                        Response response = chain.proceed(request);
                        if (response.isSuccessful()) {
                            return response;
                        }
                    }
                    catch (SocketTimeoutException ex) {
                        /* om nom nom */
                    }
                }

                throw new IOException("retries exhausted");
            })
            .connectTimeout(3, TimeUnit.SECONDS)
            .build();
    }

    private String track, artist, album, url;
    private AlbumArtCallback callback;
    private boolean fetching;
    private boolean noart;
    private int id;
    private Size desiredSize;

    public enum Size {
        Small("small", 0),
        Medium("medium", 1),
        Large("large", 2),
        ExtraLarge("extralarge", 3),
        Mega("mega", 4);

        final String name;
        final int order;

        static Size from(final String value) {
            for (Size size : values()) {
                if (size.name.equals(value)) {
                    return size;
                }
            }
            return null;
        }

        Size(String name, int order) {
            this.name = name;
            this.order = order;
        }
    }

    public static class Image {
        final String url;
        final Size size;

        public Image(final Size size, final String url) {
            this.url = url;
            this.size = size;
        }
    }

    public static AlbumArtModel empty() {
        return new AlbumArtModel("", "", "", Size.Small, null);
    }

    public AlbumArtModel(String track, String artist, String album, Size size, AlbumArtCallback callback) {
        this.track = track;
        this.artist = artist;
        this.album = album;
        this.callback = callback != null ? callback : (info, url) -> { };
        this.desiredSize = size;
        this.id = (artist + album + size.name).hashCode();

        synchronized (this) {
            this.url = URL_CACHE.get(id);
        }
    }

    public void destroy() {
        this.callback = (info, url) -> { };
    }

    public boolean is(String artist, String album) {
        return (this.artist != null && this.artist.equalsIgnoreCase(artist) &&
            this.album != null && this.album.equalsIgnoreCase(album));
    }

    public String getTrack() {
        return track;
    }

    public interface AlbumArtCallback {
        void onFinished(final AlbumArtModel info, final String url);
    }

    public synchronized String getUrl() {
        return this.url;
    }

    public int getId() {
        return id;
    }

    public synchronized AlbumArtModel fetch() {
        if (this.fetching || this.noart) {
            return this;
        }

        if (!Strings.empty(this.url)) {
            callback.onFinished(this, this.url);
        }
        else if (Strings.notEmpty(this.artist) && Strings.notEmpty(this.album)) {
            String requestUrl;

            try {
                final String sanitizedAlbum = removeKnownJunkFromMetadata(album);
                final String sanitizedArtist = removeKnownJunkFromMetadata(artist);

                requestUrl = String.format(
                    LAST_FM_ALBUM_INFO,
                    URLEncoder.encode(sanitizedArtist, "UTF-8"),
                    URLEncoder.encode(sanitizedAlbum, "UTF-8"));
            }
            catch (Exception ex) {
                throw new RuntimeException(ex);
            }

            this.fetching = true;
            final Request request = new Request.Builder().url(requestUrl).build();

            OK_HTTP.newCall(request).enqueue(new Callback() {
                @Override
                public void onFailure(Call call, IOException e) {
                    fetching = false;
                    callback.onFinished(AlbumArtModel.this, null);
                }

                @Override
                public void onResponse(Call call, Response response) throws IOException {
                    synchronized (AlbumArtModel.this) {
                        List<Image> imageList = new ArrayList<>();

                        try {
                            final JSONObject json = new JSONObject(response.body().string());
                            final JSONArray imagesJson = json.getJSONObject("album").getJSONArray("image");
                            for (int i = 0; i < imagesJson.length(); i++) {
                                final JSONObject imageJson = imagesJson.getJSONObject(i);
                                final Size size = Size.from(imageJson.optString("size", ""));
                                if (size != null) {
                                    final String resolvedUrl = imageJson.optString("#text", "");
                                    if (Strings.notEmpty(resolvedUrl)) {
                                        imageList.add(new Image(size, resolvedUrl));
                                    }
                                }
                            }

                            if (imageList.size() > 0) {
                                /* find the image with the closest to the requested size.
                                exact match preferred. */
                                Size desiredSize = Size.Mega;
                                Image closest = imageList.get(0);
                                int lastDelta = Integer.MAX_VALUE;
                                for (final Image check : imageList) {
                                    if (check.size == desiredSize) {
                                        closest = check;
                                        break;
                                    }
                                    else {
                                        int delta = Math.abs(desiredSize.order - check.size.order);
                                        if (lastDelta > delta) {
                                            closest = check;
                                            lastDelta = delta;
                                        }
                                    }
                                }

                                synchronized (AlbumArtModel.this) {
                                    URL_CACHE.put(id, closest.url);
                                }

                                fetching = false;
                                AlbumArtModel.this.url = closest.url;
                                callback.onFinished(AlbumArtModel.this, closest.url);
                                return;
                            }
                        }
                        catch (JSONException ex) {
                        }

                        noart = true; /* got a response, but it was invalid. we won't try again */
                        fetching = false;
                    }

                    callback.onFinished(AlbumArtModel.this, null);
                }
            });
        }
        else {
            callback.onFinished(this, null);
        }

        return this;
    }

    private static final Pattern[] BAD_PATTERNS = {
        Pattern.compile("(?i)^" + Pattern.quote("[") + "CD" + Pattern.quote("]")),
        Pattern.compile("(?i)" + Pattern.quote("(") + "disc \\d*" + Pattern.quote(")") + "$"),
        Pattern.compile("(?i)" + Pattern.quote("[") + "disc \\d*" + Pattern.quote("]") + "$"),
        Pattern.compile("(?i)" + Pattern.quote("(+") + "video" + Pattern.quote(")") + "$"),
        Pattern.compile("(?i)" + Pattern.quote("[+") + "video" + Pattern.quote("]") + "$"),
        Pattern.compile("(?i)" + Pattern.quote("(") + "explicit" + Pattern.quote(")") + "$"),
        Pattern.compile("(?i)" + Pattern.quote("[") + "explicit" + Pattern.quote("]") + "$"),
        Pattern.compile("(?i)" + Pattern.quote("[+") + "digital booklet" + Pattern.quote("]") + "$")
    };

    private static String removeKnownJunkFromMetadata(final String album) {
        String result = album;
        for (Pattern pattern : BAD_PATTERNS) {
            result = pattern.matcher(result).replaceAll("");
        }
        return result.trim();
    }
}
