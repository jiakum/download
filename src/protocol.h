#ifndef _RECOVERY_H_
#define _RECOVERY_H_


/* all data is little endian */

#include <stdint.h>
#define RCPACKET_MAXPAYLOADSIZE (1024 + 256)

#define RCPACKET_HEAD_LEN   12
struct DwPacket {
    unsigned char header[2]; // "@$"
    uint16_t seq;
    uint16_t flags;
    uint16_t cmd;
    uint16_t len; //the length of payload
    unsigned char crc[2]; // header crc

    unsigned char payload[RCPACKET_MAXPAYLOADSIZE]; // len
}__attribute__((packed));

struct RCFileInfo {
    int32_t fileid;
    int32_t offset;
    int32_t len;
}__attribute__((packed));

#define RCPACKET_MAX_FILEDATA_LEN   (RCPACKET_MAXPAYLOADSIZE - sizeof(struct RCFileInfo))

#define RCFLAGS_COMMAND  (0x0 << 0)
#define RCFLAGS_RESPONSE (0x1 << 0)

/*
 * server is a fileserver who has file (as phone)
 * client is a user who downloads the file (as controller)
 * all command has response .
 * No response means packet missed or other reason .
 */
struct MachineInfo {
    char machineid[16];
    char ubootversion[16];
    char kernelversion[16];
    char rootfsversion[16];
    char rcdaemonversion[16]; // including rcdaemon, ui, rtsp switch, video player
}__attribute__((packed));

/*
 * command : RCCOMMAND_GET_MACHINEINFO
 * sent by server
 * no payload
 *
 * response payload: struct MachineInfo
 * sent by client
 * It is the server's responsibility to check machineid and check whether if the client's software is update-to-date.
 */
#define RCCOMMAND_GET_MACHINEINFO (0x0)

#define RCFILE_NAMES_MAX_LEN    1024
struct PackageInfo {
    struct MachineInfo machine;
    int32_t type;
    int32_t num;
    char filenames[RCFILE_NAMES_MAX_LEN];
}__attribute__((packed));

/*
 * command: RCCOMMAND_START_RECOVERY
 * sent by server
 * payload : struct PackageInfo
 * filenames is num of files's names joined by '\0'
 *
 * response payload : "OK" if machine info is suitable for client
 *   "FAILED" if machineid is wrong.
 *   ATTENTION!! Client will return "OK" and the recovery will go on , even if the version is older.
 *   "\0" is accepted for version but not for machineid.
 */
#define RCCOMMAND_START_RECOVERY (RCCOMMAND_GET_MACHINEINFO + 0x1)

/*
 * command : RCCOMMAND_END_RECOVERY
 * sent by client
 * payload : "OK" if recovery is success
 *   "FAILED" if failed
 *
 * response :
 * sent by server
 * no payload
 */
#define RCCOMMAND_END_RECOVERY (RCCOMMAND_GET_MACHINEINFO + 0x02)

/*
 * command : RCCOMMAND_REQUEST_NEWFILE
 * sent by client
 * command  payload : filename ended by '\0'
 *
 * response paylaod :struct RCFileInfo
 * sent by server
 * If file is not ready, fileid will be 0.
 * If fileid is bigger than 0, it indicates file is ready for transfer.
 * len is the total length of the file
 */
#define RCCOMMAND_REQUEST_NEWFILE (0x10)

/*
 * command : RCCOMMAND_TRANS_FILEDATA
 * sent by client
 * command payload : struct RCFileInfo
 * offset is the offset of the file
 * len is request length
 *
 * response payload : struct RCFileInfo + file raw data
 * sent by server
 */
#define RCCOMMAND_TRANS_FILEDATA (RCCOMMAND_REQUEST_NEWFILE + 0x01)

/*
 * command : RCCOMMAND_FILEDATA_MD5
 * payload :  struct RCFileInfo info
 * sent by client
 *   request MD5 for data that begins from info.offset and its length is info.len 
 *
 * response :  
 * sent by server
 * command payload : struct RCFileInfo + MD5.
 *
 */
#define RCCOMMAND_FILEDATA_MD5 (RCCOMMAND_REQUEST_NEWFILE + 0x02)

/*
 * command : RCCOMMAND_END_TRANSFILE
 * sent by client
 * command payload : "OK"
 *   "FIALED" indicates error occur
 *
 * response :
 * sent by server
 * no payload
 */
#define RCCOMMAND_END_TRANSFILE (RCCOMMAND_REQUEST_NEWFILE + 0x03)

/*
 * command: RCCOMMAND_RECOVERY_KEEPALIVE
 * sent by client
 * internal : 5s
 * no payload
 *
 * response:
 * sent by server
 * no payload
 */
#define RCCOMMAND_RECOVERY_KEEPALIVE (0x0f0)

/*
 * crc check function
 */
unsigned char calc_crc8(unsigned char *buf, int len);

#endif // _RECOVERY_H_
