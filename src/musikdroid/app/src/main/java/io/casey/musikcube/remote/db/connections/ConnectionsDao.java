package io.casey.musikcube.remote.db.connections;

import android.arch.persistence.room.Dao;
import android.arch.persistence.room.Insert;
import android.arch.persistence.room.OnConflictStrategy;
import android.arch.persistence.room.Query;

import java.util.List;

@Dao
public interface ConnectionsDao {
    @Insert(onConflict = OnConflictStrategy.REPLACE)
    void insertConnection(Connection... connections);

    @Query("SELECT * FROM Connection ORDER BY LOWER(name)")
    List<Connection> queryConnections();

    @Query("SELECT * FROM Connection WHERE name=:name")
    Connection queryConnection(String name);
}
