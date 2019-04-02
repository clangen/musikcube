package io.casey.musikcube.remote.ui.settings.model;

import java.util.List;

import androidx.room.Dao;
import androidx.room.Insert;
import androidx.room.OnConflictStrategy;
import androidx.room.Query;

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
