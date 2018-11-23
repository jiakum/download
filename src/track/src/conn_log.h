#ifndef _TRACK_CONN_LOG_H__
#define _TRACK_CONN_LOG_H__

#include <unordered_map>

struct conn_log {
    char uid[24];
    unsigned int dev_address, client_address;
    unsigned short dev_port, client_port;

    int type, state;
    unsigned int timeNat, timeQuery, timeSetup;
    unsigned int start, end, nat, retry;
    unsigned int rspcode, family, plat;    
};

struct conn_status {
    /* 
     * type:0 for local connection 
     * type:1 for p2p connection
     * type:2 for relay connection
     */
    int success[3]; 
    int failed;
};





int parse_conn_log(const char *filename, std::unordered_map<std::string, int> &uidmap,
        std::unordered_map<std::string, struct conn_status> &result);






#endif