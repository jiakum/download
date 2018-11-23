
#include "device_type.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

using namespace std;


#define MAX_BUF_SIZE (16 * 1024)
int parse_device_type(unordered_map<string, int> &lmap, const char *name, int type) 
{
    FILE *fp = fopen(name, "rb");
    char *buf = (char *)malloc(MAX_BUF_SIZE), *pos;

    if(fp == NULL || buf == NULL) {
        goto failed;
    }

	while(fgets(buf, MAX_BUF_SIZE, fp) != NULL)
	{
		struct device_type dt;
		if((pos = strstr(buf,"\"uid\"")) != NULL) {
			pos = pos + 7;
			memcpy(dt.uid,pos,16);
            dt.uid[16] = '\0';
		} else {
            continue;
		}
        
		if((pos = strstr(buf,"\"type\"")) != NULL) {
			pos = pos + 7;
			dt.type = strtol(pos, NULL, 0);
			if(dt.type > 10)
				dt.type = 0;
		} else {
            continue;
		}
        
        if(dt.type != type)
        {
            continue;
        }

		lmap[string(dt.uid)] = 1;
	}

    fclose(fp);
    free(buf);

    return 0;
    
failed:
    printf("parse device type:%s failed:%s\n", name, strerror(errno));
    
    if(fp) fclose(fp);
    if(buf) free(buf);
    return -1;
}
