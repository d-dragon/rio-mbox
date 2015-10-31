/*
 * xmlHandler.h
 *
 *  Created on: Feb 7, 2015
 *      Author: duyphan
 */

#ifndef XMLHANDLER_H_
#define XMLHANDLER_H_
#include "acpHandler.h"
#define XML_MESSGAE_TYPE		"type"
#define XML_MESSGAE_INFO		"info"
#define XML_MESSAGE_NOTIFY		"notify"
#define XML_MESSAGE_REQUEST 	"request"
#define XML_MESSAGE_COMMAND		"command"
#define XML_MESSAGE_RESPONSE	"response"
#define XML_ENCODING			"UTF-8"

//get content from xml doc by element name
char *getXmlMessageType(char *xmlbuff);
char *getXmlElementByName(char *xmlbuff, char *name);
char *writeXmlToBuffResp(char *msg_id, char *resp_for,char *resp_code, char *att_info);
char *writeXmlToBuffNotify(char *msg_id, NotifyPiStatus notify_status);


#endif /* XMLHANDLER_H_ */
