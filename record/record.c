
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <inttypes.h>
#include <time.h>
#include <sys/time.h>
#include <sys/mman.h>

#include "record.h"

#define BUILD_STRING(s)   .str = s,\
                          .size = __builtin_strlen(s) 
static struct {
    char *str;
    int size;
} element_types[] = {
    {
    BUILD_STRING("char"),
    },
    {
    BUILD_STRING("char unsigned"),
    },
    {
    BUILD_STRING("smallint"),
    },
    {
    BUILD_STRING("smallint unsigned"),
    },
    {
    BUILD_STRING("int"),
    },
    {
    BUILD_STRING("int unsigned"),
    },
    {
    BUILD_STRING("bigint"),
    },
    {
    BUILD_STRING("bigint unsigned"),
    },
    {
    BUILD_STRING("binary"),
    },
    {
    BUILD_STRING("text"),
    },
    {
    BUILD_STRING("")
    },
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

static int usage(char *func)
{
    printf("usage:%s dbfile\n", func);
    return(0);
}

int64_t get_current_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static inline int hashfn(char *uid)
{
    int val = uid[14] ^ uid[15];
    return val & RECORD_HASH_MASK;
}

static int read_line(struct io_context *io, char *buf, int max)
{
    char *ptr, *end;
    int size = 0;

    max -= 1;
    ptr = io->rdptr;
    end = io->end;
    if(ptr >= end)
        return EOF;

    while(ptr < end && size < max) {
        if(*ptr == '\n' || *ptr == '\r') {
            break;
        }

        *buf++ = *ptr++;
        size++;
    }

    *buf = '\0';
    io->rdptr = ptr + 1;

    return size;
}

static void skip_to_next_line(struct io_context *io)
{
    char *ptr, *end;

    ptr = io->rdptr;
    end = io->end;

    while(ptr < end) {
        if(*ptr == '\n' || *ptr == '\r') {
            break;
        }
        ptr++;
    }

    io->rdptr = ptr + 1;
}

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
    if((result = malloc(size)) == NULL)
        return NULL;

    rp = result;
    while(*ptr != '`')
        *rp++ = *ptr++;

    *rp = '\0';
    *p = ptr + 1;

    return result;
}

static int read_talbe_title(struct io_context *io, struct record_context *rctx)
{
    char buf[1024], *ptr, *end;
    struct element *el;
    int i, ret, n = 0;

    rctx->cn = 64;
    if((rctx->title = malloc(rctx->cn * sizeof(*rctx->title))) == NULL)
        return -ENOMEM;

    do {
        ret = read_line(io, buf, sizeof(buf));
        
        if(ret < 0)
            return ret;

        if(!strncmp(buf, "CREATE TABLE", __builtin_strlen("CREATE TABLE")))
            break;
    }while(1);

    do {
        ret = read_line(io, buf, sizeof(buf));
        
        if(ret < 0)
            break;

        ptr = buf;
        end = ptr + ret;
        if(ptr[0] == ')')
            break;

        if((el = calloc(1, sizeof(*el))) == NULL)
            return -1;

        el->name = read_table_string(&ptr, end);

        if(el->name == NULL || ptr >= end) {
            free(el);
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
                if(!strncmp(ptr, "unsigned", __builtin_strlen("unsigned")))
                    i++;

                break;
            }
        }

        if(i >= ELEMENT_TYPE_MAX) {
            free(el);
            continue;
        }

        el->type = i;
        el->data.ptr = NULL;
        if(n >= rctx->cn) {
            rctx->cn += 64;
            rctx->title = realloc(rctx->title, rctx->cn * sizeof(*rctx->title));

            if(rctx->title == NULL)
                return -ENOMEM;
        }
        rctx->title[n++] = el;
    } while(1);

    if(n == 0)
        return -1;

    rctx->cn = n;
    for(i = 0;i < rctx->cn;i++) {
        struct element *el = rctx->title[i];

        printf("read one element, name:%s, type:%d, size:%d\n", el->name, el->type, el->size);
    }
    return 0;
}

static struct record *alloc_record(struct record_context *rctx)
{
    struct record *re = calloc(1, sizeof(*re));
    struct element **ptr = malloc(rctx->cn * sizeof(*ptr));
    int i;

    if(re == NULL || ptr == NULL)
        return NULL;

    re->cn = rctx->cn;
    re->data = ptr;
    for(i = 0;i < rctx->cn;i++) {
        struct element *el = rctx->title[i];
        struct element *n = malloc(sizeof(*n));

        if(n == NULL) 
            return NULL;

        memcpy(n, el, sizeof(*n));
        re->data[i] = n;

        if(!strcmp(n->name, "uid"))
            re->uid = n;
    }

    return re;
}

static int read_element(struct element *el, char **p, char *end)
{
    char *ptr = *p, *buf;
    int size = el->size;

    if(el->type != ELEMENT_TEXT && ptr + size >= end) {
        return -EOF;
    }

    switch(el->type)
    {
        case ELEMENT_CHAR:
        case ELEMENT_UCHAR:
            buf = malloc(ALIGN4(size + 1));

            if(buf == NULL)
                return -ENOMEM;
            el->data.ptr = buf;

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
            while(*(ptr + size) != '\'' && *(ptr + size) != '\0' && ptr < end)
                size++;

            buf = malloc(ALIGN4(size + 1));

            if(buf == NULL)
                return -ENOMEM;

            memcpy(buf, ptr, size);
            ptr += size + 2;
            *(buf + size) = '\0';
            el->data.ptr = buf;
            el->size = size;
            break;
        default:
            return -1;
    }

    *p = ptr;

    return 0;
}

static void free_record(struct record *re)
{
    int i;

    for(i = 0;i < re->cn;i++) {
        struct element *el = re->data[i];

        switch(el->type)
        {
            case ELEMENT_CHAR:
            case ELEMENT_UCHAR:
            case ELEMENT_TEXT:
                free(el->data.ptr);
                break;
            default:
                break;
        }

        free(el);
    }
    free(re->data);

    if(re->list.next && re->list.prev)
        list_del(&re->list);

    if(!hlist_unhashed(&re->hlist))
        hlist_del(&re->hlist);

    free(re);
}

static int read_all_record(struct io_context *io, struct record_context *rctx)
{
    struct record *re;
    char *buf, *ptr, *end;
    int ret, i = 0;
#define MAX_LINE_SIZE (1024 * 1024 * 20)

    do {
        buf = io->rdptr;

        if(buf + strlen("INSERT INTO ") >= io->end)
            break;
        if(strncmp(buf, "INSERT INTO ", strlen("INSERT INTO "))) {
            skip_to_next_line(io);
            continue;
        }

        printf("%s,%d, sizeof element:%d\n", __func__, __LINE__, (int)sizeof(struct element));
        ptr = buf;
        end = io->end;
        do {

            re = alloc_record(rctx);
            if(re == NULL)
                return -1;

            SKIP_UNTIL(ptr, end, '(');

            for(i = 0;i < re->cn;i++) {
                struct element *el = re->data[i];

                ret = read_element(el, &ptr, end);
                if(ret < 0) {
                    break;
                }
            }

            list_add_tail(&re->list, &rctx->records);
            if(!re->uid->data.ptr)
                free_record(re);

            SKIP_UNTIL(ptr, end, ')');
        } while(ptr < end && *ptr == ',');
        io->rdptr = ptr;
        skip_to_next_line(io);

    } while(1);

    return 0;
}

static void dump_record(struct record *re)
{
    char buf[1024];
    int len = 0, i;

    for(i = 0;i < re->cn;i++) {
        struct element *el = re->data[i];

        switch(el->type)
        {
            case ELEMENT_SMALLINT:
            case ELEMENT_INT:
            case ELEMENT_BIGINT:
                len += snprintf(buf + len, sizeof(buf) - len, "%"PRId64"  ", el->data.bigint);
                break;
            case ELEMENT_BIGUINT:
                if(!strcmp("date", el->name)) {
                    time_t t = el->data.biguint / 1000;
                    struct tm calendar;

                    gmtime_r(&t, &calendar);
                    len += snprintf(buf + len, sizeof(buf) - len, "%04d-%02d-%02d %02d:%02d:%02d  ", 
                            calendar.tm_year + 1900, calendar.tm_mon + 1, calendar.tm_mday, calendar.tm_hour, calendar.tm_min, calendar.tm_sec);
                    break;
                }
                /* go through */
            case ELEMENT_SMALLUINT:
            case ELEMENT_UINT:
                len += snprintf(buf + len, sizeof(buf) - len, "%"PRIu64"  ", el->data.biguint);
                break;
            case ELEMENT_CHAR:
            case ELEMENT_UCHAR:
            case ELEMENT_TEXT:
                len += snprintf(buf + len, sizeof(buf) - len, "%s  ", el->data.ptr);
                break;
            default:
                break;
        }
    }

    printf("%s\n", buf);
}

static void dump_records(struct list_head *head)
{
    struct list_head *pos;

    list_for_each(pos, head) {
        struct record *re = list_entry(pos, struct record, list);

        dump_record(re);
    }
}

int scan_table(struct record_context *rctx, void (*action)(struct record *, void *priv), void *priv)
{
    struct hlist_node *pos, *n;
    struct hlist_head *heads = rctx->uid;
    int i;
        
    for(i = 0;i < RECORD_HASHTABLE_SIZE;i++) {
        hlist_for_each_safe(pos, n, heads + i) {
            struct record *re = hlist_entry(pos, struct record, hlist);

            action(re, priv);
        }
    }

    return 0;
}

/*
 * return 1 if they are the same uid
 * return ret_if_empty if hlist is empty
 */
static inline int is_same_uid(struct hlist_head *head, char *uid, int ret_if_empty)
{
    struct hlist_node *n;
    struct record *rn;

    if(hlist_empty(head))
        return ret_if_empty;

    n = head->first;
    rn = hlist_entry(n, struct record, hlist);
    return !strcmp(uid, rn->uid->data.ptr);
}

static int search_uid(struct record_context *rctx, char *uid, struct list_head *head)
{
    struct hlist_head *heads = rctx->uid;
    struct hlist_node *pos;
    struct record *rn;
    int key;
        
    key  = hashfn(uid);
    if(is_same_uid(heads + key, uid, 0))
        goto found;

    key += RECORD_HASH_SIZE;
    if(is_same_uid(heads + key, uid, 0))
        goto found;

    for(key = 0;key < RECORD_HASHTABLE_SIZE;key++) {
        if(is_same_uid(heads + key, uid, 0)) {
            goto found;
        }
    }

    return 0;

found:
    hlist_for_each(pos, heads + key) {
        rn = hlist_entry(pos, struct record, hlist);
        list_del(&rn->list);
        list_add_tail(&rn->list, head);
    }

    return 0;
}

void default_search_action(struct record *re, void *priv)
{
    struct list_head *head = priv;
    char *name = "command";
    int value = 2, i;

    for(i = 0;i < re->cn;i++) {
        struct element *el = re->data[i];

        if(!strcmp(name, el->name)) {
            if(el->data.bigint == value) {
                list_del(&re->list);
                list_add_tail(&re->list, head);
            }
            break;
        }
    }
}

#define CMP_RECORD(re1, action, re2) (((re1)->key->data.bigint) action ((re2)->key->data.bigint))

#define M 9

struct {
	int start,end;
} *stack;
static int stack_ptr = 0;
#define PUSH(l,r) 	do { \
						if(l < r) { \
							stack[stack_ptr].start = l;\
							stack[stack_ptr].end = r;\
							stack_ptr++;\
						 }\
					}while(0);

static inline int PULL(int *l,int *r) {
	if(stack_ptr) {
		stack_ptr--;
		*l = stack[stack_ptr].start;
		*r = stack[stack_ptr].end;
		return 1; // success
	}
	return 0;
}

void quick_sort(struct record **records, int n)
{
    struct record *k, *tmp;
	int i, j, r, l;

	stack = malloc(n * sizeof(*stack));
	l = 0; r = n - 1;

	while(1) {
		if(r - l < M && !PULL(&l, &r))
			break;

		k = records[(l + r) / 2]; j=r+1; i=l - 1;
		while(1) {
			while((++i < r) && (CMP_RECORD(records[i] ,< ,k)));
			while((--j > l) && (CMP_RECORD(records[j] ,> ,k)));

			if(i < j) {
				tmp = records[j];
				records[j] = records[i];
				records[i] = tmp;
				continue;
			} else {
				tmp = records[j];
				records[j] = records[(l + r) / 2];
				records[(l + r) / 2] = tmp;
				break;
			}
		}

		if(r - j > M) { 
			if(j - l > M) {
				PUSH(j + 1 ,r);
				r = j - 1;
			} else 
				l = j + 1;
			continue;
		} else if(j - l > M) {
			PUSH(l, j-1);
			l = j + 1;
			continue;
		} else if(!PULL(&l ,&r))
			break;
	}

	for(j = 1; j < n; j++) {
		if(CMP_RECORD(records[j-1] ,<= ,records[j]))
			continue;
		k = records[j];
		for(i = j - 1; i >= 0; i--) {
			if(CMP_RECORD(records[i] ,<= ,k))
				break;
			records[i+1] = records[i];
		}
		records[i+1] = k;
	}

    free(stack);
}

void heap_sort(struct record **records, int n)
{
    struct record *k;
    int i, j, r, l;

    l = ((n-1)>>1) + 1; r = n - 1;

    while(1) {
        if(l > 0) {
            l -= 1;
            k = records[l];
        } else if(r > 0) {
            k = records[r];
            records[r] = records[l];
            records[l] = k;
            r--;
        } else
            break;

        i = l; j = i << 1;
        while(j < r) {
            if(CMP_RECORD(records[j], <, records[j+1]))
                j += 1;
            if(CMP_RECORD(records[j], <=, k))
                break;
            records[i] = records[j];
            i = j; j <<= 1;
        }
        if((j == r) && CMP_RECORD(records[j] ,>, k)) {
            records[i] = records[j];
            i = j;
        }
        records[i] = k;
    }

    return;
}

static int sort_record_by(struct list_head *head, char *name)
{
    struct record **records;
    struct list_head *pos, *tmp;
    int n = 0, i, key = -1;

    printf("%s,%d, current time:%"PRId64"\n", __func__, __LINE__, get_current_time_ms());
    list_for_each(pos, head) {
        struct record *re = list_entry(pos, struct record, list);

        n++;
        if(key < 0) {
            for(i = 0;i < re->cn;i++) {
                struct element *el = re->data[i];

                if(!strcmp(name, el->name)) {
                    key = i;
                    break;
                }
            }

            if(key < 0) {
                printf("%s error ! can not find %s in the table ?\n", __func__, name);
                return -EINVAL;
            }
        }

        re->key = re->data[key];
    }

    if(n == 0)
        return 0;

    printf("%s %s, count:%d\n", __func__, name, n);
    printf("%s,%d, current time:%"PRId64"\n", __func__, __LINE__, get_current_time_ms());

    records = malloc(n * sizeof(*records));
    if(records == NULL)
        return -ENOMEM;

    i = 0;
    list_for_each_safe(pos, tmp, head) {
        struct record *re = list_entry(pos, struct record, list);

        records[i++] = re;
        list_del(pos);
    }

    printf("%s,%d, current time:%"PRId64"\n", __func__, __LINE__, get_current_time_ms());
    //heap_sort(records, n);
    quick_sort(records, n);
    printf("%s,%d, current time:%"PRId64"\n", __func__, __LINE__, get_current_time_ms());

    for(i = 0;i < n;i++) {
        list_add_tail(&records[i]->list, head);
    }

    free(records);

    return 0;
}

/*
 * head : records to be filtered 
 * dest : store filter-out records , can be NULL
 * int (*filter)(struct element *el, void *priv): filter function, return 1 to filter out record
 *       should not be NULL
 */
static int filter_record(struct list_head *head, struct list_head *dest, 
        int (*filter)(struct record *re, void *priv), void *priv)
{
    struct list_head *pos, *tmp;

    list_for_each_safe(pos, tmp, head) {
        struct record *re = list_entry(pos, struct record, list);

        if(filter(re, priv)) {
            if(dest) {
                list_del(&re->list);
                list_add_tail(&re->list, dest);
            }
        }
    }

    return 0;
}

static int exclude_none_HBP(struct record *re, void *priv)
{
    int hbp = 0, i;

    for(i = 0;i < re->cn;i++) {
        struct element *el = re->data[i];

        if(!strcmp("command", el->name)) {
            if(el->data.bigint == 2)
                hbp = 1;
            break;
        }
    }

    if(hbp) {
        hbp = 0;
        for(i = 0;i < re->cn;i++) {
            struct element *el = re->data[i];

            if(!strcmp("ext_data", el->name)) {
                if(el->data.ptr) {
                    char *ptr = el->data.ptr;
                    int len = el->size;

                    if(len < (int)strlen("<P2P><D2R_HB><uid>"))
                        break;

                    ptr = strchr(ptr + strlen("<P2P><D2R_HB><uid>"), '>');
                    if(ptr == NULL)
                        break;

                    ptr++;
                    if(!strncmp(ptr, "<sid_list>", strlen("<sid_list>")))
                        break;

                    hbp = 1;
                }

                break;
            }
        }
    }

    if(hbp == 0)
        free_record(re);

    return 0;
}

static int make_uid_hashtable(struct record_context *rctx)
{
    struct list_head *pos, *tmp;
    struct hlist_node *n;
    struct hlist_head *heads = rctx->uid;
    struct record *rn;
    int key, i;

    list_for_each_safe(pos, tmp, &rctx->records) {
        struct record *re = list_entry(pos, struct record, list);

        key  = hashfn(re->uid->data.ptr);
        if(is_same_uid(heads + key, re->uid->data.ptr, 1)) {
            hlist_add_head(&re->hlist, heads + key);
            continue;
        }

        if(is_same_uid(heads + key + RECORD_HASH_SIZE, re->uid->data.ptr, 1)) {
            hlist_add_head(&re->hlist, heads + key + RECORD_HASH_SIZE);
            continue;
        }

        for(i = RECORD_HASH_SIZE;i < RECORD_HASHTABLE_SIZE;i++) {
            if(is_same_uid(heads + i, re->uid->data.ptr, 1)) {
                hlist_add_head(&re->hlist, heads + i);
                break;
            }
        }

        if(i == RECORD_HASHTABLE_SIZE) {
            for(i = 0;i < RECORD_HASH_SIZE;i++) {
                if(is_same_uid(heads + i, re->uid->data.ptr, 1)) {
                    hlist_add_head(&re->hlist, heads + i);
                    break;
                }
            }

            if(i == RECORD_HASHTABLE_SIZE) {
                printf("hash table full, enlarge hash table, uid:%s!\n", re->uid->data.ptr);
                free_record(re);
            }
        }
    }
        
    for(i = 0;i < RECORD_HASHTABLE_SIZE;i++) {
        if(hlist_empty(heads + i))
            continue;

        n = heads[i].first;
        rn = hlist_entry(n, struct record, hlist);
        printf("uid:%s\n", rn->uid->data.ptr);
    }

    return 0;
}

static void free_record_context(struct record_context *rctx)
{
    struct hlist_head *heads = rctx->uid;
    struct hlist_node *pos, *n;
    int i;

    for(i = 0;i < RECORD_HASHTABLE_SIZE;i++) {
        hlist_for_each_safe(pos, n, heads + i) {
            struct record *re = hlist_entry(pos, struct record, hlist);

            free_record(re);
        }
    }

    for(i = 0;i < rctx->cn;i++) {
        struct element *el = rctx->title[i];

        free(el);
    }

    free(rctx->title);
    free(rctx->uid);
    free(rctx);
}

static struct record_context *alloc_record_context(void)
{
    struct record_context *rctx = malloc(sizeof(*rctx));
    int i;

    if(rctx == NULL)
        return NULL;
    rctx->uid = malloc(RECORD_HASHTABLE_SIZE * sizeof(*rctx->uid));
    if(rctx->uid == NULL)
        return NULL;
    INIT_LIST_HEAD(&rctx->records);

    for(i = 0;i < RECORD_HASHTABLE_SIZE;i++)
        INIT_HLIST_HEAD(rctx->uid + i);

    return rctx;
}

static struct io_context *init_io_context(char *filename)
{
    struct io_context *io = malloc(sizeof(*io));
    int fd;

    if(io == NULL)
        return NULL;

    if((fd = open(filename, O_RDONLY)) < 0)
        goto open_failed;

    io->filesize = lseek(fd, 0, SEEK_END);
    if(io->filesize < 0)
        goto failed;
    lseek(fd, 0, SEEK_SET);

    io->buf = mmap(NULL, io->filesize, PROT_READ, MAP_SHARED, fd, 0);
    if(io->buf == MAP_FAILED)
        goto failed;

    io->fd = fd;
    io->rdptr = io->buf;
    io->end = io->buf + io->filesize;

    return io;

failed:
    close(fd);
open_failed:
    free(io);
    return NULL;
}

static void free_io_context(struct io_context *io)
{
    if(io) {
        munmap(io->buf, io->filesize);
        close(io->fd);
        free(io);
    }
}

int main(int argc, char **argv)
{
    struct io_context *io;
    struct record_context *rctx;
    char buf[1024];

    if(argc < 2) {
        goto args_err;
    }

    printf("%s,%d, current time:%"PRId64"\n", __func__, __LINE__, get_current_time_ms());
    io = init_io_context(argv[1]);
    rctx = alloc_record_context();

    if(io == NULL || rctx == NULL)
        goto failed;

    printf("%s,%d, current time:%"PRId64"\n", __func__, __LINE__, get_current_time_ms());
    if(read_talbe_title(io, rctx) < 0)
        goto failed;
    printf("%s,%d, current time:%"PRId64"\n", __func__, __LINE__, get_current_time_ms());
    read_all_record(io, rctx);
    printf("%s,%d, current time:%"PRId64"\n", __func__, __LINE__, get_current_time_ms());
    sort_record_by(&rctx->records, "date");
    printf("%s,%d, current time:%"PRId64"\n", __func__, __LINE__, get_current_time_ms());
    filter_record(&rctx->records, NULL, exclude_none_HBP, NULL);
    printf("%s,%d, current time:%"PRId64"\n", __func__, __LINE__, get_current_time_ms());
    make_uid_hashtable(rctx);
    printf("%s,%d, current time:%"PRId64"\n", __func__, __LINE__, get_current_time_ms());

    free_io_context(io);

    while(fgets(buf, sizeof(buf), stdin)) {
        struct list_head head, *pos, *n;

        INIT_LIST_HEAD(&head);
        if(buf[strlen(buf) - 1] == '\n')
            buf[strlen(buf) - 1] = '\0';

        search_uid(rctx, buf, &head);

        if(!list_empty(&head)) {
            printf("search success\n");
            sort_record_by(&head, "date");
            dump_records(&head);
            list_for_each_safe(pos, n, &head) {
                list_del(pos);
                list_add_tail(pos, &rctx->records);
            }
        } else {
            printf("no record found for uid:%s\n", buf);
        }
    }

    free_record_context(rctx);
    return 0;

failed:
    if(rctx)
        free_record_context(rctx);
    if(io)
        free_io_context(io);
    printf("%s is an illegal mysql db file\n", argv[1]);
args_err:
    return usage(argv[0]);
}
