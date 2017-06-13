package io.casey.musikcube.remote.offline;

import android.arch.persistence.room.Entity;
import android.arch.persistence.room.PrimaryKey;

import org.json.JSONException;
import org.json.JSONObject;

import io.casey.musikcube.remote.playback.Metadata;
import io.casey.musikcube.remote.util.Strings;

@Entity
public class OfflineTrack {
    @PrimaryKey
    public String externalId;

    public String uri, title, album, artist, albumArtist, genre;
    public long albumId, artistId, albumArtistId, genreId;
    public int trackNum;

    public boolean fromJSONObject(final String uri, final JSONObject from) {
        if (Strings.empty(uri)) {
            throw new IllegalArgumentException("uri cannot be empty");
        }

        final String externalId = from.optString(Metadata.Track.EXTERNAL_ID, "");
        if (Strings.empty(externalId)) {
            return false;
        }

        this.externalId = externalId;
        this.uri = uri;
        this.title = from.optString(Metadata.Track.TITLE, "");
        this.album = from.optString(Metadata.Track.ALBUM, "");
        this.artist = from.optString(Metadata.Track.ARTIST, "");
        this.albumArtist = from.optString(Metadata.Track.ALBUM_ARTIST, "");
        this.genre = from.optString(Metadata.Track.GENRE, "");
        this.albumId = from.optLong(Metadata.Track.ALBUM_ID, -1L);
        this.artistId = from.optLong(Metadata.Track.ARTIST_ID, -1L);
        this.albumArtistId = from.optLong(Metadata.Track.ALBUM_ARTIST_ID, -1L);
        this.genreId = from.optLong(Metadata.Track.GENRE_ID, -1L);
        this.trackNum = from.optInt(Metadata.Track.TRACK_NUM, 0);

        return true;
    }

    JSONObject toJSONObject() {
        try {
            final JSONObject json = new JSONObject();
            json.put(Metadata.Track.TITLE, title);
            json.put(Metadata.Track.ALBUM, album);
            json.put(Metadata.Track.ARTIST, artist);
            json.put(Metadata.Track.ALBUM_ARTIST, albumArtist);
            json.put(Metadata.Track.GENRE, genre);
            json.put(Metadata.Track.ALBUM_ID, albumId);
            json.put(Metadata.Track.ARTIST_ID, artistId);
            json.put(Metadata.Track.ALBUM_ARTIST_ID, albumArtistId);
            json.put(Metadata.Track.GENRE_ID, genreId);
            json.put(Metadata.Track.TRACK_NUM, trackNum);
            json.put(Metadata.Track.EXTERNAL_ID, externalId);
            json.put(Metadata.Track.URI, uri);
            return json;
        }
        catch (JSONException ex) {
            throw new RuntimeException("json serialization error");
        }
    }
}
