targets = feedseek mpg123_to_wav mpg123_to_wav_replaced_io scan id3dump mpglib dump_seekindex

all: $(targets)

# On largefile-aware systems you might want to use these instead:
#MPG123_CFLAGS  := $(shell pkg-config --cflags libmpg123_64)
#MPG123_LDFLAGS := $(shell pkg-config --libs   libmpg123_64)
# This works on sane setups where off_t is off_t, and just that.
MPG123_CFLAGS  := $(shell pkg-config --cflags libmpg123)
MPG123_LDFLAGS := $(shell pkg-config --libs   libmpg123)
SND_CFLAGS     := $(shell pkg-config --cflags sndfile)
SND_LDFLAGS    := $(shell pkg-config --libs   sndfile)

# Oder of libs not that important here...
compile = $(CC) $(CPPFLAGS) $(CFLAGS) $(MPG123_CFLAGS)
linkflags = $(MPG123_LDFLAGS) $(LDFLAGS)

mpg123_to_wav: mpg123_to_wav.c
	$(compile) -o mpg123_to_wav mpg123_to_wav.c $(SND_CFLAGS) $(SND_LDFLAGS) $(linkflags)

mpg123_to_wav_replaced_io: mpg123_to_wav_replaced_io.c
	$(compile) -o $@ $< $(SND_CFLAGS) $(SND_LDFLAGS) $(linkflags)

feedseek: feedseek.c
	$(compile) -o feedseek feedseek.c $(linkflags)

scan: scan.c
	$(compile) -o scan scan.c $(linkflags)

id3dump:id3dump.c
	$(compile) -o id3dump id3dump.c $(linkflags)

dump_seekindex: dump_seekindex.c
	$(compile) -o dump_seekindex dump_seekindex.c $(linkflags)

mpglib: mpglib.c
	$(compile) -o mpglib mpglib.c $(linkflags)

clean:
	rm -vf $(targets)
