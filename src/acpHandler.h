/*
 * acpHandler.h  Application Communication Protocol
 *
 *  Created on: Sep 22, 2014
 *      Author: duyphan
 */

#ifndef ACPHANDLER_H_
#define ACPHANDLER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define ACP_SUCCESS		0
#define ACP_FAILED		-1
#define MAX_FILE_BUFF_LEN	10*1024
#define RECV_FILE_ENABLED	1
#define RECV_FILE_DISABLED 0
#define MAX_WAITING_COUNT 	3000
#define ENABLED		1
#define DISABLED	0
#define BUFF_LEN_MAX 2048
#define NUM_CONTENT_TAG 5
#define RESPONSE_SUCCESS	"SUCCESS"
#define RESPONSE_FAILED		"FAILED"
#define MSG_ID_DEFAULT		"000"
#define ROOM_NAME_DEFAULT			"room2"

/********************************************************
 * Define communication protocol session
 ********************************************************/
#define MAX_PACKAGE_LEN		1024
#define PACKAGE_HEADER 		0x55 //package header

//package type
#define PACKAGE_CONTROL 	0x01
#define PACKAGE_RESQUEST 	0x02
#define PACKAGE_CTRL_RESP  0x03
#define PACKAGE_REQ_RESP	0x04

//define command control
#define CMD_CTRL_PLAY_AUDIO 		0x00
#define CMD_CTRL_STOP_AUDIO			0x01
#define CMD_SEND_FILE				0x02
#define CMD_START_TRANFER_FILE		0x03
#define CMD_CTRL_EOF				0x04
#define CMD_CTRL_PAUSE_AUDIO 		0x05

//define request
#define CMD_REQ_GET_LIST_FILE		0x01
#define CMD_REQ_DISCOVER			0x02

//define control response
#define CTRL_RESP_FAILED           0x00
#define CTRL_RESP_SUCCESS			0x01
#define CTRL_RESP_ALREADY			0x02
#define CTRL_RESP_FILE_FINISH		0x03

//define request respone
#define REQ_RESP_GET_LIST			0X00
/******************************************************/



int g_RecvFileFlag;
int g_StartTransferFlag;
int g_waitCount;
int g_writeDataFlag;
char *g_remote_addr;
int multicast_fd;

typedef struct Ftp_Info{

	char Ip[24];
	char User[24];
	char Password[24];
}FtpInfo;
typedef struct Server_Info{
	char serverIp[24];
	FtpInfo ftp;
}ServerInfo;

/*typedef struct Xml_Element{
	char *ele_name;
	char *ele_content;
}XMLTag;

typedef struct Notify_Info{
	char *info;
	XMLTag *obj_ele;
	XMLTag *status_ele;
}NotifyPiStatus;*/

typedef struct Xml_Element{
	char *ele_name;
	char *ele_content;
}XMLTag;

typedef struct Notify_Info{
	char *info;
	int num_content_tag;
	XMLTag content_tag[NUM_CONTENT_TAG];
}NotifyPiStatus;


ServerInfo g_ServerInfo;

pthread_t g_TaskHandlerThread;


char *g_FileBuff;
typedef char byte;

typedef struct PackageContent{
	byte package_code;
	short int num_arg;
	byte *args;
}PackageContent;

typedef struct PackageData{
	byte header;
	byte type;
	short int length;
	PackageContent content;
}PackageData;


//Package function declaration
void parsePackageContent(char *packageBuff);
int wrapperControlResp(char resp);
int wrapperRequestResp(char *resp);
int ControlHandler(char *ctrlBuff, short int length);
int RequestHandler(char *);

void *waitingConnectionThread();
void recvnhandlePackageLoop();
static int isEOFPackage(char *);
int initFileHandlerThread();
int initAudioPlayer(char *);

void TaskReceiver();
int initTaskHandler(char *message);
int collectServerInfo(message);
void *TaskHandlerThread(void *arg);
void MessageProcessor(char *msg_buff);
int sendResultResponse(char *msg_id, char *resp_for,int resp_code, char *resp_content);
int sendPlayingStatusNotify(char *msg_id, char *file_name,int num_tag, char *status);

#endif /* ACP_H_ */




/*
 *
 * PLAYAUDIOFLAG=-D PLAY_AUDIO
.c.o: $(DEPS)
			gcc $(CFLAGS) -c -o $@  $<
*/
