/*
 * logger.c
 *
 *  Created on: Jul 31, 2014
 *      Author: duyphan
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "logger.h"

void initLogger(){

	openlog("MEDIA_HUB", LOG_PID | LOG_CONS | LOG_NDELAY | LOG_PERROR, LOG_USER);
}

void closeLogger(){
	closelog();
}

/************************************************************************
 * Function: writeLog
 * 		Interface to be used to write app log to syslog.
 * 	Parameters:
 * 		logLevel - log Level
 * 		pLogStr - string to written to syslog
 * 		... - list variables
 ************************************************************************/
void writeLog(int logLevel, const char *pLogStr, ...){

	char log_info_buff[LOG_BUFF_LEN];
	va_list args; //variable arguments list

	memset(log_info_buff, 0, LOG_BUFF_LEN);

	va_start(args, pLogStr); //Initialize variables list after pLogStr argument
	vsnprintf(log_info_buff, sizeof(log_info_buff), pLogStr, args);
	va_end(args);

	syslog(logLevel,"%s \n", log_info_buff);
	return;
}
