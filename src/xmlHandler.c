/*
 * xmlHandler.c
 *
 *  Created on: Feb 7, 2015
 *      Author: duyphan
 */

#include <stdio.h>
#include <strings.h>
#include <libxml2/libxml/parser.h>
#include <libxml2/libxml/tree.h>
#include <libxml2/libxml/xmlstring.h>
#include <libxml2/libxml/xmlmemory.h>
#include <libxml2/libxml/xmlwriter.h>
#include <libxml2/libxml/xmlIO.h>
#include <libxml2/libxml/encoding.h>
#include <libxml2/libxml/xmlversion.h>
#include "xmlHandler.h"
#include "logger.h"
#include "acpHandler.h"
#include "FileHandler.h"

char *findElement(xmlDocPtr doc, xmlNodePtr node, char *element_name);

/* Get xml message type
 * return: notify/request/response
 * xmlbuff: xml message content
 * must free return variable after using
 */
char *getXmlMessageType(char *xmlbuff) {

	xmlDocPtr xmldoc;
	xmlNodePtr xmlrootnode;
	xmlChar *xmlcontent;
	xmldoc = xmlReadMemory(xmlbuff, (int) strlen(xmlbuff), "tmp.xml", NULL, 0);

	if (xmldoc == NULL) {
		xmlFreeDoc(xmldoc);
		appLog(LOG_DEBUG, "xmlReadMemory failed");
		return NULL;
	}

	xmlrootnode = xmlDocGetRootElement(xmldoc);
	if (xmlrootnode == NULL) {
		xmlFreeDoc(xmldoc);
		appLog(LOG_DEBUG, "xmlDocGetRootElement failed", __FUNCTION__);
		return NULL;
	}
	if (strcmp((char *) xmlrootnode->name, (char *) "message")) {
		appLog(LOG_DEBUG, "message is invalid!!");
		return NULL;
	}
	xmlcontent = xmlNodeListGetString(xmldoc, xmlrootnode->children->children,
			1);
	xmlFreeDoc(xmldoc);

	return (char *) xmlcontent;

}

char *findElement(xmlDocPtr doc, xmlNodePtr node, char *element_name) {

	xmlChar *content = NULL;
	node = node->children;
	while (node != NULL) {
		if (strcmp((char *) node->name, element_name)) {
			if (node->children->children != NULL) {
				content = findElement(doc, node, element_name);
			}
		} else {
			content = xmlNodeListGetString(doc, node->children, 1);
//			result = (char *)content;
			break;
		}
		node = node->next;
	}
	return (char *) content;
}

/*
 * get element content by name
 * xmlbuff: xml content
 * name: name of element
 * return element content
 * must free pointer which point to return value
 */
char *getXmlElementByName(char *xmlbuff, char *name) {

	xmlDocPtr xmldoc;
	xmlNodePtr xmlrootnode;
	char *xmlcontent;
//	xmlcontent = malloc(512);
//	memset(xmlcontent, 0x00, 512);

	xmldoc = xmlReadMemory(xmlbuff, (int) strlen(xmlbuff), "tmp.xml", NULL, 0);
	if (xmldoc == NULL) {
		xmlFreeDoc(xmldoc);
		appLog(LOG_DEBUG, "xmlReadMemory failed");
		return NULL;
	}
	xmlrootnode = xmlDocGetRootElement(xmldoc);
	if (xmlrootnode == NULL) {
		xmlFreeDoc(xmldoc);
		appLog(LOG_DEBUG, "xmlDocGetRootElement failed", __FUNCTION__);
		return NULL;
	}
//	appLog(LOG_DEBUG, "root node name: %s", xmlrootnode->name);
	if (strcmp((char *) xmlrootnode->name, (char *) "message")) {
		appLog(LOG_DEBUG, "message is invalid!!");
		return NULL;
	}
	xmlcontent = findElement(xmldoc, xmlrootnode, name);
//	appLog(LOG_DEBUG, "%s: %s", name, xmlcontent);
	xmlFreeDoc(xmldoc);
//	xmlFreeNode(xmlrootnode);
	return xmlcontent;
}

/*must free return pointer*/
char *writeXmlToBuffResp(char *msg_id, char *resp_for, char *resp_code,
		char *att_info) {

	int ret;
	xmlTextWriterPtr writer;
	xmlBufferPtr buffer;
	char *tmp;

	tmp = calloc(BUFF_LEN_MAX, sizeof(char));

	/* Create a new XML buffer, to which the XML document will be
	 * written */
	buffer = xmlBufferCreate();
	if (buffer == NULL) {
		appLog(LOG_DEBUG, "create xml buffer failed");
		return NULL;
	}

	/* Create a new XmlWriter for memory, with no compression.
	 * Remark: there is no compression for this kind of xmlTextWriter */
	writer = xmlNewTextWriterMemory(buffer, 0);
	if (writer == NULL) {
		appLog(LOG_DEBUG, "create xml writer failed");
		return NULL;
	}

	/* Start the document with the xml default for the version,
	 * encoding ISO UTF-8 and the default for the standalone
	 * declaration. */
	ret = xmlTextWriterStartDocument(writer, NULL, XML_ENCODING, "yes");
	if (ret < 0) {
		appLog(LOG_DEBUG, "error at xmlTextWriterStartDocument");
		return NULL;
	}

	/*start root element*/
	ret = xmlTextWriterStartElement(writer, BAD_CAST "message");
	if (ret < 0) {
		appLog(LOG_DEBUG, "error at xmlTextWriterStartElement");
		return NULL;
	}

	ret = xmlTextWriterWriteElement(writer, BAD_CAST "id", msg_id);
	if (ret < 0) {
		appLog(LOG_DEBUG, "error in xmlTextWriterWriteElement");
		return NULL;
	}

	ret = xmlTextWriterWriteElement(writer, BAD_CAST "deviceid",
			(char *) g_device_info.device_id);
	if (ret < 0) {
		appLog(LOG_DEBUG, "error in xmlTextWriterWriteElement");
		return NULL;
	}

	/*start type elemnt*/
	ret = xmlTextWriterWriteElement(writer, BAD_CAST "type", "response");
	if (ret < 0) {
		appLog(LOG_DEBUG, "error in xmlTextWriterWriteElement");
		return NULL;
	}

	/*end type element*/
	/*	ret = xmlTextWriterEndElement(writer);
	 if (ret < 0) {
	 appLog(LOG_DEBUG, "error in xmlTextWriterEndElement");
	 return NULL;
	 }*/

	/*start type elemnt*/
	ret = xmlTextWriterStartElement(writer, BAD_CAST "content");
	if (ret < 0) {
		appLog(LOG_DEBUG, "error in xmlTextWriterWriteElement");
		return NULL;
	}
	ret = xmlTextWriterWriteElement(writer, BAD_CAST "responsefor", resp_for);
	if (ret < 0) {
		appLog(LOG_DEBUG, "error in xmlTextWriterWriteFormatComment");
		return NULL;
	}
	if (resp_code != NULL) {
		ret = xmlTextWriterWriteElement(writer, BAD_CAST "result", resp_code);
		if (ret < 0) {
			appLog(LOG_DEBUG, "error in xmlTextWriterWriteFormatComment");
			return NULL;
		}
	}
	if (att_info != NULL) {
		ret = xmlTextWriterWriteElement(writer, BAD_CAST "AttInfo", att_info);
		if (ret < 0) {
			appLog(LOG_DEBUG, "error in xmlTextWriterWriteFormatComment");
			return NULL;
		}
	}
	/*close the elements message and type*/
	ret = xmlTextWriterEndDocument(writer);
	if (ret < 0) {
		appLog(LOG_DEBUG, "error in xmlTextWriterEndDocument");
		return NULL;
	}
	strncpy(tmp, buffer->content, strlen(buffer->content));

//	tmp[strlen(tmp)]='\0';
	xmlFreeTextWriter(writer);
	xmlBufferFree(buffer);
//	appLog(LOG_DEBUG, "xml response:  %s", (char *) buffer->content);
	return tmp;
}

/*must free return pointer*/
char *writeXmlToBuffNotify(char *msg_id, NotifyPiStatus notify_status) {

	int ret;
	int i = 0;
	xmlTextWriterPtr writer;
	xmlBufferPtr buffer;
	char *tmp;

	tmp = calloc(BUFF_LEN_MAX, sizeof(char));

	/* Create a new XML buffer, to which the XML document will be
	 * written */
	buffer = xmlBufferCreate();
	if (buffer == NULL) {
		appLog(LOG_DEBUG, "create xml buffer failed");
		return NULL;
	}

	/* Create a new XmlWriter for memory, with no compression.
	 * Remark: there is no compression for this kind of xmlTextWriter */
	writer = xmlNewTextWriterMemory(buffer, 0);
	if (writer == NULL) {
		appLog(LOG_DEBUG, "create xml writer failed");
		return NULL;
	}

	/* Start the document with the xml default for the version,
	 * encoding ISO UTF-8 and the default for the standalone
	 * declaration. */
	ret = xmlTextWriterStartDocument(writer, NULL, XML_ENCODING, "yes");
	if (ret < 0) {
		appLog(LOG_DEBUG, "error at xmlTextWriterStartDocument");
		return NULL;
	}

	/*start root element*/
	ret = xmlTextWriterStartElement(writer, BAD_CAST "message");
	if (ret < 0) {
		appLog(LOG_DEBUG, "error at xmlTextWriterStartElement");
		return NULL;
	}

	ret = xmlTextWriterWriteElement(writer, BAD_CAST "id", msg_id);
	if (ret < 0) {
		appLog(LOG_DEBUG, "error in xmlTextWriterWriteElement");
		return NULL;
	}

	ret = xmlTextWriterWriteElement(writer, BAD_CAST "deviceid",
			(char *) g_device_info.device_id);
	if (ret < 0) {
		appLog(LOG_DEBUG, "error in xmlTextWriterWriteElement");
		return NULL;
	}

	/*start type elemnt*/
	ret = xmlTextWriterWriteElement(writer, BAD_CAST "type", "notify");
	if (ret < 0) {
		appLog(LOG_DEBUG, "error in xmlTextWriterWriteElement");
		return NULL;
	}

	ret = xmlTextWriterWriteElement(writer, BAD_CAST "info",
			notify_status.info);
	if (ret < 0) {
		appLog(LOG_DEBUG, "error in xmlTextWriterWriteElement");
		return NULL;
	}
	/*end type element*/
	/*	ret = xmlTextWriterEndElement(writer);
	 if (ret < 0) {
	 appLog(LOG_DEBUG, "error in xmlTextWriterEndElement");
	 return NULL;
	 }*/

	/*start type elemnt*/
	ret = xmlTextWriterStartElement(writer, BAD_CAST "content");
	if (ret < 0) {
		appLog(LOG_DEBUG, "error in xmlTextWriterWriteElement");
		return NULL;
	}

	for (i = 0; i < notify_status.num_content_tag; i++) {
		ret = xmlTextWriterWriteElement(writer,
				BAD_CAST notify_status.content_tag[i].ele_name,
				notify_status.content_tag[i].ele_content);
		if (ret < 0) {
			appLog(LOG_DEBUG, "error in xmlTextWriterWriteFormatComment");
			return NULL;
		}
	}
	/*	ret = xmlTextWriterWriteElement(writer,
	 BAD_CAST notify_status->obj_ele->ele_name,
	 notify_status->obj_ele->ele_content);
	 if (ret < 0) {
	 appLog(LOG_DEBUG, "error in xmlTextWriterWriteFormatComment");
	 return NULL;
	 }

	 ret = xmlTextWriterWriteElement(writer,
	 BAD_CAST notify_status->status_ele->ele_name,
	 notify_status->status_ele->ele_content);
	 if (ret < 0) {
	 appLog(LOG_DEBUG, "error in xmlTextWriterWriteFormatComment");
	 return NULL;
	 }*/
	/*close the elements message and type*/
	ret = xmlTextWriterEndDocument(writer);
	if (ret < 0) {
		appLog(LOG_DEBUG, "error in xmlTextWriterEndDocument");
		return NULL;
	}
	strncpy(tmp, buffer->content, strlen(buffer->content));

//	tmp[strlen(tmp)]='\0';
	xmlFreeTextWriter(writer);
	xmlBufferFree(buffer);
//	appLog(LOG_DEBUG, "xml response:  %s", (char *) buffer->content);
	return tmp;
}
