#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <endian.h>

#include <json-c/json.h>

#define HAR_BUFFER_SIZE (8 * 1024 * 1024)

struct har_context {
    FILE *fp;
    char *buf, *start, *end;
    int len;
};

static struct har_context *init_har_context()
{
    struct har_context *hctx = malloc(sizeof(struct har_context));

    if (hctx == NULL)
    {
        return NULL;
    }

    hctx->buf = (char *)malloc(HAR_BUFFER_SIZE);
    if (hctx->buf == NULL)
    {
        free (hctx);
        return NULL;
    }

    hctx->start = hctx->buf;
    hctx->end   = hctx->buf + HAR_BUFFER_SIZE;
    hctx->len   = 0;

    return hctx;
}

static void free_har_context(struct har_context *hctx)
{
    free(hctx->start);
    free(hctx);
}

static int usage (char *func)
{
    printf("%s inputfile outputfile\n", func);
    return 0;
}

static const char base64_table[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'
};
static int base64_decode (const char *base64, int input_size, char *output, int max_size)
{
    int len = 0, i, prefix = 0;
    char *start = output;

    for (i = 0;i < input_size;i++)
    {
        char c = base64[i], code;

        if (c >= 'A' && c <= 'Z')
        {
            code = c - 'A';
        }
        else if (c >= 'a' && c <= 'z')
        {
            code = c - 'a' + 26;
        }
        else if (c >= '0' && c <= '9')
        {
            code = c - '0' + 52;
        }
        else
        {
            int skip = 0;
            switch (c)
            {
                case '+':
                case '_':
                    code = 62;
                    break;
                case '/':
                case '-':
                    code = 63;
                    break;
                case '=':
                    code = 0;
                    break;
                case 0:
                case '\n':
                case '\r':
                case '\t':
                    skip = 1;
                    break;
                default:
                    printf ("unregconised code:total:%d, current:%d, c:0x%x\n", input_size, i, c);
                    return -1;
            }

            if (skip)
            {
                continue;
            }
        }

        if (output >= start + max_size)
        {
            printf ("output buffer not long enough\n");
            return -1;
        }

        switch (prefix)
        {
            case 0:
                *output = code << 2;
                prefix = 6;
                break;
            case 2:
                *output |= code;
                output++;
                prefix = 0;
                break;
            case 4:
                *output |= (code & 0x3c) >> 2;
                output++;
                *output = (code & 0x03) << 6;
                prefix = 2;
                break;
            case 6:
                *output |= (code & 0x30) >> 4;
                output++;
                *output = (code & 0x0f) << 4;
                prefix = 4;
                break;
            default:
                printf ("fatal error!!! prefix :%d\n", prefix);
                return -1;
        }
    }

    output ++;
    len = output - start;

    return len;
}

static void detect_box (unsigned char *buf, int len)
{
    unsigned int header = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
    printf ("ts header:0x%x\n", header);
}

static int decode_har(char *file_name, FILE *out_fp)
{
    unsigned char *buf = (char *)malloc(HAR_BUFFER_SIZE), header[4];
    struct har_context *hctx = NULL;
    int ret = 0, len, skip = 10;
    size_t i;

    if (buf == NULL)
    {
        ret = -1;
        goto failed;
    }

    header[0] = 0x0;
    header[1] = 0x0;
    header[2] = 0x1;
    header[3] = 0xBA;
    hctx = init_har_context();
    if (hctx == NULL)
    {
        goto failed;
    }

    struct json_object *root = json_object_from_file(file_name);
    struct json_object *obj;

    if (root == NULL) {
        printf ("failed to parse xml file:%s\n", file_name);
        goto failed;
    }

    printf ("get root json obeject type:%d\n", json_object_get_type(root));
    obj = json_object_object_get (root, "log");
    if (obj == NULL) {
        printf ("failed get log json obeject\n");
        goto failed;
    }
    root = obj;

    obj = json_object_object_get (root, "version");
    printf ("get version json obeject type:%d\n", json_object_get_type(obj));

    obj = json_object_object_get (root, "entries");
    printf ("get entries json obeject type:%d\n", json_object_get_type(obj));

    struct array_list *list = json_object_get_array(obj);
    if (list == NULL) {
        printf ("failed get array of entries json obeject\n");
        goto failed;
    }
    printf ("got length:%ld, size:%ld entries, %ld\n", list->length, list->size, json_object_array_length(obj));

    for (i = 0;i < list->length;i++) {
        struct json_object *entry, *request, *response, *content;

        entry = list->array[i];
        request = json_object_object_get (entry, "request");
        if (request == NULL) {
            printf ("failed get request of json obeject\n");
            continue;
        }

        response = json_object_object_get (entry, "response");
        if (response == NULL) {
            printf ("failed get response of json obeject\n");
            continue;
        }

        content = json_object_object_get (response, "content");
        if (content == NULL) {
            printf ("failed get content of json obeject\n");
            continue;
        } else {
            struct json_object *sizeobj, *typeobj, *textobj;
            sizeobj = json_object_object_get(content, "size");
            typeobj = json_object_object_get(content, "mimeType");
            textobj = json_object_object_get(content, "text");

            if ((sizeobj != NULL)
                    && (typeobj != NULL)
                    && (textobj != NULL)
                    && (json_object_get_type(sizeobj) == json_type_int)
                    && (json_object_get_type(typeobj) == json_type_string)
                    && (json_object_get_type(textobj) == json_type_string)) {
                if (strcmp(json_object_get_string(typeobj), "video/mp2t") == 0) {
                    if ((len = base64_decode(json_object_get_string(textobj), strlen(json_object_get_string(textobj)), buf, HAR_BUFFER_SIZE)) > 0) {
                        if (fwrite(header, 4, 1, out_fp) <= 0)
                        {
                            printf ("write ouput file failed\n");
                            break;
                        }
                        if (fwrite(buf, len, 1, out_fp) <= 0)
                        {
                            printf ("write ouput file failed\n");
                            break;
                        }
                        detect_box (buf, len);
                        break;
                    }
                }
                printf ("get response size:%d, type:%s, text size:%ld, len:%d\n",
                        json_object_get_int(sizeobj), json_object_get_string(typeobj), strlen(json_object_get_string(textobj)), len);
            }
        }
    }

failed:
    free (buf);
    if (hctx)
    {
        free_har_context(hctx);
    }
    fclose (out_fp);
    return ret;
}

int main(int argc, char **argv)
{
    char *input, *output;
    FILE *fp, *out_fp;

    if (argc < 2)
    {
        return usage (argv[0]);
    }

    input = argv[1];
    if (argc < 3)
    {
        output = "har.ts";
        printf ("using default output file:%s\n", output);
    }
    else
    {
        output = argv[2];
    }

    if ((fp = fopen(input, "rb")) == NULL)
    {
        return usage (argv[0]);
    }
    fclose(fp);

    if ((out_fp = fopen(output, "w+")) == NULL)
    {
        return usage (argv[0]);
    }

    return decode_har (input, out_fp);
}
