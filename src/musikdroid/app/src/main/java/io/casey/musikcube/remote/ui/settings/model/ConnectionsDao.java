package io.casey.musikcube.remote.ui.settings.model;

import android.arch.persistence.room.Dao;
import android.arch.persistence.room.Insert;
import android.arch.persistence.room.OnConflictStrategy;
import android.arch.persistence.room.Query;

import java.util.List;

@Dao
public interface ConnectionsDao {
    @Insert(onConflict = OnConflictStrategy.REPLACE)
    void insert(Connection... connections);

    @Query("SELECT * FROM Connection ORDER BY LOWER(name)")
    List<Connection> query();

    @Query("SELECT * FROM Connection WHERE name=:name")
    Connection query(String name);

    @Query("DELETE FROM Connection WHERE name=:name")
    void delete(String name);

    @Query("UPDATE Connection SET name=:newName WHERE name=:oldName")
    void rename(String oldName, String newName);
}
