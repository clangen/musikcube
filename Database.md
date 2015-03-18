# Introduction #

The database that the musikCore is using is a [SQLite](http://www.sqlite.org) database.
The database design is a normalized design that supports multiple custom metadata.

# Design #

![http://onnerby.se/~daniel/mc2db.png](http://onnerby.se/~daniel/mc2db.png)


Some explanations:

Genres: this design supports multiple genres per track through the track\_genres table. There is also a track.visible\_genre\_id that points to an aggregated genre string if it has multiple genres. This because you may retrieve the genres faster for listing.

Artists: Same as genres. Multiple artists are supported.

Folders: the folders table lists the paths to where you may find the files. The tracks.filename will only contain the filename without the path that instead is located in the folders-table. This makes the database a lot smaller and will not make it very much slower.

Meta: the meta\_keys, meta\_values and track\_meta tables is a way of setting any type of data to a track. This design of the metadata is very normalized and will allow for fast retrieval of unique listings (i.e if you want a listing like the genres and artists windows).