/*
 * stationsimulator.c
 *
 *  Created on: Oct 1, 2015
 *      Author: duyphan
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sock_infra.h"

/*
<?xml version="1.0" encoding="UTF-8" standalone="yes"?><message><id>[ID]</id><deviceid>B827EB7A7169</deviceid><type>request</type><action><command>DeleteFile</command><filename>Hard Drive Design and Operation - ACS Data Recovery.MP4</filename></action></message>

<?xml version="1.0" encoding="UTF-8" standalone="yes"?><message><id>39</id><type>request</type><action xsi:type="mediaAction" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"><command>PlayFile</command><filename>Chenh Venh Guitar Version_ - Le Cat Tron [MP3 320kbps].mp3</filename><type>audio</type><volume>0.0</volume></action></message>

<?xml version="1.0" encoding="UTF-8" standalone="yes"?><message><id>24</id><type>request</type><action xsi:type="mediaAction" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"><command>StopFile</command><filename></filename><type></type><volume>0.0</volume></action></message>

<message><id>123</id><deviceid>MBox.B827EB7A7169</deviceid><type>request</type><action><command>SetBinary</command><value>0</value></action></message>
<message><id>123</id><deviceid>MBox.B827EB7A7169</deviceid><type>request</type><action><command>SetBinary</command><value>1</value></action></message>
<message><id>123</id><deviceid>MBox.B827EB7A7169</deviceid><type>request</type><action><command>GetBinary</command></action></message>]
 */


int main(int argc, char *argv[]) {

	int ret;
	char *remote_address;
	getInterfaceAddress();
	ret = openStreamSocket();
	if (ret == SOCK_SUCCESS) {
		printf("TCP socket successfully opened--------\n");
		printf("waiting for new connection!\n");
	} else {
		printf("TCP socket open failed\n");
	}

	while (1) {
		child_stream_sock_fd = accept(stream_sock_fd,
				(struct sockaddr *) &remote_addr, &socklen);
		if (child_stream_sock_fd < 0) {
			printf("accept call error\n");
			exit(0);
			continue;
		}
		remote_address = inet_ntoa(remote_addr.sin_addr);
		printf("server: got connection from %s\n", remote_address);
		pid_t pid = fork();
		switch (pid) {
		case 0:
			recvnhandlePackageLoop();
			break;
		default:
			printf("parent process runing from here\n");
			break;
		}
	}
}

void recvnhandlePackageLoop() {

	int ret;
	char *msg_str;
	char response_msg[1024];


	msg_str = calloc(1024, sizeof(char));

	ret = recv(child_stream_sock_fd,response_msg, 1024,0);
	printf("notify message>>>\n%s\n", response_msg);
	while (1) {
		memset(msg_str, 0x00, 1024);
		printf("message>>>  ");
		gets(msg_str);

		ret = send(child_stream_sock_fd, msg_str, strlen(msg_str), 0);
		if(ret > 0){
			ret = recv(child_stream_sock_fd,response_msg, 1024,0);
			if(ret > 0 ){
				printf("\nresponse message>>>\n%s\n", response_msg);
			}else if (ret == 0){
				break;
			}
		}

	}
	exit(0);
}
