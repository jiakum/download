
#include "sql.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
                          
#include <unistd.h>
#include <sys/types.h>

#define BUILD_STRING(s)   .str = s,\
                          .size = __builtin_strlen(s) 
static struct {
    const char *str;
    int size;
} element_types[] = {
    { BUILD_STRING("char"),},
    { BUILD_STRING("char unsigned"), },
    { BUILD_STRING("smallint"), },
    { BUILD_STRING("smallint unsigned"), },
    { BUILD_STRING("int"), },
    { BUILD_STRING("int unsigned"), },
    { BUILD_STRING("bigint"), },
    { BUILD_STRING("bigint unsigned"), },
    { BUILD_STRING("binary"), },
    { BUILD_STRING("text"), },
    { BUILD_STRING("varchar"),},
    { BUILD_STRING("") },
};
    
static inline char *read_table_string(char **p, char *end)
{
    char *ptr = *p, *result, *rp;
    int size = 0;

    SKIP_SPACE(ptr, end);

    if(ptr >= end)
        return NULL;

    if(*ptr != '`')
        return NULL;
    ptr++;
    
    while((ptr + size < end) && (*(ptr + size) != '`'))
        size++;

    if(ptr >= end)
        return NULL;

    size = ALIGN4(size + 1);
    if((result = (char *)malloc(size)) == NULL)
        return NULL;

    rp = result;
    while(*ptr != '`')
        *rp++ = *ptr++;

    *rp = '\0';
    *p = ptr + 1;

    return result;
}

int sql_read_talbe_title(char *src, char *end, struct sql_record *record)
{
    char buf[1024], *ptr;
    struct element *el, *elist;
    int i, ret, n = 0, maxcn = 64;

    memset(record, 0, sizeof(*record));
    if((elist = (struct element *)malloc(maxcn * sizeof(*elist))) == NULL)
        return -1;
    memset(elist, 0, maxcn * sizeof(*elist));

    do {
        ret = read_line(&src, end, buf, sizeof(buf));

        if(ret < 0)
            goto failed;

        if(strncmp(buf, "CREATE TABLE", strlen("CREATE TABLE")) == 0)
            break;
    }while(1);

    do {
        ret = read_line(&src, end, buf, sizeof(buf));

        if(ret < 0)
            break;

        ptr = buf;
        end = ptr + ret;
        if(ptr[0] == ')')
            break;

        el = elist + n;
        el->name = read_table_string(&ptr, end);

        if(el->name == NULL || ptr >= end) {
            continue;
        }
        SKIP_SPACE(ptr, end);
        for(i = 0;i < ELEMENT_TYPE_MAX;i++) {
            if(!strncmp(ptr, element_types[i].str, element_types[i].size)) {
                ptr += element_types[i].size;
                if(*ptr == '(') {
                    ptr++;
                    el->size = strtol(ptr, &ptr, 0);
                    ptr++;
                }

                SKIP_SPACE(ptr, end);
                if(!strncmp(ptr, "unsigned", strlen("unsigned")))
                    i++;

                break;
            }
        }

        if(i >= ELEMENT_TYPE_MAX) {
            continue;
        }

        el->type = (ElementType)i;
        el->data.ptr = NULL;
        if(++n >= maxcn) {
            maxcn += 64;
            elist = (struct element *)realloc(elist, maxcn * sizeof(*elist));

            if(elist == NULL)
                goto failed;
        }
    } while(1);

    if(n == 0)
        goto failed;

    record->count = n;
    record->elist = elist;
    for(i = 0;i < n;i++) {
        struct element *el = elist + i;

        printf("read one element, name:%s, type:%d, size:%d\n", el->name, el->type, el->size);
    }
    return 0;

failed:
    if(elist)
        free(elist);

    return -1;
}

static int read_element(struct element *el, char **p, char *end)
{
    char *ptr = *p, *buf = (char *)el->data.ptr;
    int size = el->size;

    if((ptr == NULL) || (el->type != ELEMENT_TEXT && ptr + size >= end)) {
        return -1;
    }

    switch(el->type)
    {
        case ELEMENT_VARCHAR:
        case ELEMENT_CHAR:
        case ELEMENT_UCHAR:
            if(buf == NULL) {
                buf = (char *)malloc(ALIGN4(size + 1));

                if(buf == NULL)
                    return -ENOMEM;
                el->data.ptr = buf;
            }

            ptr++;
            while(*ptr != '\'' && size-- > 0)
                *buf++ = *ptr++;
            while(*ptr != '\'')
                ptr++;
            ptr += 2;
            *buf = '\0';
            break;
        case ELEMENT_INT:
        case ELEMENT_SMALLINT:
        case ELEMENT_BIGINT:
            el->data.bigint = strtoll(ptr, &ptr, 0);
            ptr++;
            break;
        case ELEMENT_SMALLUINT:
        case ELEMENT_UINT:
        case ELEMENT_BIGUINT:
            el->data.biguint = strtoull(ptr, &ptr, 0);
            ptr++;
            break;
        case ELEMENT_TEXT:
            size = 0;

            ptr++;
            if(buf == NULL) {
                while(*(ptr + size) != '\'' && *(ptr + size) != '\0' && ptr < end)
                    size++;

                buf = (char *)malloc(ALIGN4(size + 1));

                if(buf == NULL)
                    return -ENOMEM;
                el->data.ptr = buf;
            }

            memcpy(buf, ptr, size);
            ptr += size + 2;
            *(buf + size) = '\0';
            el->size = size;
            break;
        default:
            return -1;
    }

    *p = ptr;

    return 0;
}

int sql_read_one_record(char **buf, char *end, struct sql_record *record)
{
    char *ptr = *buf;
    int i, ret = 0;

    SKIP_UNTIL(ptr, end, '(');
    for(i = 0;i < record->count;i++) {
        struct element *el = record->elist + i;
        ret = read_element(el, &ptr, end);

        if(ret < 0)
            break;
    }
    *buf = ptr;

    return ret;
}

int sql_preinit_record(struct sql_record *record, int maxsize)
{
    char *buf;
    int i;
    
    for(i = 0;i < record->count;i++) {
        struct element *el = record->elist + i;
        int size = el->size;
        
        switch(el->type)
        {
            case ELEMENT_VARCHAR:
            case ELEMENT_CHAR:
            case ELEMENT_UCHAR:
                buf = (char *)malloc(ALIGN4(size + 1));
                el->data.ptr = buf;        
                break;
            case ELEMENT_INT:
            case ELEMENT_SMALLINT:
            case ELEMENT_BIGINT:
            case ELEMENT_SMALLUINT:
            case ELEMENT_UINT:
            case ELEMENT_BIGUINT:
                break;
            case ELEMENT_TEXT:
                buf = (char *)malloc(maxsize);
                el->data.ptr = buf;
                break;
            default:
                break;
        }

    }

    return 0;
}

