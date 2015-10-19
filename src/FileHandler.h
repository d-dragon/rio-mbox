/*
 * receiveFile.h
 *
 *  Created on: Jul 11, 2014
 *      Author: d-dragon
 */

#ifndef RECEIVE_FILE_H_
#define RECEIVE_FILE_H_


#ifdef RPI
#define DEFAULT_PATH "/media/data/mbox/"
#define SOURCE_PATH "/home/pi/Smart_Classroom/src/"
#define PY_SYS_PATH "sys.path.append('/user/bin')"
#define DEFAULT_CONFIG_PATH "/etc/mbox.cfg"
#else
#define DEFAULT_PATH "/home/duyphan/git/Smart_Classroom/Media_Hub/List_File/"
#define SOURCE_PATH "/home/duyphan/git/Smart_Classroom/Media_Hub/src"
#define PY_SYS_PATH "sys.path.append('/home/duyphan/git/Smart_Classroom/Media_Hub/src')"
#define DEFAULT_CONFIG_PATH "/home/duyphan/mbox.cfg"
//#define PY_SYS_PATH "sys.path.append('/home/r1/Downloads/testpi')"
#endif
//#define DEFAULT_PATH "/home/"
#define FILE_PATH_LEN_MAX 200
#define FILE_NAME_MAX 256
#define LIST_FILE_MAX 1000
#define FILE_ERROR -1
#define FILE_SUCCESS 0
#define FILE_UNKNOW 2
#define PY_MODULE "ftplib_example"
#define FTP_GET_FILE_FUNC "getFile"

FILE *g_file_stream;
int g_file_size;
pthread_t g_File_Handler_Thd;

extern pthread_mutex_t g_file_buff_mutex;

typedef struct file{
	int index;
	char *filename;
}FileInfo;

struct device_config_t{
	char device_id[24];
	char device_name[128];
	char address[16];
	char station_addr[16];
};

struct device_config_t g_device_info;

FILE *createFileStream(char *);
void *FileStreamHandlerThread();
void closeFileStream();
int getListFile(char *, char *);

int getFileFromFtp(char *FtpServerIP, char *FileName, char *UserName, char *Password);
int getFile(char *);
int changeConfigSetting(char *setting_name, char *new_value);
int createDefaultConfigFile(char *mac_addr);
void initDeviceInfo(char *mac_addr);


#endif /* RECEIVE_FILE_H_ */
