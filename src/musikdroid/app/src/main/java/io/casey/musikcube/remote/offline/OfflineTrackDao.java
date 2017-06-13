package io.casey.musikcube.remote.offline;

import android.arch.persistence.room.Dao;
import android.arch.persistence.room.Insert;
import android.arch.persistence.room.OnConflictStrategy;
import android.arch.persistence.room.Query;

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
            "ORDER BY albumArtist ASC, album ASC, trackNum ASC, TITLE ASC")
    List<OfflineTrack> queryTracks();

    @Query("SELECT COUNT(*) FROM OfflineTrack")
    int countTracks();

    @Query("SELECT DISTINCT uri FROM OfflineTrack")
    List<String> queryUris();

    @Query("DELETE FROM OfflineTrack WHERE uri IN(:uris)")
    void deleteByUri(List<String> uris);
}
