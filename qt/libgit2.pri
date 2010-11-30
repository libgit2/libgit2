DEPENDPATH += $$PWD/src/ $$PWD/src/block-sha1 $$PWD/src/git $$PWD/src/ppc $$PWD/src/unix $$PWD/src/win32
INCLUDEPATH += $$PWD/../src/

LIBS += -lz

SOURCES += $$PWD/../src/blob.c \
           $$PWD/../src/commit.c \
           $$PWD/../src/delta-apply.c \
           $$PWD/../src/errors.c \
           $$PWD/../src/filelock.c \
           $$PWD/../src/fileops.c \
           $$PWD/../src/hash.c \
           $$PWD/../src/hashtable.c \
           $$PWD/../src/index.c \
           $$PWD/../src/odb.c \
           $$PWD/../src/oid.c \
           $$PWD/../src/person.c \
           $$PWD/../src/repository.c \
           $$PWD/../src/revwalk.c \
           $$PWD/../src/tag.c \
           $$PWD/../src/thread-utils.c \
           $$PWD/../src/tree.c \
           $$PWD/../src/util.c \
           $$PWD/../src/block-sha1/sha1.c

unix {
    SOURCES += $$PWD/../src/unix/map.c
}

win {
    SOURCES +=  \
        $$PWD/../src/win32/dir.c \
        $$PWD/../src/win32/fileops.c \
        $$PWD/../src/win32/map.c
}

