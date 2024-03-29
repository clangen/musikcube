// Copyright (c) 2023 Anton Zhiyanov, MIT License
// https://github.com/nalgeon/sqlean

// Unicode support for SQLite.

#ifndef UNICODE_EXTENSION_H
#define UNICODE_EXTENSION_H

#include <sqlite/sqlite3.h>

extern int unicode_init(sqlite3* db);

#endif /* UNICODE_EXTENSION_H */
