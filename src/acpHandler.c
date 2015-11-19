/*
 * acpHandler.c Application Communication Protocol
 *
 *  Created on: Sep 22, 2014
 *      Author: duyphan
 */

#include "acpHandler.h"
#include "logger.h"
#include "sock_infra.h"
#include "FileHandler.h"
#include "playAudio.h"
#include "xmlHandler.h"
#include "gpio.h"
#include <endian.h>
#include <strings.h>
#include <unistd.h>


/*Task commands definition*/
struct RequestCmd {
	/*command*/
	const char *cmd_str;
	/* command index*/
	int cmd_index;
	/*number of arguments*/
	int num_args;
	/*arguments*/
	char *args;

} ReqestCmd;

enum request_cmd_index {

	GET_PI_INFO = 1,
	PI_CONNECT_SERVER,
	GET_FILE,
	PLAY_AUDIO,
	STOP_AUDIO,
	PAUSE_AUDIO,
	CHANGE_DEVICE_NAME,
	DELETE_FILE,
	SET_BINARY,
	GET_BINARY,
	CLOSE_TASK_HANDLER
};

static struct RequestCmd RequestCmdList[] = {
		{ "GetPiInfo", GET_PI_INFO, 1, "" },
		{ "PiConnectServer",PI_CONNECT_SERVER, 1, "server_ip" }, 
		{ "GetFile", GET_FILE, 2,"list_file" },
		{ "PlayFile", PLAY_AUDIO, 1, "file_name" },
		{ "StopFile", STOP_AUDIO, 1, "file_name" }, 
		{ "PauseFile",PAUSE_AUDIO, 1, "file_name" }, 
		{ "ChangeRoomName",	CHANGE_DEVICE_NAME, 1, "new_name" },
		{ "SetBinary",	SET_BINARY, 1, "value" },
		{ "GetBinary",	GET_BINARY, 0, "" },
		{ "DeleteFile",	DELETE_FILE, 1, "file_name" } 
};

struct NotifyInfo {
	const char *info_str;
	int info_index;

} NotifyInfo;

enum notify_info_index {
	SERVER_INFO = 1, FTP_ADDR
};
static struct NotifyInfo InfoList[] = { { "server info", SERVER_INFO }, {
		"ftpaddr", FTP_ADDR } };

ServerInfo server_info;

pthread_mutex_t g_file_buff_mutex;

int stream_sock_sd;

int NotifyMessageHandler(char *message);
int RequestMessageHandler(char *message);
int ResponseMessageHandler(char *message);
int changeRoomName(char *message);
int playAudio(char *message);
int playMedia(char *message);
int getNotifyIndex(char *info);
int getRequestCommandIndex(char *command);
int connectStation(char *station_addr);
int notifyDeviceInfo(int num_tag, char *status);
void closePlayerfifo(char *file);
void setSignalHandlers(void);
static void handleSigSegv(int signum, siginfo_t *pInfo, void *pVoid);
int isRequestMessageValid(char *msg_id, char *device_id, char *cmd, char *arg,
		char *arg_name);
int MediaPlayerUtil(char *request);
void setRelayBinary(char *message);
void getRelayBinary(char *message);

/*
 pthread_mutex_t g_file_buff_mutex_2 = PTHREAD_MUTEX_INITIALIZER;
 pthread_cond_t	g_file_thread_cond = PTHREAD_COND_INITIALIZER;
 pthread_cond_t	g_file_thread_cond_2 = PTHREAD_COND_INITIALIZER;*/

/*******************************************************
 * This thread is responsible for receive and accept
 * connection from other side application then fork new
 * process for each connection.
 *******************************************************/
void *waitingConnectionThread() {

	int ret;

	getInterfaceAddress();
	ret = openStreamSocket();
	if (ret == SOCK_SUCCESS) {
		appLog(LOG_DEBUG, "TCP socket successfully opened--------\n");
		appLog(LOG_DEBUG, "waiting for new connection!\n");
	} else {
		appLog(LOG_DEBUG, "TCP socket open failed\n");
	}

	/*start UDP socket for advertise server info*/
	appLog(LOG_DEBUG, "<call sem_post> to active init UDP sock\n");
	sem_post(&sem_sock);

	while (1) {
		child_stream_sock_fd = accept(stream_sock_fd,
				(struct sockaddr *) &remote_addr, &socklen);
		if (child_stream_sock_fd < 0) {
			appLog(LOG_ERR, "accept call error\n");
			exit(0);
			continue;
		}
		g_remote_addr = inet_ntoa(remote_addr.sin_addr);
		appLog(LOG_INFO, "server: got connection from %s\n", g_remote_addr);
		pid_t pid = fork();
		switch (pid) {
		case 0:
			recvnhandlePackageLoop();
			break;
		default:
			appLog(LOG_DEBUG, "parent process runing from here\n");
			break;
		}
	}
}

/*******************************************************
 * Process function used to waiting - receiving and handling
 * the received package
 *******************************************************/
void recvnhandlePackageLoop() {

	int ret;
	char *PackBuff;

	g_RecvFileFlag = 0;

	PackBuff = calloc(MAX_PACKAGE_LEN, sizeof(char));
	g_FileBuff = calloc(MAX_FILE_BUFF_LEN, sizeof(char));

	memset(PackBuff, 0x00, MAX_PACKAGE_LEN);
	memset(g_FileBuff, 0x00, MAX_FILE_BUFF_LEN);
	appLog(LOG_DEBUG, "PackBuff addr: %p", PackBuff);
	pthread_mutex_init(&g_audio_status_mutex, NULL);
//	ret = send(child_stream_sock_fd, "<?xml version=\"1.0\"?><message><type>request</type><action><command>SendFile</command><ftpaddr>192.168.1.34</ftpaddr><username>anonymous</username><filename>21 Guns - Green day.mp3</filename></action></message>", MAX_PACKAGE_LEN, 0);
	while (1) {
		/*################ receive message package here#####################*/
		if (g_RecvFileFlag != RECV_FILE_ENABLED) {
			appLog(LOG_DEBUG, "receiving message----");
			num_byte_read = read(child_stream_sock_fd, PackBuff,
					MAX_PACKAGE_LEN);
			if (num_byte_read < 0) {
				appLog(LOG_ERR, "read() call receive failed!\n");
			} else if (num_byte_read == 0) {
				appLog(LOG_DEBUG, "client connection closed!\n");
				g_RecvFileFlag = RECV_FILE_DISABLED;

				free(PackBuff);
				free(g_FileBuff);
				close(child_stream_sock_fd);
				exit(0);
			} else {
				parsePackageContent(PackBuff);
			}
			memset(PackBuff, 0x00, MAX_PACKAGE_LEN);

		} else {
			/*################ receive file data here #####################*/
			if (g_writeDataFlag != DISABLED) {
				continue;
			}
			appLog(LOG_DEBUG, "receiving file data-------");
			pthread_mutex_lock(&g_file_buff_mutex);
			memset(g_FileBuff, 0x00, MAX_FILE_BUFF_LEN);
			num_byte_read = read(child_stream_sock_fd, g_FileBuff,
			MAX_FILE_BUFF_LEN);
			if (num_byte_read < 0) {
				appLog(LOG_ERR, "read() call receive failed!\n");
			} else if (num_byte_read == 0) {
				appLog(LOG_DEBUG, "client connection closed!\n");
				g_RecvFileFlag = RECV_FILE_DISABLED;
				close(child_stream_sock_fd);
				exit(0);
			} else {
				if (isEOFPackage(g_FileBuff)) {
					appLog(LOG_DEBUG, "reached EOF!!!");

					g_RecvFileFlag = RECV_FILE_DISABLED;
					g_writeDataFlag = ENABLED; //force stop FileStreamHandlerThread
					memset(g_FileBuff, 0x00, MAX_FILE_BUFF_LEN);
					pthread_mutex_unlock(&g_file_buff_mutex);
				} else {
					appLog(LOG_DEBUG, "enabled write data to file")
					g_writeDataFlag = ENABLED;
					pthread_mutex_unlock(&g_file_buff_mutex);
				}
			}
		}

	}
}

/*******************************************************
 * Function used for parse package content then call
 * corresponding handler
 *******************************************************/
void parsePackageContent(char *packageBuff) {

	char package_header;
	char package_type;
	short int package_len;
//	int cmd;
//	int num_args;
	int i;
	/*	for (i = 0; i < sizeof(packageBuff); i++) {
	 appLog(LOG_DEBUG, "%x|", packageBuff[i]);
	 }*/
//check whether package header is valid or not (1st byte)
	package_header = *packageBuff;
//	appLog(LOG_DEBUG, "package_header %x\n", package_header);
	if (package_header != PACKAGE_HEADER) {
		appLog(LOG_ERR, "received invalid package!!!\n");

		return;
	}
	packageBuff++;

//analyse package type (2nd byte) and take package length
	package_type = *packageBuff;
//	appLog(LOG_DEBUG, "package_type %x", package_type);

	memcpy(&package_len, ++packageBuff, sizeof(package_len));

//convert byte order to little endian
	package_len = be16toh(package_len);

//	appLog(LOG_DEBUG, "package_len %d\n", package_len);
	packageBuff = packageBuff + 2;

	/*
	 #ifdef DEBUG	//debug--open/
	 appLog(LOG_DEBUG, "package_content\n");
	 for(i = 0; i < package_len; i++) {
	 appLog(LOG_DEBUG, "%x", packageBuff[i]);
	 }

	 #endif 	//debug--close
	 */
//parse package type, striped header and package type and length
	switch (package_type) {
	case PACKAGE_CONTROL:
		appLog(LOG_DEBUG, "package type: PACKAGE_CONTROL\n");
		ret = ControlHandler(packageBuff, package_len);
		break;
	case PACKAGE_RESQUEST:
		appLog(LOG_DEBUG, "package type: PACKAGE_REQUEST\n");
		//TODO PACKAGE_REQUEST
		ret = RequestHandler(packageBuff);
		break;
	default:
		appLog(LOG_DEBUG, "package_type invalid\n");
		break;
	}

	return;
}

/************************************************************
 * This handler used for call/process corresponding function
 * for each control command
 ************************************************************/
int ControlHandler(char *ctrlBuff, short int length) {

	appLog(LOG_DEBUG, "inside ControlHandler......\n");
	int ret;
	switch (*ctrlBuff) {
	case CMD_CTRL_PLAY_AUDIO:
		appLog(LOG_DEBUG, "CMD_CTRL_PLAY_AUDIO");
		if (g_audio_flag == AUDIO_PAUSE) {
			pthread_mutex_lock(&g_audio_status_mutex);
			g_audio_flag = AUDIO_PLAY;
			pthread_mutex_unlock(&g_audio_status_mutex);
			wrapperControlResp((char) CTRL_RESP_SUCCESS);
		} else {
			ret = initAudioPlayer(++ctrlBuff);
			ret = wrapperControlResp((char) CTRL_RESP_SUCCESS);
		}
		break;
	case CMD_CTRL_STOP_AUDIO:
		appLog(LOG_DEBUG, "CMD_CTRL_STOP_AUDIO");
//		ret = stopAudio();
		if (ret == ACP_SUCCESS) {
			wrapperControlResp((char) CTRL_RESP_SUCCESS);
		}
		break;
	case CMD_CTRL_PAUSE_AUDIO:
		appLog(LOG_DEBUG, "CMD_CTRL_PAUSE_AUDIO");
//		ret = pauseAudio();
		if (ret == ACP_SUCCESS) {
			wrapperControlResp((char) CTRL_RESP_SUCCESS);
		}
		break;
	case CMD_SEND_FILE:
		appLog(LOG_DEBUG, "CMD_SEND_FILE");
//		getFileFromFtp(g_remote_addr, ++ctrlBuff);
//		ret = initFileHandlerThread(ctrlBuff);
		if (ret == ACP_SUCCESS) {
			ret = wrapperControlResp(CTRL_RESP_ALREADY);
			if (ret == ACP_SUCCESS) {
				return ret;
			}
		} else {
			ret = wrapperControlResp((char) CTRL_RESP_FAILED);
			if (ret == ACP_SUCCESS) {
				return ret;
			}
		}
		break;
	case CMD_START_TRANFER_FILE:
		appLog(LOG_DEBUG, "CMD_START_TRANFER_FILE");
		g_StartTransferFlag = 1;
		break;
	default:
		appLog(LOG_DEBUG, "Command invalid");
		break;
	}

	return ret;
}

int RequestHandler(char *reqBuff) {

	int ret;
	switch (*reqBuff) {
	case CMD_REQ_GET_LIST_FILE:
		appLog(LOG_DEBUG, "CMD_REQ_GET_LIST_FILE");
		char *DirPath;
		char *ListFile;

		DirPath = malloc(FILE_PATH_LEN_MAX * sizeof(char));
		ListFile = malloc(LIST_FILE_MAX * sizeof(char));
		appLog(LOG_DEBUG, "DirPath addr: %p || ListFile addr: %p",
				DirPath, ListFile);
		memset(DirPath, 0, FILE_PATH_LEN_MAX);
		memset(ListFile, 0, LIST_FILE_MAX);

		strcat(DirPath, (char *) DEFAULT_PATH);
		appLog(LOG_DEBUG, "DirPath: %s", DirPath);

		ret = getListFile(DirPath, ListFile);
		if (ret == FILE_SUCCESS) {
			appLog(LOG_DEBUG, "%s", ListFile);
			ret = wrapperRequestResp(ListFile);
			appLog(LOG_DEBUG, "DirPath addr: %p || ListFile addr: %p",
					DirPath, ListFile);
			free(DirPath);
			free(ListFile);
			return ret;
		} else {
			free(DirPath);
			free(ListFile);
			return ACP_FAILED;
		}
		//no need break;
	}
}
/*Check if package is EOF control command*/

static int isEOFPackage(char *packBuff) {

	char tmpBuff[5];
	memcpy(&tmpBuff, packBuff, 5);
	if ((tmpBuff[0] != PACKAGE_HEADER) || (tmpBuff[1] != PACKAGE_CONTROL)
			|| (tmpBuff[4] != CMD_CTRL_EOF)) {
		return 0;
	}
	appLog(LOG_DEBUG, "isEOF");
	return 1;
}

int initFileHandlerThread(char *FileInfo) {

	g_StartTransferFlag = 0;
	g_waitCount = 0;
	g_file_stream = createFileStream(FileInfo);
	if (g_file_stream == NULL) {
		appLog(LOG_DEBUG, "Create file stream failed!!!");
		return ACP_FAILED;
	}
	if (pthread_create(&g_File_Handler_Thd, NULL, &FileStreamHandlerThread,
			NULL)) {
		appLog(LOG_DEBUG, "FileHandlerThread init fail\n");
		return ACP_FAILED;
	}
	pthread_mutex_init(&g_file_buff_mutex, NULL);
	g_RecvFileFlag = RECV_FILE_ENABLED;
//	pthread_join(&g_File_Handler_Thd, NULL);
	return ACP_SUCCESS;
}

int wrapperControlResp(char resp) {

	char *buff;
	int i, ret;
	buff = calloc(8, sizeof(char));
	memset(buff, 0x00, 8);

	*buff = PACKAGE_HEADER; //header
	*(++buff) = PACKAGE_CTRL_RESP; //package type
	*(++buff) = 0x01; //length
	buff = buff + 2;
	*buff = resp;

//	memcpy(&buff, (char *)resp, sizeof(resp));
	buff = buff - 4;
	ret = send(child_stream_sock_fd, buff, 8, 0);
	if (ret < 0) {
		appLog(LOG_DEBUG, "send response package failed");
		return ACP_FAILED;
	}

	//debug package response
	for (i = 0; i < 8; i++) {
		appLog(LOG_DEBUG, "%x", buff[i]);
	}
	free(buff);
	appLog(LOG_DEBUG, "send response package success");
	return ACP_SUCCESS;
}
int wrapperRequestResp(char *resp) {
	unsigned char *buff;
	int i, ret;
	unsigned int resp_len = (unsigned int) strlen(resp);
	resp_len += 1;
	buff = malloc(MAX_PACKAGE_LEN);
	memset(buff, 0x00, MAX_PACKAGE_LEN);

	*buff = (char) PACKAGE_HEADER; //header
	buff++;
	*buff = (char) PACKAGE_REQ_RESP; //package type
	buff++;
	appLog(LOG_DEBUG, "content length: %d", resp_len);
	memcpy(buff, (void *) &resp_len, 2); //content length
	appLog(LOG_DEBUG, "content length: %d", resp_len);
	buff += 2;
	*buff = (char) REQ_RESP_GET_LIST;
	buff++;
	memcpy(buff, resp, strlen(resp));

	//	memcpy(&buff, (char *)resp, sizeof(resp));
	buff -= 5;
	ret = send(child_stream_sock_fd, buff, MAX_PACKAGE_LEN, 0);
	if (ret < 0) {
		appLog(LOG_DEBUG, "send response package failed");
		return ACP_FAILED;
	}

#ifdef DEBUG
	//debug package response
	for (i = 0; i < 5; i++) {
		appLog(LOG_DEBUG, "%x", buff[i]);
	}
	buff += 5;
	appLog(LOG_DEBUG, "content:%s", buff);
	buff -= 5;
#endif

//	free(resp);
	free(buff);
	appLog(LOG_DEBUG, "send response package %d success", ret);
	return ACP_SUCCESS;
}

int initTaskHandler(char *message) {

	int ret;
	char *server_ip = &(g_ServerInfo.serverIp);
	char *room_name;
	char *msg_id;
	char *resp_for;

	room_name = getXmlElementByName(message, "room");
	msg_id = getXmlElementByName(message, "id");
	resp_for = getXmlElementByName(message, "command");

	if (!room_name) {
		appLog(LOG_DEBUG, "server_ip is invalid");
		free(room_name);
		return ACP_FAILED;
	}
	if (strstr(room_name, (char *) g_device_info.device_name) != NULL) {
		ret = pthread_create(&g_TaskHandlerThread, NULL, &TaskHandlerThread,
				(void *) server_ip);
		free(room_name);
		if (ret) {
			appLog(LOG_DEBUG, "init Task Handler failed");
			ret = ACP_FAILED;
		} else {
			appLog(LOG_DEBUG, "init Task handler success %d", stream_sock_fd);
			ret = ACP_SUCCESS;
		}
	}
	int count = 0;
	usleep(200000);
	while (count < 5) {

		if (stream_sock_fd > 0) {
			sendResultResponse(msg_id, resp_for, ACP_SUCCESS,
					(char *) g_device_info.device_name);
			break;
		} else {
			sleep(1);
			count++;
		}
		if (count == 5) {
			sendResultResponse(msg_id, resp_for, ACP_FAILED,
					(char *) g_device_info.device_name);
			break;
		}
	}
	free(msg_id);
	free(resp_for);
	return ret;
}

void *TaskHandlerThread(void *arg) {

	int ret;
	int byte_count = 0;
	char *server_ip = arg;
	char *pmsg_buff;

	pmsg_buff = calloc(BUFF_LEN_MAX, sizeof(char));

	appLog(LOG_DEBUG, "Pi is connecting to Station at %s", server_ip);
	stream_sock_fd = connecttoStreamSocket(server_ip, STREAM_SOCK_PORT);

	if (stream_sock_fd != SOCK_ERROR) {
		appLog(LOG_DEBUG, "Pi connected to Station!!!");
//		free(server_ip);
	} else {
		appLog(LOG_DEBUG,
				"Pi - Station connection init failed: stream_sock_fd %d, TaskHandlerThread exit",
				stream_sock_fd);
//		free(server_ip);
		pthread_exit(NULL);
	}
	pthread_mutex_init(&g_audio_status_mutex, NULL);
	while (1) {
		appLog(LOG_DEBUG, "running Task Handler!!!");
		memset(pmsg_buff, 0x00, BUFF_LEN_MAX);

		byte_count = recv(stream_sock_fd, pmsg_buff, BUFF_LEN_MAX, 0);
		if (byte_count < 0) {
			appLog(LOG_DEBUG, "received data failed!");
		} else if (byte_count == 0) {
			appLog(LOG_DEBUG,
					"remote socket was closed -> close stream sock %d",
					stream_sock_fd);
			close(stream_sock_fd);
			free(pmsg_buff);
			appLog(LOG_DEBUG, "exit Task Handler thread!!");
			pthread_exit(NULL);
		} else {
			appLog(LOG_DEBUG, "message data received >>>> %s", pmsg_buff);
			MessageProcessor(pmsg_buff);
		}
	}
}

void MessageProcessor(char *message) {

	int ret;
	char *pmsg_type;

	pmsg_type = getXmlElementByName(message, XML_MESSGAE_TYPE);

	if (pmsg_type != NULL) {
		appLog(LOG_DEBUG, "message type: %s", pmsg_type);
	} else {
		appLog(LOG_DEBUG, "message type is invalid!!");
		free(pmsg_type);
		return;
	}

	if (strcmp(pmsg_type, XML_MESSAGE_NOTIFY) == 0) {
		ret = NotifyMessageHandler(message);
		//TODO
	} else if (strcmp(pmsg_type, XML_MESSAGE_REQUEST) == 0) {
		ret = RequestMessageHandler(message);
		//TODO
	} else if (strcmp(pmsg_type, XML_MESSAGE_RESPONSE) == 0) {
		ret = ResponseMessageHandler(message);
		//TODO
	} else {
		appLog(LOG_DEBUG, "message type not match");
	}
	if (ret == ACP_SUCCESS) {
		appLog(LOG_DEBUG, "Handle %s message success!", pmsg_type);
	} else {
		appLog(LOG_DEBUG, "Handle %s message failed!", pmsg_type);
	}
	free(pmsg_type);
	return;

}

int NotifyMessageHandler(char *message) {

	appLog(LOG_DEBUG, "processing notify message");
	int ret;
	int notify_index;
	char *pinfo;

	pinfo = getXmlElementByName(message, XML_MESSGAE_INFO);
	if (!pinfo) {
		appLog(LOG_DEBUG, "nofity info: %s is invalid", pinfo);
		return ACP_FAILED;
	}
	notify_index = getNotifyIndex(pinfo);

	switch (notify_index) {
	case SERVER_INFO:
		ret = collectServerInfo(message);
		break;
	default:
		appLog(LOG_DEBUG, "notify info %s index %d no match",
				pinfo, notify_index);
		break;
	}

	free(pinfo);
	return ret;
}

int RequestMessageHandler(char *message) {
	appLog(LOG_DEBUG, "processing request message");
	int ret;
	int cmd_index;
	char *cmd;

	cmd = getXmlElementByName(message, XML_MESSAGE_COMMAND);

	if (!cmd) {
		appLog(LOG_DEBUG, "get request command failed: %s", cmd);
		return ACP_FAILED;
	}
	cmd_index = getRequestCommandIndex(cmd);

	if (cmd_index == 0) {
		appLog(LOG_DEBUG, "command not found");
		return ACP_FAILED;
	}
	switch (cmd_index) {
	case PI_CONNECT_SERVER:
		ret = initTaskHandler(message);
		break;
	case GET_FILE:
		ret = getFile(message);
		break;
	case PLAY_AUDIO:
		appLog(LOG_DEBUG, "called playAudio");
//		ret = playAudioAlt(message);
		ret = playMedia(message);
		break;
	case STOP_AUDIO:
		appLog(LOG_DEBUG, "called stopMediaPlayer");
//		ret = stopAudio(message);
		ret = stopMediaPlayer(message);
		break;
	case PAUSE_AUDIO:
		appLog(LOG_DEBUG, "called pauseAudio");
//		ret = pauseAudio(message);
		ret = pauseMedia(message);
		break;
	case CHANGE_DEVICE_NAME:
		appLog(LOG_DEBUG, "called changeRoomName");
		ret = changeRoomName(message);
		break;
	case DELETE_FILE:
		appLog(LOG_DEBUG, "called delete file");
		ret = deleteFile(message);
		break;
	case SET_BINARY:
		appLog(LOG_DEBUG, "called set relay binary");
		setRelayBinary(message);
		break;
	case GET_BINARY:
		appLog(LOG_DEBUG, "called get relay binary");
		getRelayBinary(message);
		break;
	default:
		break;
	}
}

int ResponseMessageHandler(char *message) {
	appLog(LOG_DEBUG, "processing response message");
}

int getNotifyIndex(char *info) {

	int numofnotifies = (sizeof(InfoList)) / (sizeof(NotifyInfo));
	int i;
	for (i = 0; i < numofnotifies; i++) {
		appLog(LOG_DEBUG, "InfoList[%d].info_str: %s", i, InfoList[i].info_str);
		if (strcmp(info, InfoList[i].info_str) == 0) {
			return InfoList[i].info_index;
		}
	}
	return 0;
}

int getRequestCommandIndex(char *command) {

	int numofcmd = (sizeof RequestCmdList) / (sizeof ReqestCmd);
	int i;
	for (i = 0; i < numofcmd; i++) {
		if (strcmp(command, RequestCmdList[i].cmd_str) == 0) {
			return RequestCmdList[i].cmd_index;
		}
	}
	return 0;
}

int playAudio(char *message) {

	char *pfile_name;
	char *msg_id;
	char *resp_for;
	int ret;

	msg_id = getXmlElementByName(message, "id");
	resp_for = getXmlElementByName(message, "command");
	pfile_name = getXmlElementByName(message, "filename");
	if (!pfile_name) {
		appLog(LOG_DEBUG, "file is not exist");
		free(msg_id);
		free(resp_for);
		free(pfile_name);
		sendResultResponse(msg_id, resp_for, ACP_FAILED, NULL);
		return ACP_FAILED;
	}
	appLog(LOG_DEBUG, "inside playAudio");
	if (g_audio_flag == STOP_AUDIO) {
		appLog(LOG_DEBUG, "file name: %s", pfile_name);

		snprintf(g_file_name_playing, sizeof(g_file_name_playing), "%s",
				pfile_name);
		ret = initAudioPlayer(pfile_name);
		usleep(500000); //0.5s
		if (ret == ACP_SUCCESS) {
			if (g_audio_flag == AUDIO_PLAY) {
				sendResultResponse(msg_id, resp_for, ACP_SUCCESS, pfile_name);
			} else {
				sendResultResponse(msg_id, resp_for, ACP_FAILED, NULL);
			}
		}
	} else if (g_audio_flag == AUDIO_PAUSE) {
		if (strcmp(g_file_name_playing, pfile_name) == 0) {
			pthread_mutex_lock(&g_audio_status_mutex);
			g_audio_flag = AUDIO_PLAY;
			pthread_mutex_unlock(&g_audio_status_mutex);
			appLog(LOG_DEBUG, "%s is playing", pfile_name);
			sendResultResponse(msg_id, resp_for, ACP_SUCCESS, "playing");
		} else {
			g_audio_flag = AUDIO_STOP;
			usleep(100000);
			ret = initAudioPlayer(pfile_name);
			usleep(500000); //0.5s
			if (ret == ACP_SUCCESS) {
				if (g_audio_flag == AUDIO_PLAY) {
					sendResultResponse(msg_id, resp_for, ACP_SUCCESS,
							pfile_name);
				} else {
					sendResultResponse(msg_id, resp_for, ACP_FAILED, NULL);
				}
			}
		}

	} else { // g_audio_flag == AUDIO_PLAY

//		pfile_name = getXmlElementByName(message, "filename");

		if (strcmp(g_file_name_playing, pfile_name) == 0) {
			appLog(LOG_DEBUG, "%s is playing", pfile_name);
			sendResultResponse(msg_id, resp_for, ACP_SUCCESS, "playing");
			return ACP_SUCCESS;
		} else {
			g_audio_flag = AUDIO_STOP;
			usleep(200000); //sleep 0.2s waiting for previous thread exit
		}

		ret = initAudioPlayer(pfile_name);
		usleep(500000); //0.5s
		if (ret == ACP_SUCCESS) {
			if (g_audio_flag == AUDIO_PLAY) {
				sendResultResponse(msg_id, resp_for, ACP_SUCCESS, pfile_name);
			} else {
				sendResultResponse(msg_id, resp_for, ACP_FAILED, NULL);
			}
		}
	}

	free(msg_id);
	free(resp_for);
	free(pfile_name);
	return ret;
}
int playAudioAlt(char *message) {

	int ret;
	PlayingInfo *info; //this pointer will be freed in sub-function
	char *resp_cmd, msg_id;
	char shell_cmd[256];

	//info struct will be freed in this function if play failed or not call initAudioPlayer
	//otherwise, it will be freed in playAudioThread
	appLog(LOG_DEBUG, "inside %s", __FUNCTION__);
	info = malloc(sizeof(PlayingInfo));
//	info->filename = calloc(128, sizeof(char));
	appLog(LOG_DEBUG, "inside %s", __FUNCTION__);
	msg_id = getXmlElementByName(message, "id");
	info->filename = getXmlElementByName(message, "filename");
	info->type = getXmlElementByName(message, "mediatype");

	resp_cmd = getXmlElementByName(message, "command");
	appLog(LOG_DEBUG, "inside %s", __FUNCTION__);
	if ((msg_id == NULL) || (info->filename == NULL) || (resp_cmd == NULL)) {
		appLog(LOG_DEBUG, "play failed");
		sendResultResponse("000", "play", ACP_FAILED, NULL);
		free(info->filename);
		free(msg_id);
		free(info->type);
		free(info);
		free(resp_cmd);
		return ACP_FAILED;
	}

	if (g_audio_flag == AUDIO_PLAY) {
		if (strncmp(g_file_name_playing, info->filename, strlen(info->filename))
				== 0) { //Audio file is playing

			sendResultResponse(msg_id, resp_cmd, ACP_SUCCESS,
					g_file_name_playing);
			free(info->filename);
			free(info);
		} else { //play another file
			//stop recently player thread
			memset(shell_cmd, 0x00, 256);
			snprintf(shell_cmd, 256, "echo -n q > %s", FIFO_PLAYER_PATH);
			if (system(shell_cmd) != 0) {
				pthread_cancel(g_play_audio_thd); //force stop thread
				pthread_mutex_lock(&g_audio_status_mutex);
				g_audio_flag = AUDIO_STOP;
				pthread_mutex_unlock(&g_audio_status_mutex);
				sendPlayingStatusNotify(NULL, g_file_name_playing, 2,
						"stopped!");
			} else { //quit audio player success, terminate player thread
				usleep(200000); //sleep 0.2s for waiting playing thread exit
				int check_count = 0;
				do {
					if (g_audio_flag == AUDIO_STOP) {
						break;
					} else {
						check_count++;
						if (check_count == 5) {
							pthread_mutex_lock(&g_audio_status_mutex);
							g_audio_flag = AUDIO_STOP;
							pthread_mutex_unlock(&g_audio_status_mutex);
							pthread_cancel(g_play_audio_thd);
						}
						usleep(50000);
					}
				} while (check_count < 5);

				memset(g_file_name_playing, 0x00, FILE_NAME_MAX);
				snprintf(g_file_name_playing, FILE_NAME_MAX, "%s",
						info->filename);

				ret = initAudioPlayerAlt(info);
				if (ret == ACP_SUCCESS) {
					sendResultResponse(msg_id, resp_cmd, ACP_SUCCESS,
							g_file_name_playing);
				} else {
					sendResultResponse(msg_id, resp_cmd, ACP_FAILED,
							g_file_name_playing);
					free(info->filename);
					free(info);
				}
			}
		}

	} else if (g_audio_flag == AUDIO_PAUSE) {
		if (strncmp(g_file_name_playing, info->filename, strlen(info->filename))
				== 0) {
			snprintf(shell_cmd, 256, "echo -n p > %s", FIFO_PLAYER_PATH);
			appLog(LOG_DEBUG, "resume cmd: %s", shell_cmd);
			if (system(shell_cmd) == 0) {
				sendResultResponse(msg_id, resp_cmd, ACP_SUCCESS,
						g_file_name_playing);
			} else {
				sendResultResponse(msg_id, resp_cmd, ACP_FAILED,
						g_file_name_playing);
			}
			free(info->filename);
			free(info);
		} else { //play another file
				 //stop recently player thread
			memset(shell_cmd, 0x00, 256);
			snprintf(shell_cmd, 256, "echo -n q > %s", FIFO_PLAYER_PATH);
			if (system(shell_cmd) != 0) {
				pthread_cancel(g_play_audio_thd);
				pthread_mutex_lock(&g_audio_status_mutex);
				g_audio_flag = AUDIO_STOP;
				pthread_mutex_unlock(&g_audio_status_mutex);
				sendPlayingStatusNotify(NULL, g_file_name_playing, 2,
						"stopped!");
			} else { //quit audio player success, terminate player thread
				int check_count = 0;
				do {
					if (g_audio_flag == AUDIO_STOP) {
						break;
					} else {
						check_count++;
						if (check_count == 5) {
							pthread_mutex_lock(&g_audio_status_mutex);
							g_audio_flag = AUDIO_STOP;
							pthread_mutex_unlock(&g_audio_status_mutex);
							pthread_cancel(g_play_audio_thd);
						}
						usleep(50000);
					}
				} while (check_count < 5);

				memset(g_file_name_playing, 0x00, FILE_NAME_MAX);
				snprintf(g_file_name_playing, FILE_NAME_MAX, "%s",
						info->filename);

				ret = initAudioPlayerAlt(info);
				if (ret == ACP_SUCCESS) {
					sendResultResponse(msg_id, resp_cmd, ACP_SUCCESS,
							g_file_name_playing);
				} else {
					sendResultResponse(msg_id, resp_cmd, ACP_FAILED,
							g_file_name_playing);
					free(info->filename);
					free(info);
				}
			}

		}

	} else { // status is AUDIO_STOP

		memset(g_file_name_playing, 0x00, FILE_NAME_MAX);
		snprintf(g_file_name_playing, FILE_NAME_MAX, "%s", info->filename);
		appLog(LOG_DEBUG, "debug-----");
		ret = initAudioPlayerAlt(info);
		appLog(LOG_DEBUG, "resp cmd: %s --- filename: %s",
				resp_cmd, info->filename);
		appLog(LOG_DEBUG, "debug-----");
		if (ret == ACP_SUCCESS) {
			sendResultResponse(msg_id, resp_cmd, ACP_SUCCESS,
					g_file_name_playing);
		} else {
			sendResultResponse(msg_id, resp_cmd, ACP_FAILED,
					g_file_name_playing);
			free(info->filename);
			free(info);
		}

	}
	free(resp_cmd);

}
int playMedia(char *message) {

	int ret;
	PlayingInfo *info; //this pointer will be freed in sub-function
	char *msg_id, *device_id, *cmd;
	char shell_cmd[256];

	//info struct will be freed in this function if play failed or not call initAudioPlayer
	//otherwise, it will be freed in playAudioThread
	info = malloc(sizeof(PlayingInfo));
	appLog(LOG_DEBUG, "inside %s", __FUNCTION__);
	msg_id = getXmlElementByName(message, "id");
	device_id = getXmlElementByName(message, "deviceid");
	info->filename = getXmlElementByName(message, "filename");
	info->type = getXmlElementByName(message, "mediatype");
	cmd = getXmlElementByName(message, "command");

	if (isRequestMessageValid(msg_id, device_id, cmd, info->filename,
			NULL) == ACP_FAILED) {
		sendResultResponse(msg_id, cmd, ACP_FAILED, "message format invalid");
		free(info->filename);
		free(msg_id);
		free(info->type);
		free(device_id);
		free(cmd);
		return ACP_FAILED;
	}

	if (g_audio_flag == AUDIO_PLAY) {
		if (strncmp(g_file_name_playing, info->filename, strlen(info->filename))
				== 0) { //Audio file is playing

			appLog(LOG_DEBUG, "g_file_name_playing: %s", g_file_name_playing);
			sendResultResponse(msg_id, cmd, ACP_SUCCESS, g_file_name_playing);
		} else { //play another file
			//stop recently player thread
			appLog(LOG_DEBUG, "g_file_name_playing: %s", g_file_name_playing);
			memset(shell_cmd, 0x00, 256);
			snprintf(shell_cmd, 256, "%s stop", PLAYER_CONTROLLER);
			if (system(shell_cmd) != 0) {
				pthread_cancel(g_play_audio_thd); //force stop thread
				pthread_mutex_lock(&g_audio_status_mutex);
				g_audio_flag = AUDIO_STOP;
				pthread_mutex_unlock(&g_audio_status_mutex);
				sendPlayingStatusNotify(NULL, g_file_name_playing, 2,
						"stopped!");
			} else { //quit audio player success, terminate player thread
				usleep(200000); //sleep 0.2s for waiting playing thread exit
				appLog(LOG_DEBUG, "g_file_name_playing: %s", g_file_name_playing);
				int check_count = 0;
				do {
					if (g_audio_flag == AUDIO_STOP) {
						break;
					} else {
						check_count++;
						if (check_count == 5) {
							pthread_mutex_lock(&g_audio_status_mutex);
							g_audio_flag = AUDIO_STOP;
							pthread_mutex_unlock(&g_audio_status_mutex);
							pthread_cancel(g_play_audio_thd);
						}
						usleep(50000);
					}
				} while (check_count < 5);

				memset(g_file_name_playing, 0x00, FILE_NAME_MAX);
				snprintf(g_file_name_playing, FILE_NAME_MAX, "%s",
						info->filename);

				ret = initMediaPlayer(info);
				if (ret == ACP_SUCCESS) {
					sendResultResponse(msg_id, cmd, ACP_SUCCESS,
							g_file_name_playing);
				} else {
					sendResultResponse(msg_id, cmd, ACP_FAILED,
							g_file_name_playing);
					free(info->filename);
					free(info);
				}
			}
		}

	} else if (g_audio_flag == AUDIO_PAUSE) {
		if (strncmp(g_file_name_playing, info->filename, strlen(info->filename))
				== 0) {
			appLog(LOG_DEBUG, "current: %s\tnew:%s\n", g_file_name_playing, info->filename);
			snprintf(shell_cmd, 256, "%s pause", PLAYER_CONTROLLER);
			appLog(LOG_DEBUG, "resume cmd: %s", shell_cmd);
			if (system(shell_cmd) == 0) {
				pthread_mutex_lock(&g_audio_status_mutex);
				g_audio_flag = AUDIO_PLAY;
				pthread_mutex_unlock(&g_audio_status_mutex);
				sendResultResponse(msg_id, cmd, ACP_SUCCESS,
						g_file_name_playing);
			} else {
				sendResultResponse(msg_id, cmd, ACP_FAILED,
						g_file_name_playing);
			}
			free(info->filename);
			free(info);
		} else { //play another file
				 //stop recently player thread
			appLog(LOG_DEBUG, "current: %s\tnew:%s\n", g_file_name_playing, info->filename);
			memset(shell_cmd, 0x00, 256);
			snprintf(shell_cmd, 256, "%s stop", PLAYER_CONTROLLER);
			appLog(LOG_DEBUG, "run: %s", shell_cmd);
			if (system(shell_cmd) != 0) {
				pthread_cancel(g_play_audio_thd);
				pthread_mutex_lock(&g_audio_status_mutex);
				g_audio_flag = AUDIO_STOP;
				pthread_mutex_unlock(&g_audio_status_mutex);
				sendPlayingStatusNotify(NULL, g_file_name_playing, 2,
						"stopped!");
			} else { //quit audio player success, terminate player thread
				int check_count = 0;
				do {
					if (g_audio_flag == AUDIO_STOP) {
						break;
					} else {
						check_count++;
						if (check_count == 5) {
							pthread_mutex_lock(&g_audio_status_mutex);
							g_audio_flag = AUDIO_STOP;
							pthread_mutex_unlock(&g_audio_status_mutex);
							pthread_cancel(g_play_audio_thd);
						}
						usleep(50000);
					}
				} while (check_count < 5);

				memset(g_file_name_playing, 0x00, FILE_NAME_MAX);
				snprintf(g_file_name_playing, FILE_NAME_MAX, "%s",
						info->filename);

				ret = initMediaPlayer(info);
				if (ret == ACP_SUCCESS) {
					sendResultResponse(msg_id, cmd, ACP_SUCCESS,
							g_file_name_playing);
				} else {
					sendResultResponse(msg_id, cmd, ACP_FAILED,
							g_file_name_playing);
					free(info->filename);
					free(info);
				}
			}

		}

	} else { // status is AUDIO_STOP

		memset(g_file_name_playing, 0x00, FILE_NAME_MAX);
		snprintf(g_file_name_playing, FILE_NAME_MAX, "%s", info->filename);
		appLog(LOG_DEBUG, "g_file_name_playing %s", g_file_name_playing);
		ret = initMediaPlayer(info);
		if (ret == ACP_SUCCESS) {
			sendResultResponse(msg_id, cmd, ACP_SUCCESS, g_file_name_playing);
		} else {
			sendResultResponse(msg_id, cmd, ACP_FAILED, g_file_name_playing);
		}

	}
	free(cmd);
	free(msg_id);
	free(device_id);

}
int collectServerInfo( message) {

	char *servIp;
	char *ftpaddr;
	char *ftpusr;
	char *ftppass;
	char *msg_id;
	char *resp_for;

	memset(&(g_ServerInfo.ftp.Password), 0x00,
			sizeof(g_ServerInfo.ftp.Password));

	servIp = getXmlElementByName(message, "serveraddress");
	ftpaddr = getXmlElementByName(message, "ftpaddress");
	ftpusr = getXmlElementByName(message, "ftpuser");
	ftppass = getXmlElementByName(message, "ftppassword");
	msg_id = getXmlElementByName(message, "id");
	resp_for = getXmlElementByName(message, XML_MESSGAE_INFO);

	if (servIp != NULL) {
		memset(&(g_ServerInfo.serverIp), 0x00, sizeof(g_ServerInfo.serverIp));
		snprintf(g_ServerInfo.serverIp, sizeof(g_ServerInfo.serverIp), "%s",
				servIp);
		free(servIp);
	}

	if (ftpaddr != NULL) {
		memset(&(g_ServerInfo.ftp.Ip), 0x00, sizeof(g_ServerInfo.ftp.Ip));
		snprintf(g_ServerInfo.ftp.Ip, sizeof(g_ServerInfo.ftp.Ip), "%s",
				ftpaddr);
		free(ftpaddr);
	}

	if (ftpusr != NULL) {
		memset(&(g_ServerInfo.ftp.User), 0x00, sizeof(g_ServerInfo.ftp.User));
		snprintf(g_ServerInfo.ftp.User, sizeof(g_ServerInfo.ftp.User), "%s",
				ftpusr);
		free(ftpusr);
	}

	if (ftppass != NULL) {
		memset(&(g_ServerInfo.ftp.Password), 0x00,
				sizeof(g_ServerInfo.ftp.Password));
		snprintf(g_ServerInfo.ftp.Password, sizeof(g_ServerInfo.ftp.Password),
				"%s", ftppass);
		free(ftppass);
	}

	if (!(servIp && ftpaddr && ftpusr && ftppass)) {
		appLog(LOG_DEBUG, "collect Server Info failed");
		return ACP_FAILED;
	}
	appLog(LOG_DEBUG, "%s || %s || %s || %s",
			g_ServerInfo.serverIp, g_ServerInfo.ftp.Ip, g_ServerInfo.ftp.User, g_ServerInfo.ftp.Password);
	char *buff = NULL;
	buff = writeXmlToBuffResp(msg_id, resp_for, RESPONSE_SUCCESS,
			(char *) g_device_info.device_name);
	if (buff == NULL) {
		return ACP_FAILED;
	}
	/*int ret = sendto(multicast_fd, buff, BUFF_LEN_MAX, 0,(struct sockaddr*)&mul_sock, sizeof(mul_sock));
	 if(ret > 0){
	 appLog(LOG_DEBUG, "sent %d byte response success!", ret);
	 }
	 sleep(5);*/
	appLog(LOG_DEBUG, "xml response %d byte: %s", strlen(buff), buff);
	sendMulMessage(buff);
	free(buff);
	free(msg_id);
	free(resp_for);
	return ACP_SUCCESS;
}

int sendResultResponse(char *msg_id, char *resp_for, int resp_code,
		char *resp_content) {

	int ret;
	char *resp_buff;

	if (resp_code == 0) {
		resp_buff = writeXmlToBuffResp(msg_id, resp_for,
				(char *) RESPONSE_SUCCESS, resp_content);
	} else {
		resp_buff = writeXmlToBuffResp(msg_id, resp_for,
				(char *) RESPONSE_FAILED, resp_content);
	}

	if (resp_buff == NULL) {
		appLog(LOG_DEBUG, "send result response failed!");
		return ACP_FAILED;
	}

	appLog(LOG_DEBUG, "response content: %s", resp_buff);
	ret = send(stream_sock_fd, resp_buff, strlen(resp_buff) + 1, 0);
	free(resp_buff);
	if (ret < 0) {
		appLog(LOG_DEBUG, "send response failed");
		return ACP_FAILED;
	}

	return ACP_SUCCESS;
}

/*int sendPlayingStatusNotify(char *file_name, char *status) {

 char *data_buff;
 int ret, i;
 NotifyPiStatus *notify_status;

 appLog(LOG_DEBUG, "--------");
 notify_status = malloc(sizeof(notify_status));
 appLog(LOG_DEBUG, "--------");
 notify_status->info = calloc(32, sizeof(char));
 appLog(LOG_DEBUG, "--------");
 appLog(LOG_DEBUG, "--------");
 notify_status->obj_ele = malloc(sizeof(XMLTag));
 notify_status->status_ele = malloc(sizeof(XMLTag));
 notify_status->obj_ele->ele_name = calloc(32, sizeof(char));
 appLog(LOG_DEBUG, "--------");
 notify_status->status_ele->ele_name = calloc(32, sizeof(char));
 appLog(LOG_DEBUG, "--------");
 notify_status->obj_ele->ele_content = calloc(128, sizeof(char));
 appLog(LOG_DEBUG, "--------");
 if ((notify_status->content_ele[i]->ele_name == NULL)
 || (notify_status->content_ele[i]->ele_content == NULL)) {
 return ACP_FAILED;
 }
 //	memset(notify_status, 0, sizeof(notify_status));
 appLog(LOG_DEBUG, "--------");
 notify_status->info = "AudioStatus";
 appLog(LOG_DEBUG, "--------");
 memcpy(notify_status->obj_ele->ele_name, (char *)"filename", strlen("filename"));
 appLog(LOG_DEBUG, "--------");
 notify_status->obj_ele->ele_content = file_name;
 appLog(LOG_DEBUG, "--------");
 notify_status->status_ele->ele_name = "status";
 notify_status->status_ele->ele_content = status;
 appLog(LOG_DEBUG, "--------");
 data_buff = writeXmlToBuffNotify("123", notify_status);

 if (data_buff == NULL) {
 appLog(LOG_DEBUG, "xml data is null");
 return ACP_FAILED;
 }
 appLog(LOG_DEBUG, "notify data: \n %s", data_buff);
 ret = send(stream_sock_fd, data_buff, strlen(data_buff), 0);

 if( ret < 0){
 appLog(LOG_DEBUG, "send nofity failed!");
 return ACP_FAILED;
 }
 free(notify_status);
 free(data_buff);
 }*/

int sendPlayingStatusNotify(char *msg_id, char *file_name, int num_tag,
		char *status) {

	char *data_buff;
	int ret, i;
	NotifyPiStatus notify_status;

	notify_status.info = "audio status";
	notify_status.num_content_tag = num_tag;
	notify_status.content_tag[0].ele_name = "filename";
	notify_status.content_tag[0].ele_content = file_name;
	notify_status.content_tag[1].ele_name = "status";
	notify_status.content_tag[1].ele_content = status;

	if (msg_id == NULL) {
		msg_id = (char *) MSG_ID_DEFAULT;
	}
	data_buff = writeXmlToBuffNotify(msg_id, notify_status);

	if (data_buff == NULL) {
		appLog(LOG_DEBUG, "xml data is null");
		return ACP_FAILED;
	}
	appLog(LOG_DEBUG, "notify data: \n %s", data_buff);
	ret = send(stream_sock_fd, data_buff, strlen(data_buff) + 1, 0);

	if (ret < 0) {
		appLog(LOG_DEBUG, "send nofity failed!");
		return ACP_FAILED;
	}
	free(data_buff);
	return ACP_SUCCESS;
}

//modify config file and update device info variable
//if room name is not match current room->ignore it
int changeRoomName(char *message) {

	char *msg_id, *cmd, *cur_name, *new_name;

	cur_name = getXmlElementByName(message, "currentname");
	if (strcmp(g_device_info.device_name, cur_name) != 0) {
		free(cur_name);
		return ACP_SUCCESS;
	}

	msg_id = getXmlElementByName(message, "id");
	cmd = getXmlElementByName(message, "command");
	new_name = getXmlElementByName(message, "newName");

	if (cmd == NULL || new_name == NULL) {
		free(msg_id);
		free(cmd);
		free(new_name);
		return ACP_FAILED;
	}

	if (changeConfigSetting("deviceName", new_name) == FILE_ERROR) {

		return ACP_FAILED;
	}
	strcpy(g_device_info.device_name, new_name);

	free(cur_name);
	free(msg_id);
	free(cmd);
	free(new_name);
	return ACP_SUCCESS;
}

int deleteFile(char *message) {

	int ret = ACP_SUCCESS;
	char *msg_id = NULL;
	char *device_id = NULL;
	char *cmd = NULL;
	char *file_name = NULL;
	char shell_cmd[128];

	msg_id = getXmlElementByName(message, "id");
	device_id = getXmlElementByName(message, "deviceid");
	cmd = getXmlElementByName(message, "command");
	file_name = getXmlElementByName(message, "filename");

	if (isRequestMessageValid(msg_id, device_id, cmd, file_name,
			NULL) == ACP_FAILED) {
		ret = ACP_FAILED;
	} else {
		sprintf(shell_cmd, "rm \"%s%s\"", DEFAULT_PATH, file_name);
		appLog(LOG_DEBUG, "shell cmd >>> %s", shell_cmd);
		if (system(shell_cmd) != 0) {
			ret = ACP_FAILED;
		}
	}
	sendResultResponse(msg_id, cmd, ret, NULL);
	free(msg_id);
	free(device_id);
	free(cmd);
	free(file_name);
	return ret;
}

int isRequestMessageValid(char *msg_id, char *device_id, char *cmd, char *arg,
		char *arg_name) {

	if (msg_id == NULL || device_id == NULL || cmd == NULL) {
		appLog(LOG_DEBUG, "message is not match!!");
		return ACP_FAILED;
	}
	if (strcmp(device_id, g_device_info.device_id) != 0) {
		return ACP_FAILED;
	}
	return ACP_SUCCESS;
}

int connectStation(char *station_addr) {

	int count = 0;

	//if the argument is host name -> call gethostbyname
	//TO DO:
	do {

		stream_sock_fd = connecttoStreamSocket(station_addr, STREAM_SOCK_PORT);
		if (stream_sock_fd < 0) {
			count++;
			sleep(2);
			appLog(LOG_DEBUG, "trying connect to station - %d failed", count);
		} else {
			return ACP_SUCCESS;
		}

	} while (count < 5 && stream_sock_fd < 0);

	return ACP_FAILED;
}

void TaskReceiver() {

	int ret;
	char *msg_buff;
	int buff_len;

	g_audio_flag = AUDIO_STOP;
	g_file_name_playing = calloc(FILE_NAME_MAX, sizeof(char));
	if (g_file_name_playing == NULL) {
		appLog(LOG_ERR, "allocate memory failed - app restart");
		exit(EXIT_FAILURE);
	}
	
	setSignalHandlers();
	MediaPlayerUtil("stop");
	initializeGPIO();
	while (1) {
		ret = connectStation((char *) g_device_info.station_addr);
		if (ret == ACP_FAILED) {
			//terminate app
			appLog(LOG_ERR, "connect to station failed - terminating app");
			exit(EXIT_FAILURE);
		}

		ret = notifyDeviceInfo(3, "IDLE");
		if (ret == ACP_FAILED) {
			//need suitable handle
			appLog(LOG_DEBUG, "sending notification failed!!1");
		}

		msg_buff = calloc(BUFF_LEN_MAX, sizeof(char));
		if (msg_buff == NULL) {
			appLog(LOG_ERR, "allocate memory failed - app restart");
			exit(EXIT_FAILURE);
		}
		//waiting for receiving message
		while (1) {

			appLog(LOG_DEBUG, "waiting incoming message...");
			buff_len = recv(stream_sock_fd, msg_buff, BUFF_LEN_MAX, 0);
			if (buff_len < 0) {
				appLog(LOG_ERR, "have no message received!!!");

			} else if (buff_len == 0) {
				//station is down, need more handle
				appLog(LOG_ERR, "station was disconnected");
				close(stream_sock_fd);
				free(msg_buff);
				MediaPlayerUtil("stop");
				//marking station is down for next step
				break;
			} else {
				//handle message
				appLog(LOG_DEBUG, "message received>>>>> %s", msg_buff);
				MessageProcessor(msg_buff);
				memset(msg_buff, 0, BUFF_LEN_MAX);

			}
		}
	}
}

int notifyDeviceInfo(int num_tag, char *status) {

	char *data_buff;
	int ret, i;
	NotifyPiStatus notify_status;

	notify_status.info = "mbox info";
	notify_status.num_content_tag = num_tag;
	notify_status.content_tag[0].ele_name = "devicename";
	notify_status.content_tag[0].ele_content = g_device_info.device_name;
	notify_status.content_tag[1].ele_name = "address";
	notify_status.content_tag[1].ele_content = g_device_info.address;
	notify_status.content_tag[2].ele_name = "status";
	notify_status.content_tag[2].ele_content = status;

	data_buff = writeXmlToBuffNotify("000", notify_status);

	if (data_buff == NULL) {
		appLog(LOG_DEBUG, "xml data is null");
		return ACP_FAILED;
	}
	appLog(LOG_DEBUG, "notify data: \n %s", data_buff);
	ret = send(stream_sock_fd, data_buff, strlen(data_buff) + 1, 0);

	if (ret < 0) {
		appLog(LOG_DEBUG, "send nofity failed!");
		return ACP_FAILED;
	}
	free(data_buff);
	return ACP_SUCCESS;
}

void closePlayerfifo(char *file) {

	char cmd[128];
	//check fifo file existed or not
	appLog(LOG_DEBUG, "[debug]");
	if (access(file, F_OK) != -1) {
		appLog(LOG_DEBUG, "[debug]");
		//File exist -> stop playing
		sprintf(cmd, "echo -n q > %s", file);
		appLog(LOG_DEBUG, "[debug]-%s", cmd);
		system(cmd);
		appLog(LOG_DEBUG, "[debug]");
		unlink(file);
		appLog(LOG_DEBUG, "[debug]");
	}
}

void setSignalHandlers(void) {
	struct sigaction act, oldact;

	act.sa_flags = (SA_NOCLDSTOP | SA_NOCLDWAIT | SA_RESTART | SA_SIGINFO);
	act.sa_sigaction = handleSigSegv;
	if (sigaction(SIGSEGV, &act, &oldact)) {
		appLog(LOG_ERR, "sigaction failed... errno: %d", errno);
	} else {
		if (oldact.sa_handler == SIG_IGN)
			appLog(LOG_DEBUG, "oldact RTMIN: SIGIGN");

		if (oldact.sa_handler == SIG_DFL)
			appLog(LOG_DEBUG, "oldact RTMIN: SIGDFL");
	}

}

static void handleSigSegv(int signum, siginfo_t *pInfo, void *pVoid) {
	uint32_t *up = (uint32_t *) &signum;
	unsigned int sp = 0;

	appLog(LOG_ALERT, "SIGNAL SIGSEGV [%d] RECEIVED, aborting...", signum);

	closePlayerfifo(FIFO_PLAYER_PATH);

	abort();
}

int stopMediaPlayer(char *message) {

	char *resp_for;
	char *msg_id;
	char *device_id;
	char shell_cmd[256];

	memset(shell_cmd, 0x00, 256);
	msg_id = getXmlElementByName(message, "id");
	resp_for = getXmlElementByName(message, "command");
	device_id = getXmlElementByName(message, "deviceid");
	if (isRequestMessageValid(msg_id, device_id, resp_for, NULL,
			NULL) != ACP_SUCCESS) {
		sendResultResponse(msg_id, resp_for, ACP_FAILED, "Request invalid");
	} else {
		if (g_audio_flag == AUDIO_STOP) {
			sendResultResponse(msg_id, resp_for, ACP_SUCCESS, "Stopped");
		} else {
			snprintf(shell_cmd, 256, "%s stop", PLAYER_CONTROLLER);
			if (system(shell_cmd) != 0) {
				pthread_cancel(g_play_audio_thd);
				pthread_mutex_lock(&g_audio_status_mutex);
				g_audio_flag = AUDIO_STOP;
				pthread_mutex_unlock(&g_audio_status_mutex);
				sendPlayingStatusNotify(NULL, g_file_name_playing, 2,
						"stopped!");
			} else { //quit audio player success, terminate player thread
				int check_count = 0;
				do {
					if (g_audio_flag == AUDIO_STOP) {
						break;
					} else {
						check_count++;
						if (check_count == 10) {
							pthread_mutex_lock(&g_audio_status_mutex);
							g_audio_flag = AUDIO_STOP;
							pthread_mutex_unlock(&g_audio_status_mutex);
							pthread_cancel(g_play_audio_thd);
						}
						usleep(100000);
					}
				} while (check_count < 10);
			}

			memset(g_file_name_playing, 0x00, 128);
			sendResultResponse(msg_id, resp_for, ACP_SUCCESS, NULL);
		}
	}
	free(resp_for);
	free(msg_id);
	free(device_id);
	return ACP_SUCCESS;
}

int pauseMedia(char *message) {

	char *resp_for;
	char *msg_id;
	char *device_id;
	char shell_cmd[256];

	memset(shell_cmd, 0x00, 256);
	msg_id = getXmlElementByName(message, "id");
	resp_for = getXmlElementByName(message, "command");
	device_id = getXmlElementByName(message, "deviceid");
	if (isRequestMessageValid(msg_id, device_id, resp_for, NULL,
			NULL) != ACP_SUCCESS) {
		sendResultResponse(msg_id, resp_for, ACP_FAILED, "Request invalid");
	} else if (g_audio_flag != AUDIO_PAUSE) {

		snprintf(shell_cmd, 256, "%s pause", PLAYER_CONTROLLER);
		appLog(LOG_DEBUG, "inside %s", __FUNCTION__);
		if (system(shell_cmd) == 0) {
			appLog(LOG_DEBUG, "inside %s", __FUNCTION__);
			sendResultResponse(msg_id, resp_for, ACP_SUCCESS,
					g_file_name_playing);
			pthread_mutex_lock(&g_audio_status_mutex);
			appLog(LOG_DEBUG, "inside %s", __FUNCTION__);
			g_audio_flag = AUDIO_PAUSE;
			pthread_mutex_unlock(&g_audio_status_mutex);
		} else {
			sendResultResponse(msg_id, resp_for, ACP_FAILED,
					g_file_name_playing);
		}

	}
	free(msg_id);
	free(resp_for);
	free(device_id);
	return ACP_SUCCESS;
}

void setRelayBinary(char *message){

	char *msg_id, *device_id, *command, value;

	msg_id = getXmlElementByName(message, "id");
	device_id = getXmlElementByName(message, "deviceid");
	command = getXmlElementByName(message, "command");
	value = getXmlElementByName(message, "value");

	if(isRequestMessageValid(msg_id, device_id, command, NULL, NULL) != ACP_SUCCESS){
		sendResultResponse(msg_id, command, ACP_FAILED, "Request invalid");
	}else{
		
		setBinary(RELAYPIN1, atoi(value));
		if (getBinary(RELAYPIN1) == atoi(value)){
			sendResultResponse(msg_id, command, ACP_SUCCESS, value);
		}else{
			sendResultResponse(msg_id, command, ACP_FAILED, "");
		}
	}

	free(msg_id);
	free(device_id);
	free(command);
	free(value);
}

void getRelayBinary(char *message){

	char *msg_id, *device_id, *command;


	msg_id = getXmlElementByName(message, "id");
	device_id = getXmlElementByName(message, "deviceid");
	command = getXmlElementByName(message, "command");

	if(isRequestMessageValid(msg_id, device_id, command, NULL, NULL) != ACP_SUCCESS){
		sendResultResponse(msg_id, command, ACP_FAILED, "Request invalid");
	}else{

		int relay_value = getBinary(RELAYPIN1);
		if(relay_value == 0){

			sendResultResponse(msg_id, command, ACP_SUCCESS, "0");
		}else{

			sendResultResponse(msg_id, command, ACP_SUCCESS, "1");
		}
		
	}

	free(msg_id);
	free(device_id);
	free(command);
}

int MediaPlayerUtil(char *request) {

	char shell[128];

	sprintf(shell, "%s %s", PLAYER_CONTROLLER, request);
	appLog(LOG_DEBUG, "%s", shell);
	return system(shell);
}
