#include "mediapull.h"
#include "rtsp/rtsp.h"
#include "g711dec.h"
#include "api/api_safe.h"
#include "rtp/rtp.h"
#include "initlock.h"
#include "AVLB/AVLB_def.h"
#include "initlock.h"
#include "g711encode .h"

#define BUF_SIZE 1500
#define RTSP_CLIENT_DEBUG_INFO AppDbg_Printf
#define RTSP_VER  "RTSP/1.0"

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
  int			fd;
  char		session[32];
  char		transport[32];
  int	client_state;
  unsigned int	cseq;
  char		outbuff[BUF_SIZE / 4];
  char		inbuff[BUF_SIZE];
} RTSP_CLI;

static RTSP_CLI avClient;
UINT8 PullStatus = STREAM_IDLE;
static char RTSP_SEV_ADDR[16];//	"139.196.221.163"
static int RTSP_SEV_PORT;//10700
static char RTSP_SEV_URL[128];//"rtsp://"RTSP_SEV_ADDR":"RTSP_SEV_PORT_STR"/test.sdp"

void adhereSSID(char* out, char* ssid);
int getSessionId(char *buf, char *dst);

static int sendTeardown(char *ssid);

static int rtspSendRequest(void) {
    int status;
    int len;

    if (avClient.outbuff == NULL)
    {
        return RTSP_CLIENT_FAILURE;
    }
    len = strlen(avClient.outbuff);
    status = send(avClient.fd, avClient.outbuff, len, 0);
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

    memset (avClient.inbuff, 0, BUF_SIZE);
    len = BUF_SIZE;
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
  memset (avClient.outbuff, 0, BUF_SIZE / 4);
	sprintf(avClient.outbuff, \
        "OPTIONS %s %s\r\n"
        "CSeq: %d\r\n"
        "User-Agent: Lavf57.71.100\r\n", \
        RTSP_SEV_URL, RTSP_VER, avClient.cseq);
  adhereSSID(avClient.outbuff, ssid);
  return rtspSendRequest();
}
static int rtspRequestDescribe(char *ssid) {
	memset (avClient.outbuff, 0, BUF_SIZE / 4);
	sprintf(avClient.outbuff, \
    "DESCRIBE %s %s\r\n"
    "CSeq: %d\r\n" 
    "User-Agent: Lavf57.71.100\r\n"
    "Accept: application/sdp\r\n", \
    RTSP_SEV_URL, RTSP_VER, avClient.cseq);
  adhereSSID(avClient.outbuff, ssid);
  return rtspSendRequest();
}
static int rtspRequestSetup(char *ssid) {
	memset (avClient.outbuff, 0, BUF_SIZE / 4);
  sprintf(avClient.outbuff, \
    "SETUP %s/trackID=0 %s\r\n"
    "CSeq: %d\r\n"
    "User-Agent: Lavf57.71.100\r\n"
    "Transport: RTP/AVP/TCP;interleaved=0-1;unicast;mode=play\r\n", \
    RTSP_SEV_URL, RTSP_VER, avClient.cseq);
  adhereSSID(avClient.outbuff, ssid);
  return rtspSendRequest();
}
static int rtspRequestPlay(char *ssid) {
	memset (avClient.outbuff, 0, BUF_SIZE / 4);
	sprintf(avClient.outbuff, \
    "PLAY %s %s\r\n"
    "CSeq: %d\r\n"
    "User-Agent: Lavf57.71.100\r\n"
    "Range: npt=0.000-\r\n", \
    RTSP_SEV_URL, RTSP_VER, avClient.cseq);
  adhereSSID(avClient.outbuff, ssid);
    return rtspSendRequest();
}
static int rtspRequestTeardown(char *ssid) {
	memset (avClient.outbuff, 0, BUF_SIZE / 4);
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

    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(RTSP_SEV_PORT);
    srv_addr.sin_addr.s_addr = inet_addr(RTSP_SEV_ADDR);

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
	if(avClient.client_state == RTSP_PLAYING)
	{
		RTSP_CLIENT_DEBUG_INFO ("\r\nclosing connection...\n");
		status = sendTeardown(ssid);
		if (status != RTSP_CLIENT_SUCCESS) {
			RTSP_CLIENT_DEBUG_INFO ("\r\nsend_teardown failed:%d\n", status);
		}
	}
  
	RTSP_CLIENT_DEBUG_INFO ("\r\nclosing connection...\n");
	memset (&avClient, 0, sizeof(RTSP_CLI));
  ADEC_Status(AD_Stop);
  clearSocket(&avClient.fd);
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
static int sendDescribe(char *ssid) {
    int status;
    status = rtspRequestDescribe(ssid);
    if (status != RTSP_CLIENT_SUCCESS)
    {
        RTSP_CLIENT_DEBUG_INFO ("\r\nrtsp_send_request describe failed:%d\n", status);
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
static int sendSetup(char *ssid) {
    int status;
    status = rtspRequestSetup(ssid);
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
static int sendPlay(char *ssid) {
    int status;

    status = rtspRequestPlay(ssid);
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
    else
    {
        avClient.client_state = RTSP_PLAYING;
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
static int initRtsp(void) {  
  memset (&avClient, 0, sizeof(RTSP_CLI));    
  avClient.fd = socket(AF_INET, SOCK_STREAM, 0);
  AppDbg_Printf ("\r\n audio fd:%d", avClient.fd);
  avClient.cseq = 1;  
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
  rtsp[0]->fd.talkback_fd = 0;  
  return 0;
}
void mediaPullTask(UINT32 arg) {
  int status;
  
  //getMediaInfo("rtsp://192.168.1.12:10700/audio.sdp");
  while(1) {
    if(isSTAready() == 0) {
        GsnTaskSleep(100);
    }
    else
      break;
  }
  
  GsnTaskSleep(100);
  while(1) {
    while(PullStatus != STREAM_RUNNING) {
      GsnTaskSleep(100);
    }
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
    
    status = sendDescribe(0);
    if (status != RTSP_CLIENT_SUCCESS) 
    {
      RTSP_CLIENT_DEBUG_INFO ("\r\nsend describe failed:%d\n", status);
      goto error;
    } 
    RTSP_CLIENT_DEBUG_INFO("\r\nsend describe success\n");
    
    //pull audio
    status = sendSetup(0);
    if (status != RTSP_CLIENT_SUCCESS) 
    {
      RTSP_CLIENT_DEBUG_INFO ("\r\nsend setup 0 failed:%d\n", status);
      goto error;
    } 
    
    status = getSessionId(avClient.inbuff, avClient.session);
    if (status != RTSP_CLIENT_SUCCESS) 
    {
      RTSP_CLIENT_DEBUG_INFO ("\r\nget_session_id failed:%d\n", status);
      goto error;
    }
    
    status = sendPlay(avClient.session);
    if (status != RTSP_CLIENT_SUCCESS) 
    {
      RTSP_CLIENT_DEBUG_INFO ("\r\nsend play failed:%d\n", status);
      goto error;
    } 
    RTSP_CLIENT_DEBUG_INFO("\r\nsend play success\n");
    
    rtsp[0]->fd.talkback_fd = avClient.fd;
    while(1){  
      GsnTaskSleep(100);
      if((rtsp[0]->fd.talkback_fd == 0) || (PullStatus != STREAM_RUNNING)) {
        break;
      }
    }
    error:
    AppDbg_Printf("pull stopped\r\n");
    deInitRtsp();    
    ADEC_Status(AD_Stop);
    PullStatus = STREAM_IDLE;     
	}
}
int getMediaInfo(UINT8 *url) {  
  memset(RTSP_SEV_URL, 0, 128);
  strcpy((char *)RTSP_SEV_URL, (const char *)url);
  
  //读取服务器IP地址
  char *ptrs = NULL, *ptre = NULL;
  ptrs = strstr((const char*)url, "rtsp://");
  if(ptrs == NULL)
    return -1;
  ptrs += strlen("rtsp://");
  
  ptre = strstr((const char*)ptrs, ":");
  if(ptre == NULL)
    return -1;
  
  memset(RTSP_SEV_ADDR, 0, sizeof(RTSP_SEV_ADDR));
  strncpy(RTSP_SEV_ADDR, ptrs, ptre - ptrs);
  
  //读取服务器端口
  UINT8 temp[8];
  memset(temp, 0, 8);
  ptrs = ptre + 1;
  ptre = strstr((const char*)ptrs, "/");
  if(ptre == NULL)
    return -1;
  strncpy((char *)temp, (char *)ptrs, ptre - ptrs);
  RTSP_SEV_PORT = atoi((char *)temp);
  return 0;
}
void talkBackTask(UINT32 arg) {
  static UINT8 audiobuf[640];
  
  UINT8 findhead = 1;
  UINT8 findtail = 0;
  UINT8 findlen = 0;
  
  UINT16 expectlen = 0;
  UINT16 expectskip = 0;
  UINT16 i = 0, j = 0;
  
  INT16 temp;
  UINT8 expecttemp = 2;
  
  int status = 0, data_len = 0;

  while(1) {
    while(rtsp[0]->fd.talkback_fd == 0) {
      GsnTaskSleep(100);
    }   
    status = Talk_Back(TLK_START);
    if (status != RTSP_CLIENT_SUCCESS)
    {
      RTSP_CLIENT_DEBUG_INFO ("\r\nTalk_Back:%d,%d\n", status, __LINE__);
      rtsp[0]->fd.talkback_fd = 0;
      continue ;
    }
    RTSP_CLIENT_DEBUG_INFO("\r\nTalk_Back start success\n");
    
    while(1) {
      data_len = recv(rtsp[0]->fd.talkback_fd, avClient.inbuff, BUF_SIZE, MSG_WAITALL);
      i = 0;
      //RTSP_CLIENT_DEBUG_INFO ("\r\nrecv data :%d\n", data_len);
      if (data_len <= 0)
      {
        RTSP_CLIENT_DEBUG_INFO ("\r\nrecv data failed:%d\n", data_len);
        goto error;
      }
      else {
        while(i < data_len) {
          while((findhead > 0) && (i < data_len)){
            if(avClient.inbuff[i] == 0x24) {
              i++;
              findhead--;
              findtail = 1;
            }
            else {
              i++;
              continue ;
            }
          }
          while((findtail > 0) && (i < data_len)){
            if(avClient.inbuff[i] == 0x00) {
              i++;
              findtail--;
              findlen = 2;
            }
            else {
              i++;
              findhead = 1;
              findtail = 0;
              break ;
            }
          }
          while((findlen > 0) && (i < data_len)){
            findlen--;
            expectlen = (expectlen << 8) | avClient.inbuff[i];            
            i++;  
            if(findlen == 0) {
              expectlen -= 12;
              expectskip = 12;         
            }
          }
          while((expectskip > 0) && (i < data_len)) {
            i++;
            expectskip--;
          }
          while((expectlen > 0) && (i < data_len)) {
            expectlen--;            
            expecttemp--;
            if(expecttemp == 1){
              temp = avClient.inbuff[i++];
            }
            else if(expecttemp == 0) {
              temp = temp | (avClient.inbuff[i++] << 8);
              expecttemp = 2;
              audiobuf[j++] = linear2ulaw(temp);
              if(j >= 640) {
                j = 0;
                //RTSP_CLIENT_DEBUG_INFO ("\r\nplay packet\n");
                status = Send_AStream ((UINT8 *)audiobuf, 640);
                if (status != RTSP_CLIENT_SUCCESS) 
                {
                  RTSP_CLIENT_DEBUG_INFO ("\r\nTalk_Back:%d,%d\n", status);
                  goto error;
                }
              }
            }          
          }
          if((expectlen + findhead + findtail + findlen + expectskip) == 0) {
            findhead = 1;
            findtail = 1;
            findlen = 2;
            expectlen = 0;
            expectskip = 0;
            expecttemp = 2;
          }
        }
      }
    }
    error:
      findhead = 1;
      findtail = 1;
      findlen = 2;
      expectlen = 0;
      expectskip = 0;
      expecttemp = 2;
      rtsp[0]->fd.talkback_fd = 0; 
    }    
}
void stopPull(void) {
  rtsp[0]->fd.talkback_fd = 0;
  AppDbg_Printf("PullStatus:%d\r\n", PullStatus);   
  if(PullStatus == STREAM_RUNNING) { 
    //clearSocket(&avClient.fd);
    PullStatus = STREAM_STOP;
  } else if(PullStatus == STREAM_STOP) {
    clearSocket(&avClient.fd);
  }
}