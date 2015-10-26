/*
 * receive_file.c
 *
 *  Created on: Jul 12, 2014
 *      Author: d-dragon
 */

#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <dirent.h>
#include <python2.7/Python.h>
#include <libconfig.h>

#include "FileHandler.h"
#include "sock_infra.h"
#include "xmlHandler.h"
#include "logger.h"
#include "acpHandler.h"

FILE *
createFileStream(char *FileName) {

	FILE *tmp_file;
	char *path_to_file;
	path_to_file = calloc(FILE_PATH_LEN_MAX, sizeof(char));

	appLog(LOG_INFO, "FileName: %s\n", FileName);
	sprintf(path_to_file, "%s%s", DEFAULT_PATH, FileName);
	appLog(LOG_INFO, "path to file: %s\n", path_to_file);

	tmp_file = fopen(path_to_file, "w");
	if (tmp_file == NULL) {
		appLog(LOG_ERR, "fopen failed\n");
		return NULL;
	}
	appLog(LOG_DEBUG, "File stream was created successfully\n");

	return tmp_file;
}

void *
FileStreamHandlerThread() {

	int szwrite = 0;
	appLog(LOG_DEBUG, "start FileStreamHandlerThread--------\n");
	/*	while (!g_StartTransferFlag && g_RecvFileFlag == RECV_FILE_DISABLED) {
	 g_waitCount++;
	 if (g_waitCount <= MAX_WAITING_COUNT) {
	 usleep(1000);
	 } else {
	 appLog(LOG_DEBUG,
	 "Wating start transfer file timeout---thread exit\n");
	 g_RecvFileFlag = RECV_FILE_DISABLED;
	 pthread_exit(NULL);

	 }
	 }*/

//	appLog(LOG_DEBUG, "Start Transfer file-------------\n");
//	g_RecvFileFlag = RECV_FILE_ENABLED;
	while (1) {

		//first time need data before write it to file stream
		if (g_writeDataFlag != ENABLED) {
			continue;
		}
		pthread_mutex_lock(&g_file_buff_mutex);
		//check transfer file flow finished--thread exit
		if (g_RecvFileFlag == RECV_FILE_DISABLED) {
			g_writeDataFlag = DISABLED;
			closeFileStream();

			if (wrapperControlResp(CTRL_RESP_FILE_FINISH) == ACP_SUCCESS) {
				appLog(LOG_DEBUG,
						"Transfer file completed, FileStreamHandlerThread exit");
			} else {
				appLog(LOG_DEBUG,
						"Transfer file incomplete, FileStreamHandlerThread exit");
			}
			pthread_mutex_unlock(&g_file_buff_mutex);
			pthread_mutex_destroy(&g_file_buff_mutex);

			pthread_exit(NULL);
		}
		//write data to file
		szwrite = fwrite(g_FileBuff, 1, num_byte_read, g_file_stream);
		//				printf("%d bytes was written\n", szwrite);
		if (szwrite < num_byte_read) {
			appLog(LOG_ERR, "write data to stream file failed!\n");
			g_writeDataFlag = DISABLED;
			g_RecvFileFlag = RECV_FILE_DISABLED;
			closeFileStream();
			if (wrapperControlResp(CTRL_RESP_FAILED) == ACP_SUCCESS) {
				appLog(LOG_DEBUG,
						"Transfer file incomplete, FileStreamHandlerThread exit");
			}
			pthread_mutex_unlock(&g_file_buff_mutex);
			pthread_mutex_destroy(&g_file_buff_mutex);

			pthread_exit(NULL);
		} else if (szwrite == num_byte_read) {
			appLog(LOG_DEBUG, "wrote %d byte to file stream\n",
					(int) num_byte_read);
			g_writeDataFlag = DISABLED;
			pthread_mutex_unlock(&g_file_buff_mutex);

		}
		/*		pthread_mutex_lock(g_file_buff_mutex);
		 pthread_cond_wait(g_file_thread_cond, g_file_buff_mutex);
		 pthread_mutex_unlock(g_file_buff_mutex);*/
	}
}

void closeFileStream() {
	if (g_file_stream != NULL) {
		fclose(g_file_stream);
	}
}

int getListFile(char *DirPath, char *ListFile) {

	DIR *dir = NULL;
	struct dirent *dirnode = NULL;

	dir = opendir(DirPath);
	if (dir) {
		while ((dirnode = readdir(dir)) != NULL) {
			if (dirnode->d_type == DT_REG) {
				if ((strlen(ListFile) + strlen(dirnode->d_name)) > LIST_FILE_MAX) {
					//list file length exceed max len
					return FILE_UNKNOW;
				}
				strcat(ListFile, dirnode->d_name);
				strcat(ListFile, "|");
			}
		}
	} else {
		closedir(dir);
		appLog(LOG_DEBUG, "open directory failed");
		return FILE_ERROR;
	}
	closedir(dir);
	return FILE_SUCCESS;
}

int getFileFromFtp(char *FtpServerIP, char *FileName, char *UserName,
		char *Password) {

	PyObject *pName, *pModule, *pFunc;
	PyObject *pArgs, *pValue;
/*	
	char shell[256];
	sprintf(shell, "wget --user=%s --password='%s' \"ftp://%s:2121/demo/%s\" -O /media/data/\"%s\"", UserName, Password, FtpServerIP, FileName, FileName);
	if (system(shell) == 0) {
		appLog(LOG_DEBUG, "Got %s successfully!", FileName);
		return FILE_SUCCESS;
	}else{
		
		appLog(LOG_DEBUG, "Got %s failed!", FileName);
		return FILE_ERROR;
	}
*/
	int i, numargs;

	if (UserName != NULL) {
		numargs = 4;
	} else { //anonymous login
		numargs = 2;
	}

	//init python interpreter
	Py_Initialize();
	PyRun_SimpleString("import sys");
	PyRun_SimpleString(PY_SYS_PATH);
	appLog(LOG_DEBUG, "init python interpreter done!!!");
	//load python module
	pName = PyString_FromString(PY_MODULE);
	pModule = PyImport_Import(pName);
	Py_DECREF(pName);

	if (pModule != NULL) {
		appLog(LOG_DEBUG, "Loaded module successfully!!!");
		//get python function
		pFunc = PyObject_GetAttrString(pModule, FTP_GET_FILE_FUNC);
		//checking pFunc is callable
		if (pFunc && PyCallable_Check(pFunc)) {
			appLog(LOG_DEBUG, "Checking function is callable!!!");
			//create arg list with n member
			pArgs = PyTuple_New(numargs);
			//convert arg from C to Python
			for (i = 0; i < numargs; i++) {

				switch (i) {
				case 0:
					pValue = PyString_FromString(FtpServerIP);
					break;
				case 1:
					pValue = PyString_FromString(FileName);
					break;
				case 2:
					pValue = PyString_FromString(UserName);
					break;
				case 3:
					pValue = PyString_FromString(Password);
					break;
				}
				if (!pValue) {
					Py_DECREF(pArgs);
					Py_DECREF(pModule);
					printf("cannot convert argument\n");
					return FILE_ERROR;
				}
				//set arg to python reference to pValue
				PyTuple_SetItem(pArgs, i, pValue);
			}
			appLog(LOG_DEBUG, "Calling python function!!!");
			//call Python Func
			pValue = PyObject_CallObject(pFunc, pArgs);
			appLog(LOG_DEBUG, "python function script done!!!");
			Py_DECREF(pArgs);
			if (pValue != NULL) {
				appLog(LOG_DEBUG, "Result of Python call: %ld\n",
						PyInt_AsLong(pValue));
				Py_DECREF(pValue);
			} else {
				Py_DECREF(pFunc);
				Py_DECREF(pModule);
				PyErr_Print();
				appLog(LOG_DEBUG, "python script falied!!!");
				return FILE_ERROR;
			}

		} else {
			if (PyErr_Occurred()) {
				PyErr_Print();
			} else {
				printf("cannot find function \'%s\'\n", FTP_GET_FILE_FUNC);
				return FILE_ERROR;
			}
		}
		Py_XDECREF(pFunc);
		Py_DECREF(pModule);
	} else {
		PyErr_Print();
		printf("failed to load %s\n", PY_MODULE);
		return FILE_ERROR;
	}
	Py_Finalize();
	return FILE_SUCCESS;
}

int getFile(char *message) {

	int ret;
	char *pfile_name;
	char *msg_id;
	char *resp_for;

	msg_id = getXmlElementByName(message, "id");
	resp_for = getXmlElementByName(message, "command");

	/*	if (!pftp_addr) {
	 appLog(LOG_DEBUG, "get ftp address failed");
	 free(pftp_addr);
	 ret = FILE_ERROR;
	 }*/

	pfile_name = getXmlElementByName(message, "filename");
	if (strlen(pfile_name) <= 0) {
		appLog(LOG_DEBUG, "get ftp address failed");
//		free(pftp_addr);
		free(pfile_name);
		ret = FILE_ERROR;
	}

	appLog(LOG_DEBUG, " %s %s %s %s",
			g_ServerInfo.ftp.Ip, pfile_name, g_ServerInfo.ftp.User, g_ServerInfo.ftp.Password);
	ret = getFileFromFtp(g_ServerInfo.ftp.Ip, pfile_name, g_ServerInfo.ftp.User,
			g_ServerInfo.ftp.Password);
	ret = sendResultResponse(msg_id, resp_for, ret, pfile_name);

//	free(pftp_addr);
	free(pfile_name);
	free(msg_id);
	free(resp_for);

	return ret;
}

/*if the config file is not exist, this function will be called at starting app.
 * the config will init with default configuration
 */
int createDefaultConfigFile(char *mac_addr) {

	config_t cfg;
	config_setting_t *root, *group, *setting;
	char device_name[128]; //assign MAC address
	FILE *config_file;

	config_file = fopen(DEFAULT_CONFIG_PATH, "w");
	if (config_file == NULL) {
		appLog(LOG_DEBUG, "create config file failed");
		return FILE_ERROR;
	}

	config_init(&cfg);

	root = config_root_setting(&cfg);

	group = config_setting_add(root, "mbox_config", CONFIG_TYPE_GROUP);

	setting = config_setting_add(group, "config_flag", CONFIG_TYPE_INT); //0-default,1-usr_cfg
	config_setting_set_int(setting, 0);

	setting = config_setting_add(group, "deviceID", CONFIG_TYPE_STRING); //for future feature
	config_setting_set_string(setting, mac_addr);

	snprintf(device_name, 128, "MBox.%s", mac_addr);
	setting = config_setting_add(group, "deviceName", CONFIG_TYPE_STRING);
	config_setting_set_string(setting, (char *) device_name);

	setting = config_setting_add(group, "station", CONFIG_TYPE_STRING);
	config_setting_set_string(setting, (char *) STATION_DF_ADDR);

	config_write(&cfg, config_file);
	config_destroy(&cfg);
	fclose(config_file);

	return FILE_SUCCESS;
}

//if new_value parameter is NULL, this function will set device name to default value
int changeConfigSetting(char *setting_name, char *new_value) {

	config_t cfg;
	config_setting_t *root, *setting;
	char setting_path[128];
	int ret = FILE_SUCCESS;

	config_init(&cfg);

	if (config_read_file(&cfg, DEFAULT_CONFIG_PATH) == CONFIG_FALSE) {
		appLog(LOG_DEBUG, "%d - %s",
				config_error_line(&cfg), config_error_text(&cfg));
		ret = FILE_ERROR;
	} else {

		snprintf(setting_path, 128, "%s.%s", "mbox_config", setting_name);
		setting = config_lookup(&cfg, setting_path);
		if (setting == NULL) {
			appLog(LOG_DEBUG, "config setting %s not found!", setting_name);
			ret = FILE_ERROR;
		} else {
			if (config_setting_set_string(setting, new_value) == CONFIG_FALSE) {
				appLog(LOG_DEBUG, "set setting value failed!");
				ret = FILE_ERROR;
			} else {
				config_write_file(&cfg, DEFAULT_CONFIG_PATH);
			}
		}
	}
	config_destroy(&cfg);
	return ret;
}

//read device config from configuration file then store in global struct
void initDeviceInfo(char *mac_addr) {

	config_t cfg;
	config_setting_t *setting;
	const char *device_id, *device_name, *station;

	//create config file if it not exist
	if (mac_addr != NULL) {
		if (createDefaultConfigFile(mac_addr) == FILE_ERROR) {
			snprintf(g_device_info.device_name, 128, "MBox.%s", mac_addr);
			return;
		}
	}

	config_init(&cfg);

	if (config_read_file(&cfg, DEFAULT_CONFIG_PATH) == CONFIG_FALSE) {
		//set default info
		strcpy(g_device_info.device_name, "mbox.MAC");
		strcpy(g_device_info.device_id, "12345");
		config_destroy(&cfg);
		return;
	}

	setting = config_lookup(&cfg, "mbox_config");
	if (setting != NULL) {
		if (config_setting_lookup_string(setting, "deviceID",
				&device_id)!= NULL) {
			strcpy(g_device_info.device_id, device_id);
		} else {
			strcpy(g_device_info.device_id, "12345");
		}
		if (config_setting_lookup_string(setting, "deviceName",
				&device_name) != NULL) {
			strcpy(g_device_info.device_name, device_name);
		} else {
			strcpy(g_device_info.device_name, "mbox.MAC");
		}
		if (config_setting_lookup_string(setting, "station", &station) != NULL) {
			strcpy(g_device_info.station_addr, station);
		} else {
			strcpy(g_device_info.station_addr, STATION_DF_ADDR);
		}
		strcpy(g_device_info.address, interface_addr);
	}
	return;
}
