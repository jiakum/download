#ifndef _TRACK_SQL_H__
#define _TRACK_SQL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <string.h>

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
    ELEMENT_VARCHAR,
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

struct sql_record {
    struct element *elist;
    int count;
};

#define SKIP_UNTIL(buf, end, skip)   do { \
                                        if(*buf++ == skip) \
                                            break; \
                                     } while(buf < end)

#define SKIP_SPACE(buf, end)   do { \
                                  char c = *buf;\
                                  if(c == ' ' || c == ' ') \
                                      buf++; \
                                  else \
                                      break; \
                               } while(buf < end)
                               
#define ALIGN4(size) (((size) + 3) & ~0x03 )

static inline int read_line(char **buf, char *end, char *target, int maxisze)
{
    char *ptr = *buf;
    int size = 0;

    if(ptr >= end)
        return -1;

    while(ptr < end && ++size < maxisze) {
        if(*ptr == '\n' || *ptr == '\r') {
            break;
        }

        *target++ = *ptr++;
    }

    ptr++;
    *target = '\0';
    *buf = ptr;

    return size;
}

static inline char *strnstr(char *buf, char *end, const char *target)
{
    int n = strlen(target);

    while(buf + n < end) {
        if(strncmp(buf, target, n) == 0)
            return buf;

        buf++;
    }

    return NULL;
}
    
static inline char *record_skip_n(char *buf, char *end, int n)
{
    while(n-- > 0) {
        buf = strnstr(buf, end, ",");

        if(buf == NULL)
            break;

        buf++;
    }

    return buf;
}

static inline int parse_one(char *buf, char *end, char *target, int maxsize)
{
    char *start = buf;
    
    if(*buf == ',') 
        buf++;
    if(buf < end && *buf == '\'') 
        buf++;

    while(buf < end && *buf != '\'' && *buf != ',') {
        char c = *buf++;
        
        maxsize--;
        if(maxsize > 0)
            *target++ = c;
    }
    *target = '\0';
    if(buf < end && *buf == '\'') 
        buf++;
    if(buf < end && *buf == ',') 
        buf++;

    return buf - start;
}


int sql_read_talbe_title(char *src, char *end, struct sql_record *record);
int sql_read_one_record(char **buf, char *end, struct sql_record *record);
int sql_preinit_record(struct sql_record *record, int maxsize);


#ifdef __cplusplus
}
#endif

#endif
