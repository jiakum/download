#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define HAR_BUFFER_SIZE (1024 * 1024)

struct har_context {
    FILE *fp;
    char *buf, *start, *end;
    int len;
};

static struct har_context *init_har_context(FILE *fp)
{
    struct har_context *hctx = malloc(sizeof (*hctx));

    if (hctx == NULL)
    {
        return NULL;
    }

    hctx->buf = malloc(HAR_BUFFER_SIZE);
    if (hctx->buf == NULL)
    {
        free (hctx);
        return NULL;
    }

    hctx->fp    = fp;
    hctx->start = hctx->buf;
    hctx->end   = hctx->buf + HAR_BUFFER_SIZE;
    hctx->len   = 0;

    return hctx;
}

static int read_har_context (struct har_context *hctx)
{
    int len;

    if (hctx->len > 0)
    {
        memcpy(hctx->start, hctx->buf, hctx->len);
    }
    hctx->buf = hctx->start + hctx->len;

    len = fread(hctx->buf, 1, hctx->end - hctx->buf, hctx->fp);

    hctx->len += len;

    return len > 0 ? len: -1;
}

static int har_find_str (struct har_context *hctx, char *str)
{
    int len = strlen(str), ret = 0;

    if (len == 0)
    {
        return -1;
    }

    while (ret >= 0) 
    {
        if (hctx->len < len)
        {
            ret = read_har_context (hctx);
            continue;
        }

        if (strncmp (hctx->buf, str, len) == 0)
        {
            hctx->buf += len;
            hctx->len -= len;
            return 0; // string found
        }

        hctx->buf++;
        hctx->len--;
    }

    return ret;  // not found, ie: end of file
}

static int har_get_text (struct har_context *hctx, char *text, int max_size)
{
    const char *str = "\"text\": \"";
    char *start = text;
    int ret = 0, i, len = strlen(str);

    if (har_find_str (hctx, "\"response\":") != 0)
    {
        return -1;
    }

    if (har_find_str (hctx, "\"content\":") != 0)
    {
        return -1;
    }

    while (ret >= 0) 
    {
        if (hctx->len < len)
        {
            ret = read_har_context (hctx);
            continue;
        }

        if (*hctx->buf == '}')
        {
            break;
        }

        if (strncmp (hctx->buf, str, len) == 0)
        {
            hctx->buf += len;
            hctx->len -= len;
            goto found; // string found
        }

        hctx->buf++;
        hctx->len--;
    }

    return ret >= 0 ? 0: ret;

found:
    while (ret >= 0) 
    {
        if (hctx->len < 1)
        {
            ret = read_har_context (hctx);
            continue;
        }

        if (*hctx->buf == '\"')
        {
            return text - start; // end of text
        }

        if (--max_size <= 0)
        {
            printf ("text buffer size too small\n");
            break;
        }

        *text = *hctx->buf;
        text++;
        hctx->buf++;
        hctx->len--;
    }

    return -1;
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

static int base64_decode (char *base64, int input_size, char *output, int max_size)
{
    static const char table[] = {
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
        'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
        'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
        'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'
    };
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

static int decode_har(FILE *fp, FILE *out_fp)
{
    char *buf = malloc(HAR_BUFFER_SIZE), *src = malloc(HAR_BUFFER_SIZE);
    struct har_context *hctx;
    int ret = 0, len, i, skip = 1;

    if (buf == NULL)
    {
        ret = -1;
        goto failed;
    }

    hctx = init_har_context(fp);
    if (hctx == NULL)
    {
        goto failed;
    }

    while ((len = har_get_text(hctx, src, HAR_BUFFER_SIZE)) >= 0)
    {
        src[len] = 0;

        if (len > 0)
        {
            if ((len = base64_decode(src, len, buf, HAR_BUFFER_SIZE)) <= 0)
            {
                len = 0;
                continue;
            }

            if (--skip >= 0)
                continue;

            if (fwrite(buf, len, 1, out_fp) < 0)
            {
                goto failed;
            }
        }
    }

failed:
    free (buf);
    free (src);
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

    if ((out_fp = fopen(output, "w+")) == NULL)
    {
        return usage (argv[0]);
    }

    return decode_har (fp, out_fp);
}
