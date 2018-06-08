cmake_minimum_required(VERSION 3.10)
project(db)

set(CMAKE_CXX_STANDARD 11)

include_directories(dbL/include)
include_directories("dbL/sqlite-demo/VFS layer")

add_executable(db
        dbL/include/db.h
        "dbL/sqlite-demo/VFS layer/sqlite3.h"
        "dbL/sqlite-demo/VFS layer/test_demovfs.cpp"
        "dbL/sqlite-demo/VFS layer/vfs.h")
