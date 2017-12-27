#include "mediapush.h"
#include "rtsp/rtsp.h"
#include "g711dec.h"
#include "api/api_safe.h"
#include "rtp/rtp.h"
#include "initlock.h"
#include "AVLB/AVLB_def.h"
#include "include.h"
#include "lockthread.h"

#define RTSP_CLIENT_DEBUG_INFO AppDbg_Printf
#define RTSP_VER  "RTSP/1.0"
#define BUF_SIZE 1536

#define TLK_START   1
#define TLK_STOP    0

typedef enum __myRTSP_CLIENT_STATE
{
    RTSP_CLIENT_FAILURE=-1,
    RTSP_CLIENT_SUCCESS=0,
    RTSP_TIMEOUT=1,
    RTSP_CONNECTED=1,
    RTSP_PLAYING,
    RTSP_DISCONNECTED=4,
} RTSP_CLI_STATE;

typedef struct _myrtsp_s
{
	int			a_fd;
  char		*host;
  char		*path;
  int			port;
  int			fd;
  char		vtrackid[16];
  char		atrackid[16];
  char		transport[32];
  char		session[32];
  int	client_state;
  unsigned int	cseq;
  int video_enable;
  int audio_enable;
  char		outbuff[BUF_SIZE];
  char		inbuff[BUF_SIZE / 4];
	BOOL Timeout_Flag;
} RTSP_CLI;

RTSP_CLI avClient;
UINT8 PushStatus = STREAM_IDLE;
char AudioUrl[64];

char AnnounceDetial[512];

char RTSP_SEV_ADDR[16];//	"139.196.221.163"
int RTSP_SEV_PORT;//10700
char RTSP_SEV_PORT_STR[10];//"10700"
char RTSP_SEV_URL[128];//"rtsp://"RTSP_SEV_ADDR":"RTSP_SEV_PORT_STR"/test.sdp"

static int rtspSendRequest(void);
static int getStatusCode(void);
static int rtspRecv(void);
static int rtspGetResponse(void);
static int rtspRequestSetup(UINT8 choice, char *ssid);
static int rtspRequestTeardown(char *ssid);
static int rtspConnect(void);
static void rtspClose(char *ssid);
static int sendOptions(char *ssid);
static int sendSetup(UINT8 choice, char *ssid) ;
static int sendTeardown(char *ssid);

int getSessionId(char *buf, char *dst);
void adhereSSID(char* out, char* ssid);

static int rtspSendRequest(void) {
    int status;
    int len;

    if (avClient.outbuff == NULL)
    {
        return RTSP_CLIENT_FAILURE;
    }
    len = strlen(avClient.outbuff);
    GsnOsal_SemAcquire(&SocketSyncSem, GSN_OSAL_WAIT_FOREVER);
    status = send(avClient.fd, avClient.outbuff, len, 0);
    GsnOsal_SemRelease(&SocketSyncSem);
    if (status != len)
    {
        RTSP_CLIENT_DEBUG_INFO("\r\nsend failed:%d\n",status);
    }
    //RTSP_CLIENT_DEBUG_INFO ("\r\nsent %d Bytes.\n", status);
    RTSP_CLIENT_DEBUG_INFO("\r\n\r\n%s\r\n\r\n",avClient.outbuff);
    return RTSP_CLIENT_SUCCESS;
}
static int getStatusCode(void) {
    int code;
    char tmp[4];

    if (!strncmp (avClient.inbuff, RTSP_VER, sizeof(RTSP_VER) -1))
    {
        memcpy (tmp, avClient.inbuff + sizeof(RTSP_VER), 3);
        tmp[3] = 0;
        code = atoi(tmp);
    }
    if (code != 200)
    {
        RTSP_CLIENT_DEBUG_INFO("\r\nInvalid status code:%d\n", code);
    }
    return code;
}
static int rtspRecv(void) {
    int status;
    int len;
    memset(avClient.inbuff, 0, BUF_SIZE / 4);
    len = BUF_SIZE / 4;
    status = recv(avClient.fd, avClient.inbuff, len, 0);
    if (status <= 0)
    {
        RTSP_CLIENT_DEBUG_INFO("\r\nrecv failed:%d\n",status);
        return status;
    }
    RTSP_CLIENT_DEBUG_INFO ("\r\nreceived %d Bytes.\n", status);
    RTSP_CLIENT_DEBUG_INFO("\r\n\r\n%s\r\n\r\n",avClient.inbuff);
    return RTSP_CLIENT_SUCCESS;
}
int getSessionId(char *buf, char *dst) {
    char *tmp = NULL, *ptr = NULL;
    ptr = strstr((const char *)buf, "Session: ");
    if (ptr != NULL)
    {
        ptr += strlen("Session: ");
        ptr = strtok(ptr, "\r\n");
        tmp = strtok(ptr, ";");
        strcpy (dst, tmp);
        RTSP_CLIENT_DEBUG_INFO ("\r\nsession: %s\n", avClient.session);
    }
    else
    {
        RTSP_CLIENT_DEBUG_INFO ("\r\nsession id not found.\n");
        return RTSP_CLIENT_FAILURE;
    }
    return RTSP_CLIENT_SUCCESS;
}
static int rtspGetResponse(void) {
    int status;
    status = rtspRecv();
    if (status != RTSP_CLIENT_SUCCESS)
    {
        RTSP_CLIENT_DEBUG_INFO("\r\nrtsp_recv failed:%d\n", status);
    }
    else
    {
        status = getStatusCode();
        if (status != 200)
        {
            RTSP_CLIENT_DEBUG_INFO ("\r\nserver status failed:%d\n", status);
        }
        else
        {
            RTSP_CLIENT_DEBUG_INFO ("\r\nserver status:%d OK\n", status);
            status = RTSP_CLIENT_SUCCESS;
        }
        avClient.cseq++;
    }
    return status;
}
static int rtspRequestOptions(char* ssid) {
  memset (avClient.outbuff, 0, BUF_SIZE);
	sprintf(avClient.outbuff, \
        "OPTIONS %s %s\r\n"
        "CSeq: %d\r\n"
        "User-Agent: Lavf57.71.100\r\n", \
        RTSP_SEV_URL, RTSP_VER, avClient.cseq);
  adhereSSID(avClient.outbuff, ssid);
  return rtspSendRequest();
}
static int rtspRequestSetup(UINT8 choice, char *ssid) {
	memset (avClient.outbuff, 0, BUF_SIZE);
  if(choice == 0) {
    sprintf(avClient.outbuff, \
      "SETUP %s/streamid=0 %s\r\n"
      "CSeq: %d\r\n"
      "User-Agent: Lavf57.71.100\r\n"
      "Transport: RTP/AVP;unicast;client_port=20000-20001;mode=record\r\n", \
      RTSP_SEV_URL, RTSP_VER, avClient.cseq);
  } else if(choice == 1) {
    sprintf(avClient.outbuff, \
      "SETUP %s/streamid=1 %s\r\n"
      "CSeq: %d\r\n"
      "User-Agent: Lavf57.71.100\r\n"
      "Transport: RTP/AVP;unicast;client_port=30000-30001;mode=record\r\n", \
      RTSP_SEV_URL, RTSP_VER, avClient.cseq);
  } 
  adhereSSID(avClient.outbuff, ssid);
  return rtspSendRequest();
}
static int rtspRequestAnnounce(void) {
  memset (avClient.outbuff, 0, BUF_SIZE);
	sprintf(avClient.outbuff, \
    "ANNOUNCE %s %s\r\n"
    "Content-Type: application/sdp\r\n"
    "CSeq: %d\r\n"
    "User-Agent: Lavf57.71.100\r\n"
    "Content-Length: %d\r\n"
    "\r\n"
    "%s", \
    RTSP_SEV_URL, RTSP_VER, avClient.cseq, strlen(AnnounceDetial), AnnounceDetial);
    return rtspSendRequest();
}
static int rtspRequestRecord(char *ssid) {
  memset (avClient.outbuff, 0, BUF_SIZE);
	sprintf(avClient.outbuff, \
        "RECORD %s %s\r\n"
        "CSeq: %d\r\n"
        "Range: npt=0.000- \r\n"
        "User-Agent: Lavf57.71.100\r\n", \
        RTSP_SEV_URL, RTSP_VER, avClient.cseq);
  adhereSSID(avClient.outbuff, ssid);
  return rtspSendRequest();
}
static int rtspRequestTeardown(char *ssid) {
	memset (avClient.outbuff, 0, BUF_SIZE);
	sprintf(avClient.outbuff, \
    "TEARDOWN %s %s\r\n"
    "CSeq: %d\r\n"
    "User-Agent: Lavf57.71.100\r\n", \
    RTSP_SEV_URL, RTSP_VER, avClient.cseq);  
  adhereSSID(avClient.outbuff, ssid);
    return rtspSendRequest();
}
static int rtspConnect(void) {
    struct sockaddr_in srv_addr;
    int status;   

    avClient.port= RTSP_SEV_PORT;
    avClient.host = RTSP_SEV_ADDR;
    avClient.path = "/test.sdp";

    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(avClient.port);
    srv_addr.sin_addr.s_addr = inet_addr(avClient.host);

    status  = connect(avClient.fd,(struct sockaddr *)&srv_addr,sizeof(srv_addr));

    if (status != RTSP_CLIENT_SUCCESS)
    {
        AppDbg_Printf("\r\nconnect failed:%d\n", status);
        soc_close(avClient.fd);
        return status;
    }
    else
    {
        AppDbg_Printf ("\r\nconnected to server.\n");
        avClient.client_state = RTSP_CONNECTED;
    }

    return status;
}
static void rtspClose(char *ssid) {
	
  int status;
	if( rtsp[0]->is_runing == STREAM_RUNNING)
	{
		status = sendTeardown(ssid);
		if (status != RTSP_CLIENT_SUCCESS) {
			RTSP_CLIENT_DEBUG_INFO ("\r\nsend_teardown failed:%d\n", status);
		}
	}
  
	RTSP_CLIENT_DEBUG_INFO ("\r\nclosing connection...\n");
  memset (&avClient, 0, sizeof(RTSP_CLI));
  avClient.Timeout_Flag = FALSE;
  //clearSocket(&avClient.fd);
}
static int sendOptions(char *ssid) {
    int status;
    status = rtspRequestOptions (ssid);
    if (status != RTSP_CLIENT_SUCCESS)
    {
        RTSP_CLIENT_DEBUG_INFO ("\r\nrtsp_send_request options failed:%d\n", status);
        return status;
    }
    else
    {
        RTSP_CLIENT_DEBUG_INFO ("\r\nDone!.\n");
    }
    status = rtspGetResponse();
    if (status != RTSP_CLIENT_SUCCESS)
    {
        RTSP_CLIENT_DEBUG_INFO ("\r\nteardown response get failed:%d\n", status);
        return status;
    }    
    return status;
}
static int sendSetup(UINT8 choice, char *ssid) {
    int status;
    status = rtspRequestSetup(choice, ssid);
    if (status != RTSP_CLIENT_SUCCESS)
    {
        RTSP_CLIENT_DEBUG_INFO ("\r\nrtsp_send_request setup failed:%d\n", status);
        return status;
    }
    else
    {
        RTSP_CLIENT_DEBUG_INFO ("\r\nDone!.\n");
    }
    status = rtspGetResponse();
    if (status != RTSP_CLIENT_SUCCESS)
    {
        RTSP_CLIENT_DEBUG_INFO ("\r\nteardown response get failed:%d\n", status);
        return status;
    }
    return status;
}
static int sendAnnounce(void) {
    int status;
    status = rtspRequestAnnounce();
    if (status != RTSP_CLIENT_SUCCESS)
    {
        RTSP_CLIENT_DEBUG_INFO ("\r\nrtsp_send_request play failed:%d\n", status);
        return status;
    }
    else
    {
        RTSP_CLIENT_DEBUG_INFO ("\r\nDone!.\n");
    }

    status = rtspGetResponse();
    if (status != RTSP_CLIENT_SUCCESS)
    {
        RTSP_CLIENT_DEBUG_INFO ("\r\nplay response get failed:%d\n", status);
        return status;
    }
    return status;
}
static int sendRecord(char *ssid) { 
  int status;
    status = rtspRequestRecord(ssid);
    if (status != RTSP_CLIENT_SUCCESS)
    {
        RTSP_CLIENT_DEBUG_INFO ("\r\nrtsp_send_request options failed:%d\n", status);
        return status;
    }
    else
    {
        RTSP_CLIENT_DEBUG_INFO ("\r\nDone!.\n");
    }
    status = rtspGetResponse();
    if (status != RTSP_CLIENT_SUCCESS)
    {
        RTSP_CLIENT_DEBUG_INFO ("\r\nteardown response get failed:%d\n", status);
        return status;
    }    
    return status;
}
static int sendTeardown(char *ssid) {
    int status;
    status = rtspRequestTeardown(ssid);
    if (status != RTSP_CLIENT_SUCCESS)
    {
        RTSP_CLIENT_DEBUG_INFO ("\r\nrtsp_send_request tearrdown failed:%d\n", status);
        return status;
    }
    else
    {
        RTSP_CLIENT_DEBUG_INFO ("\r\nDone!.\n");
    }
    status = rtspGetResponse();
    if (status != RTSP_CLIENT_SUCCESS)
    {
        RTSP_CLIENT_DEBUG_INFO ("\r\nteardown response get failed:%d\n", status);
        return status;
    }
    else
    {
        avClient.client_state = RTSP_DISCONNECTED;
    }
    return status;
}
int getPort(char *buf) {
    char tmp[6], *ptr = NULL,*ptrend = NULL;
    memset(tmp, 0, 6);
    ptr = strstr((const char *)buf, "server_port=");
    if (ptr != NULL)
    {
        ptr += strlen("server_port=");
        ptrend = strstr((const char *)ptr, "-");
        strncpy(tmp, ptr, ptrend - ptr);
        RTSP_CLIENT_DEBUG_INFO ("\r\nport: %s\n", tmp);
        return atoi(tmp);
    }
    else
    {
        RTSP_CLIENT_DEBUG_INFO ("\r\nsession id not found.\n");
        return 0;
    }
    return 0;
}
static int initRtsp(void) {
  memset(rtsp[0], 0, sizeof(struct rtsp_buffer));
  memset (&avClient, 0, sizeof(RTSP_CLI));
  
  rtsp[0]->cli_rtp.video_seq_num = 1;
  rtsp[0]->cli_rtp.audio_seq_num = 1;
  rtsp[0]->is_runing = STREAM_IDLE;
  strncpy(rtsp[0]->cli_rtsp.cli_host,RTSP_SEV_ADDR, 16);
  rtsp[0]->cmd_port.seq=get_random_seq();
  rtsp[0]->cmd_port.ssrc = RTSP_SSRC;
  rtsp[0]->cmd_port.timestamp=0; 
  
  avClient.Timeout_Flag = FALSE;
  avClient.cseq = 1;
  avClient.fd = socket(AF_INET, SOCK_STREAM, 0);
  AppDbg_Printf ("\r\n video fd:%d", avClient.fd);  
  if (avClient.fd < 0)
  {
      soc_close(avClient.fd);
      avClient.fd = 0;      
      AppDbg_Printf ("\r\n rtsp socket failed");
      return -1;
  }
  
  return 0;
}
static int deInitRtsp(void) {
  clearSocket(&avClient.fd);
  memset (&avClient, 0, sizeof(RTSP_CLI));  
  avClient.Timeout_Flag = FALSE;
  clearSocket(&rtsp[0]->fd.audio_rtp_fd);
  clearSocket(&rtsp[0]->fd.video_rtp_fd);
  rtsp[0]->is_runing = STREAM_IDLE;  
  return 0;
}
void mediaPushTask(UINT32 arg) {
	int status;
  int timeout = 0;
  UINT8 buf[5];
  //fillString("139.196.221.163", 10700, "test");
  //fillString("192.168.1.12", 10700, 12652578); 
  while(1) {
    if(isSTAready() == 0) {
        GsnTaskSleep(100);
    }
    else
      break;
  }
  
  while(strlen((const char*)RTSP_SEV_ADDR) == 0) {
    GsnTaskSleep(200);
  }
  
  GsnTaskSleep(100);
  while(1) {    
    while(PushStatus != STREAM_RUNNING) {
      GsnTaskSleep(100);
    } 
    AppDbg_Printf("mediaPushTask start\r\n"); 
    if(initRtsp() < 0) {
      RTSP_CLIENT_DEBUG_INFO("init rtsp fail\r\n");
      goto error;
    }
    status = rtspConnect();
    if (status != RTSP_CLIENT_SUCCESS) 
    {
      RTSP_CLIENT_DEBUG_INFO ("\r\nrtsp_connect failed:%d\n", status);    
      goto error;
    } 
    
    status = sendOptions(0);
    if (status != RTSP_CLIENT_SUCCESS) 
    {
      RTSP_CLIENT_DEBUG_INFO ("\r\nsend options failed:%d\n", status);
      goto error;
    } 
    RTSP_CLIENT_DEBUG_INFO("\r\nsend option success\n");
      
    status = sendAnnounce();
    if (status != RTSP_CLIENT_SUCCESS) 
    {
      RTSP_CLIENT_DEBUG_INFO ("\r\nsend announce failed:%d\n", status);
      goto error;
    }
    RTSP_CLIENT_DEBUG_INFO("\r\nsend announce success\n");  

    status = sendSetup(0, 0);
    if (status != RTSP_CLIENT_SUCCESS) 
    {
      RTSP_CLIENT_DEBUG_INFO ("\r\nsend setup 0 failed:%d\n", status);
      goto error;
    } 
    RTSP_CLIENT_DEBUG_INFO("\r\nsend setup 0 success\n");
    
    rtsp[0]->cmd_port.video_rtp_cli_port = getPort(avClient.inbuff);
    
    status = getSessionId(avClient.inbuff, avClient.session);
    if (status != RTSP_CLIENT_SUCCESS) 
    {
      RTSP_CLIENT_DEBUG_INFO ("\r\nget_session_id failed:%d\n", status);
      goto error;
    }
    
    status = sendSetup(1, avClient.session);    
    if (status != RTSP_CLIENT_SUCCESS) 
    {
      RTSP_CLIENT_DEBUG_INFO ("\r\nsend setup 1 failed:%d\n", status);
      goto error;
    }
    RTSP_CLIENT_DEBUG_INFO("\r\nsend setup 1 success\n");
    rtsp[0]->cmd_port.audio_rtp_cli_port = getPort(avClient.inbuff);
    
    status = sendRecord(avClient.session);
    if (status != RTSP_CLIENT_SUCCESS) 
    {
      RTSP_CLIENT_DEBUG_INFO ("\r\nsend record failed:%d\n", status);
      goto error;
    } 
    RTSP_CLIENT_DEBUG_INFO("\r\nsend record success\n");
    
    RTSP_CLIENT_DEBUG_INFO ("\r\nstart push media\n");    
    //如果正在发送图片，则等图片上传完成后在发送音视频
    while(timeout++ < 10) {
      if(LockPara.net.videostatus & 0x04 == 0)
        break;
      else 
        GsnTaskSleep(100);
    }
    
    rtsp[0]->fd.audio_rtp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    rtsp[0]->fd.video_rtp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    AppDbg_Printf("push fd:%d,%d\r\n", rtsp[0]->fd.video_rtp_fd, rtsp[0]->fd.audio_rtp_fd);
    rtsp[0]->is_runing = STREAM_RUNNING;
    
    GsnTaskSleep(100);
    fillL2NBuf(PUSH_MEDIA, 0, 0, NULL, 0, 100);
    GsnTaskSleep(100);
    if(LockPara.net.videostatus & 0x01) {
      fillL2NBuf(RECORD_CTRL, 0, 0, NULL, 0, 100);
    }
    while(1){
      GsnTaskSleep(100);
      if((PushStatus != STREAM_RUNNING) || (rtsp[0]->fd.audio_rtp_fd == 0) || (rtsp[0]->fd.video_rtp_fd == 0)) {
        break;
      }       
    }     
    error:
    AppDbg_Printf("push stopped\r\n");
    deInitRtsp();
    if(LockPara.net.videostatus & 0x01) {
      LockPara.net.videostatus &= (~0x01);
      fillL2NBuf(RECORD_CTRL, 1, 0, NULL, 0, 100);
    }
    if(LockPara.net.videostatus & 0x02)
      LockPara.net.videostatus &= (~0x02);
    fillL2NBuf(STOP_MEDIA, 0, 0, NULL, 0, 100);
    PushStatus = STREAM_IDLE;
	}
}
void adhereSSID(char* out, char* ssid) {
  if(ssid != NULL) {
    strcat(out, "Session: ");
    strcat(out, ssid);
    strcat(out, "\r\n");
  }
  strcat(out, "\r\n");
}
void fillString(char* addr, int port, UINT32 time) {
  char sdp[32];
  memset(sdp, 0, 32);
  
  sprintf(sdp, "%s%d.sdp", LockPara.net.uuid, time);
  
  memset(RTSP_SEV_ADDR, 0, sizeof(RTSP_SEV_ADDR));
  strncpy(RTSP_SEV_ADDR, addr, 15);
  
  RTSP_SEV_PORT = port;
  sprintf(RTSP_SEV_PORT_STR,"%d",port);
  
  memset(RTSP_SEV_URL, 0, sizeof(RTSP_SEV_URL));//"rtsp://"RTSP_SEV_ADDR":"RTSP_SEV_PORT_STR"/test.sdp"
  strcat(RTSP_SEV_URL, "rtsp://");
  strcat(RTSP_SEV_URL, RTSP_SEV_ADDR);
  strcat(RTSP_SEV_URL, ":");
  strcat(RTSP_SEV_URL, RTSP_SEV_PORT_STR);
  strcat(RTSP_SEV_URL, "/");
  strcat(RTSP_SEV_URL, sdp);
  
  memset(AnnounceDetial, 0, sizeof(AnnounceDetial));
  strcat(AnnounceDetial, "v=0\r\n");
  strcat(AnnounceDetial, "s=No Name\r\n"); 
  strcat(AnnounceDetial, "o=- 0 0 IN IP4 127.0.0.1\r\n");
  strcat(AnnounceDetial, "c=IN IP4 ");
  strcat(AnnounceDetial, RTSP_SEV_ADDR);
  strcat(AnnounceDetial, "\r\n");
  strcat(AnnounceDetial, "t=0 0\r\n");
  strcat(AnnounceDetial, "m=video 0 RTP/AVP 96\r\n");
  strcat(AnnounceDetial, "a=rtpmap:96 H264/90000\r\n");
  strcat(AnnounceDetial, "a=fmtp:96 packetization-mode=1;profile-level-id=420028;");
  strcat(AnnounceDetial, "sprop-parameter-sets=Z0IAKOkBQHsg,aM44gA==\r\n");
  strcat(AnnounceDetial, "b=AS:500\r\n");
  strcat(AnnounceDetial, "a=framerate:15\r\n");
  strcat(AnnounceDetial, "a=control:streamid=0\r\n");
  strcat(AnnounceDetial, "m=audio 0 RTP/AVP 98\r\n");
  strcat(AnnounceDetial, "a=rtpmap:98 L16/8000\r\n");
  //strcat(AnnounceDetial, "b=AS:128\r\n");
  strcat(AnnounceDetial, "a=control:streamid=1\r\n");
}
void stopPush(void) {  
  AppDbg_Printf("PushStatus:%d\r\n", PushStatus);   
  if(PushStatus == STREAM_RUNNING) {
    clearSocket(&rtsp[0]->fd.audio_rtp_fd);
    clearSocket(&rtsp[0]->fd.video_rtp_fd);
    //clearSocket(&avClient.fd);
    PushStatus = STREAM_STOP;
  } else if(PushStatus == STREAM_STOP) {
    clearSocket(&avClient.fd);
  }
}