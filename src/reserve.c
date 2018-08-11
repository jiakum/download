
void reverse_buffer(unsigned int *src, int size, unsigned int *dest)
{
    unsigned int temp, result;
    int i;

    for(i = 0;i < size;i++) {
        temp = src[i];

        asm("RBIT %0,%1\n\t"
            "REV  %0,%0"
                : "=r"(result) : "r"(temp));

        dest[i] = result;
    }
}

