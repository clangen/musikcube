package io.casey.musikcube.remote.playback;

public class Metadata {
    public interface Track {
        String ID = "id";
        String TITLE = "title";
        String ALBUM = "album";
        String ALBUM_ID = "album_id";
        String ALBUM_ARTIST = "album_artist";
        String ALBUM_ARTIST_ID = "album_artist_id";
        String GENRE = "genre";
        String GENRE_ID = "visual_genre_id";
        String ARTIST = "artist";
        String ARTIST_ID = "visual_artist_id";
    }

    public interface Album {
        String TITLE = "title";
        String ALBUM_ARTIST = "album_artist";
    }

    private Metadata() {

    }
}
