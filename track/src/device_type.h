#ifndef _TRACK_DEVICE_TYPE_H__
#define _TRACK_DEVICE_TYPE_H__

#include <unordered_map>
#include <string>

enum {
    TRACK_DEVICE_TYPE_NONE = 0, 
    TRACK_DEVICE_TYPE_CARD, 
    TRACK_DEVICE_TYPE_BASE,
    TRACK_DEVICE_TYPE_QR,
    TRACK_DEVICE_TYPE_ARGUS2,
    TRACK_DEVICE_TYPE_GO,
    TRACK_DEVICE_TYPE_KEEN,
    TRACK_DEVICE_TYPE_IPC,
    TRACK_DEVICE_TYPE_NVR,
    TRACK_DEVICE_TYPE_DVR,
    TRACK_DEVICE_TYPE_ARGUSRPO,
};

struct device_type {
    char uid[24], suid[8];
    int status, sequence, type;
    int hwVersion, batteryType;
};


int parse_device_type(std::unordered_map<std::string, int> &lmap, const char *name, int type);


#endif