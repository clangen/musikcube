package io.casey.musikcube.remote.service.gapless.db;

import android.arch.persistence.room.Dao;
import android.arch.persistence.room.Insert;
import android.arch.persistence.room.OnConflictStrategy;
import android.arch.persistence.room.Query;

import java.util.List;

@Dao
public interface GaplessDao {
    @Insert(onConflict = OnConflictStrategy.REPLACE)
    void insert(GaplessTrack... track);

    @Query("UPDATE GaplessTrack " +
           "SET state=:state " +
           "WHERE url=:url ")
    void update(int state, String url);

    @Query("DELETE FROM GaplessTrack WHERE state=:state")
    void deleteByState(int state);

    @Query("DELETE FROM GaplessTrack WHERE url=:url")
    void deleteByUrl(String url);

    @Query("SELECT * FROM GaplessTrack WHERE state=:state")
    List<GaplessTrack> queryByState(int state);

    @Query("SELECT * FROM GaplessTrack WHERE url=:url")
    List<GaplessTrack> queryByUrl(String url);
}
