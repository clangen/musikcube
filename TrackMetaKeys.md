# Tracks MetaKeys #

| **MetaKey** | **Description** | **id3v2 frame** |
|:------------|:----------------|:----------------|
| album | Album | TALB |
| artist | Artists | TPE1, TPE2 |
| bpm | Beats Per Minutes | TBPM |
| comment | Comments | TCOMM with empty description |
| composer | Composer | TCOM |
| conductor | Conductor | TPE3 |
| copyright | Copyright | TCOP |
| disc | Part. Ie CD number |TPOS |
| encoder | Encoder | TENC |
| filename | Filename |  |
| genre | Genre | TCON |
| interpreted | Interpreted, remixed, or otherwise modified by | TPE4 |
| language | Language |TLAN |
| lyrics | Lyrics | USLT |
| mood | Mood | TMOO or TCOMM with MusicMatch\_Mood description |
| org.artist | Original Artist | TOPE |
| org.writer | Original Lyricist/Text writer | TOLY |
| path | Path to file |  |
| publisher | Publisher | TPUB |
| textrating | Rating of track | TCOMM with MusicMatch\_Preference description |
| title | Track title | TIT2 |
| totaltracks | Total number of tracks | TRCK |
| track | Track number | TRCK |
| writer | Lyricist/Text writer | TEXT |
| year | Year | TYER or TDRC |



# Special MetaKeys #
There are some special keys that can be fetched from the library.

| visual\_artist | Coma separated list of Artists | Used when listing artists in the tracklist view |
|:---------------|:-------------------------------|:------------------------------------------------|
| visual\_genre | Coma separated list of Genres | Used when listing genres in the tracklist view |