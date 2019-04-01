package io.casey.musikcube.remote.service.playback.impl.streaming.db;

import androidx.room.Dao;
import androidx.room.Insert;
import androidx.room.OnConflictStrategy;
import androidx.room.Query;

import java.util.List;

@Dao
public interface OfflineTrackDao {
    @Insert(onConflict = OnConflictStrategy.REPLACE)
    void insertTrack(OfflineTrack... track);

    @Query("SELECT * FROM OfflineTrack " +
           "ORDER BY albumArtist ASC, album ASC, trackNum ASC, TITLE ASC " +
           "LIMIT :limit OFFSET :offset")
    List<OfflineTrack> queryTracks(int limit, int offset);

    @Query("SELECT * FROM OfflineTrack " +
            "WHERE " +
            "  title LIKE '%'||LOWER(:filter)||'%' OR " +
            "  album LIKE '%'||LOWER(:filter)||'%' OR " +
            "  albumArtist LIKE '%'||LOWER(:filter)||'%' OR " +
            "  artist LIKE '%'||LOWER(:filter)||'%' " +
            "ORDER BY albumArtist ASC, album ASC, trackNum ASC, TITLE ASC " +
            "LIMIT :limit OFFSET :offset")
    List<OfflineTrack> queryTracks(String filter, int limit, int offset);

    @Query("SELECT * FROM OfflineTrack " +
            "ORDER BY albumArtist ASC, album ASC, trackNum ASC, TITLE ASC")
    List<OfflineTrack> queryTracks();

    @Query("SELECT * FROM OfflineTrack " +
            "WHERE " +
            "  title LIKE '%'||LOWER(:filter)||'%' OR " +
            "  album LIKE '%'||LOWER(:filter)||'%' OR " +
            "  albumArtist LIKE '%'||LOWER(:filter)||'%' OR " +
            "  artist LIKE '%'||LOWER(:filter)||'%' " +
            "ORDER BY albumArtist ASC, album ASC, trackNum ASC, TITLE ASC ")
    List<OfflineTrack> queryTracks(String filter);

    @Query("SELECT COUNT(*) FROM OfflineTrack")
    int countTracks();

    @Query("SELECT COUNT(*) FROM OfflineTrack " +
            "WHERE " +
            "  title LIKE '%'||LOWER(:filter)||'%' OR " +
            "  album LIKE '%'||LOWER(:filter)||'%' OR " +
            "  albumArtist LIKE '%'||LOWER(:filter)||'%' OR " +
            "  artist LIKE '%'||LOWER(:filter)||'%' ")
    int countTracks(String filter);

    @Query("SELECT DISTINCT uri FROM OfflineTrack")
    List<String> queryUris();

    @Query("DELETE FROM OfflineTrack WHERE uri IN(:uris)")
    void deleteByUri(List<String> uris);
}
