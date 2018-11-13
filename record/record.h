#ifndef __RECORD_H__
#define __RECORD_H__

#include <stdlib.h>
#include <stdint.h>
#include "list.h"

enum ElementType {
    ELEMENT_CHAR,
    ELEMENT_UCHAR,
    ELEMENT_SMALLINT,
    ELEMENT_SMALLUINT,
    ELEMENT_INT,
    ELEMENT_UINT,
    ELEMENT_BIGINT,
    ELEMENT_BIGUINT,
    ELEMENT_BINARY,
    ELEMENT_TEXT,
    ELEMENT_TYPE_MAX
};
struct element {
    union {
        int64_t bigint;
        uint64_t biguint;
        char *ptr;
    } data;

    char *name;
    enum ElementType type;
    int size;
};

struct io_context {
    int fd;
    off_t filesize;

    char *buf;
    char *rdptr, *end;
};

struct record {
    struct element **data;
    struct list_head list;
    struct hlist_node hlist;

    struct element *uid, *key;
    int cn;
};

#define RECORD_HASH_SIZE (128)
#define RECORD_HASHTABLE_SIZE (RECORD_HASH_SIZE << 1)
#define RECORD_HASH_MASK (RECORD_HASH_SIZE - 1)
struct record_context {
    struct list_head records;
    struct element **title;
    int cn;
    /* hash table sorted by uid */
    struct hlist_head *uid;
};

#endif // __RECORD_H__
