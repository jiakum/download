#ifndef _TRACK_OUTPUT_H__
#define _TRACK_OUTPUT_H__

#include "conn_log.h"



int output_conn_status_csv(std::unordered_map<std::string, struct conn_status> &result
        , const char *filename);


#endif