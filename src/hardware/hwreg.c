
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/mman.h>


static inline void usage(char *fun)
{
    printf("read reg: %s offset size\nwrite reg: %s offset=value\n", fun, fun);
    exit(0);
}

#define PAGE_SIZE (0x1000)

int main(int argc, char* argv[])
{
    unsigned int pfn, offset, size = 0, value = 0, map_len, opt;
    volatile unsigned int *ptr;
    int i, fd;
    void *addr;
    char *end;

    if(argc < 2)
        usage(argv[0]);

    offset = strtoul(argv[1], &end, 0);
    if((end == NULL) || (*end != '='))
    {
        if(argc >= 3)
            size = strtoul(argv[2], NULL, 0);

        if(size < 4)
            size = 4;
        else
            size = (size + 0x03) & ~0x03;

        opt = PROT_READ;
    }
    else 
    {
        end++;
        if(*end == '\0')
            usage(argv[0]);

        value = strtoul(end, NULL, 0);

        opt = PROT_READ | PROT_WRITE;
    }

    pfn = offset & ~(PAGE_SIZE - 1);
    map_len = (size + PAGE_SIZE * 2) & ~(PAGE_SIZE - 1);

    fd = open("/dev/mem", O_RDWR|O_SYNC);
    addr = mmap((void *)NULL, map_len, opt, MAP_SHARED, fd, pfn);

    if(addr == MAP_FAILED)
    {
        perror("map failed !!! ");
        usage(argv[0]);
    }

    offset &= (PAGE_SIZE - 1);
    ptr = (unsigned int *)((unsigned long)addr + offset);
    pfn  += offset;

    if(opt & PROT_WRITE)
    {
        *ptr = value;
        printf("write reg:0x%x result:0x%x\n", pfn, *ptr);
    }
    else
    {
        size /= 4;
        for(i = 0;i < size;i++)
        {
            if(!(i & 0x03))
                printf("\nread address 0x%x:", pfn + i * 4);
            printf("  0x%x,", *(ptr + i));
        }

        printf("\n");
    }

    munmap(addr, map_len);

    return 0;
}
