/*
 * receiveFile_socket.c
 *
 *  Created on: Jul 9, 2014
 *      Author: d-dragon
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <ifaddrs.h>
#include <semaphore.h>
#include "sock_infra.h"
#include "logger.h"

/*************************************************
 * open stream socket just for listen connection,
 * not include accept() and communication task
 *************************************************/
int openStreamSocket() {

	/*Open server socket*/
	stream_sock_fd = socket(PF_INET, SOCK_STREAM, 0);
	if (stream_sock_fd < 0) {
		appLog(LOG_ERR, "call socket() error:\n");
		return SOCK_ERROR;
	} else {
		appLog(LOG_DEBUG, "Open Sream socket success\n");
	}

	/* Initialize socket structure */
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_port = htons(TCP_PORT);
	serv_addr.sin_family = PF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	socklen = sizeof(remote_addr);

	/*bind socket address to server socket*/
	ret = bind(stream_sock_fd, (struct sockaddr *) &serv_addr,
			sizeof(serv_addr));
	if (ret < 0) {
		appLog(LOG_ERR, "call bind() error!\n");
		return SOCK_ERROR;
	} else {
		appLog(LOG_DEBUG, "bind socket success!\n");
	}
	/* Now start listening for the clients, here
	 * process will go in sleep mode and will wait
	 * for the incoming connection
	 */
	ret = listen(stream_sock_fd, BACKLOG);
	if (ret < 0) {
		appLog(LOG_ERR, "call listen() error\n");
		return SOCK_ERROR;
	} else {
		appLog(LOG_DEBUG, "TCP socket is listening incoming connection at %s\n",
				interface_addr);
	}
	return SOCK_SUCCESS;
}

int openDatagramSocket() {

	int ret;
	broadcast_enable = 1;
	appLog(LOG_INFO, "Create UDP Broadcast socket......\n");

	datagram_sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (datagram_sock_fd < 0) {
		appLog(LOG_ERR, "Open UDP Broadcast socket failed\n");
		return SOCK_ERROR;
	} else {
		appLog(LOG_DEBUG, "Open UDP Broadcast socket success\n");
	}
	ret = setsockopt(datagram_sock_fd, SOL_SOCKET, SO_BROADCAST,
			&broadcast_enable, sizeof(broadcast_enable));
	memset(&udp_server_address, 0, sizeof(udp_server_address));

	//Config UDP Server sock address
	udp_server_address.sin_family = AF_INET;
	udp_server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	udp_server_address.sin_port = htons(atoi(UDP_PORT));
	appLog(LOG_DEBUG, "Config UDP Server sock address success\n");

	//Bind address to UDP server and listen for incoming client connection
	ret = bind(datagram_sock_fd, (struct sockaddr *) &udp_server_address,
			sizeof(struct sockaddr));
	if (ret < 0) {
		appLog(LOG_ERR, "bind address to UDP server socket failed!\n");
		return SOCK_ERROR;
	} else {
		appLog(LOG_DEBUG, "bind address to UDP server socket success!\n");
	}
	return SOCK_SUCCESS;
}

char *getInterfaceAddress() {

	int ret;
	interface_addr = calloc(32, sizeof(char));

	/*get all interface info*/
	if (getifaddrs(&ifaddr) == -1) {
		appLog(LOG_ERR, "get interface address failed\n");
		return NULL;
	}
	/* Walk through linked list, maintaining head pointer so we
	 can free list later */
	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL)
			continue;
		/*just only get AF_NET interface address*/
		if (ifa->ifa_addr->sa_family == AF_INET) {
			ret = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
					interface_addr, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
			if (ret == 0) {
				appLog(LOG_DEBUG, "address of %s: %s\n",
						ifa->ifa_name, interface_addr);
				if (strncmp(LOOPBACK_DEFAULT, interface_addr,
						sizeof(LOOPBACK_DEFAULT)) == 0) {
					continue;
				}
				return interface_addr;
			}
		}

	}
	return NULL;
}

int openMulRecvSocket() {

	int mul_fd;

	mul_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (mul_fd < 0) {
		appLog(LOG_DEBUG, "Creating multicast socket failed\n");
		close(mul_fd);
		return SOCK_ERROR;
	} else {
		appLog(LOG_DEBUG, " open multicast socket success\n");
	}
	int reuse = 1; //multiple applications can use one port on datagram local addr
	if (setsockopt(mul_fd, SOL_SOCKET, SO_REUSEADDR, (char *) &reuse,
			sizeof(reuse)) < 0) {
		appLog(LOG_DEBUG, "setting REUSEADDR failed\n");
		close(mul_fd);
		return SOCK_ERROR;
	}

	memset(&mul_sock, 0, sizeof(mul_sock));
	mul_sock.sin_family = AF_INET;
	mul_sock.sin_port = htons(5102);
	mul_sock.sin_addr.s_addr = inet_addr(MULTICAST_ADDR); //INADDR_ANY

	if (bind(mul_fd, (struct sockaddr*) &mul_sock, sizeof(mul_sock))) {

		appLog(LOG_DEBUG, "binding mul socket failed\n");
		close(mul_fd);
		return SOCK_ERROR;
	}

	/* join the multicast to group 239.255.1.111 on local address.
	 * Note that this IP_ADD_MEMBERSHIP option must be */
	/* called for each local interface over which the multicast */
	/* datagrams are to be received.
	 */
	u_char loop = 0;
	mul_group.imr_multiaddr.s_addr = inet_addr((char *) MULTICAST_ADDR);
	mul_group.imr_interface.s_addr = inet_addr(interface_addr);
	if (setsockopt(mul_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mul_group,
			sizeof(mul_group))) {
		appLog(LOG_DEBUG, "joing multicast group %s on the %s failed\n",
				MULTICAST_ADDR, interface_addr);
		close(mul_fd);
		return SOCK_ERROR;
	} else {
		setsockopt(mul_fd, IPPROTO_IP, IP_MULTICAST_LOOP, &mul_group,sizeof(mul_group));
		appLog(LOG_DEBUG, "joined multicast group %s on the %s succes\n",
				MULTICAST_ADDR, interface_addr);
	}

	return mul_fd;
}

int sendMulMessage(char *message) {

	struct in_addr localInterface;
	struct sockaddr_in groupSock;
	int fd;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		appLog(LOG_DEBUG, "open socket failed");
		return -1;
	}

	memset((char *) &groupSock, 0, sizeof(groupSock));
	groupSock.sin_family = AF_INET;
	groupSock.sin_addr.s_addr = inet_addr("239.255.1.111");
	groupSock.sin_port = htons(5102);

	localInterface.s_addr = inet_addr(interface_addr);
	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, (char *) &localInterface,
			sizeof(localInterface)) < 0) {
		perror("Setting local interface error");
		exit(1);
	} else
		printf("Setting the local interface...OK\n");

	/*char loopch = 0;
	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, (char *) &loopch,
			sizeof(loopch)) < 0) {
		perror("Setting IP_MULTICAST_LOOP error");
		close(fd);
		exit(1);
	} else {
		printf("Disabling the loopback...OK.\n");
	}*/
	if (sendto(fd, message, strlen(message), 0, (struct sockaddr*) &groupSock,
			sizeof(groupSock)) < 0) {
		perror("Sending datagram message error");
		return -1;
	} else {
		printf("Sending datagram message...OK\n");
		close(fd);
		return 0;
	}


}

/*
 * connect to tcp socket
 * addr: remote address
 * port: connection port
 * return: socket file descriptor
 */
int connecttoStreamSocket(char *addr, char *port) {

	int sd_sock;
	int res;
	struct sockaddr_in serv_addr;
	long sock_mode;
	fd_set myset; 
	struct timeval tv; 
  	int valopt; 
	socklen_t lon;

	appLog(LOG_DEBUG, "[debug]");
	sd_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sd_sock < 0) {
		appLog(LOG_DEBUG, "open socket failed!");
		return SOCK_ERROR;
	}

	memset(&serv_addr, 0x00, sizeof(struct sockaddr_in));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(port));
	serv_addr.sin_addr.s_addr = inet_addr(addr);
	//set non-blocking 
	// Set non-blocking 
  	if( (sock_mode = fcntl(sd_sock, F_GETFL, NULL)) < 0) { 
     		fprintf(stderr, "Error fcntl(..., F_GETFL) (%s)\n", strerror(errno)); 
     		exit(EXIT_FAILURE); 
  	} 
	sock_mode |= O_NONBLOCK; 
  	if( fcntl(sd_sock, F_SETFL, sock_mode) < 0) { 
     		fprintf(stderr, "Error fcntl(..., F_SETFL) (%s)\n", strerror(errno)); 
     		exit(EXIT_FAILURE); 
  	} 

	appLog(LOG_DEBUG, "[debug]");
	if (connect(sd_sock, (struct sockaddr*) &serv_addr, sizeof(serv_addr))
			< 0) {
		if (errno == EINPROGRESS) {
			appLog(LOG_DEBUG, "EINPROGRESS");
			do { 
           		tv.tv_sec = 15; 
		        tv.tv_usec = 0; 
           		FD_ZERO(&myset); 
           		FD_SET(sd_sock, &myset); 
           		res = select(sd_sock+1, NULL, &myset, NULL, &tv); 
           		if (res < 0 && errno != EINTR) { 
              			fprintf(stderr, "Error connecting %d - %s\n", errno, strerror(errno)); 
              			return SOCK_ERROR;
           		} else if (res > 0) { 
              			// Socket selected for write 
              			lon = sizeof(int); 
              			if (getsockopt(sd_sock, SOL_SOCKET, SO_ERROR, (void*)(&valopt), &lon) < 0) { 
                 			fprintf(stderr, "Error in getsockopt() %d - %s\n", errno, strerror(errno)); 
                 			return SOCK_ERROR;
              			} 
              			// Check the value returned... 
              			if (valopt) { 
                 			fprintf(stderr, "Error in delayed connection() %d - %s\n", valopt, strerror(valopt)); 
			                return SOCK_ERROR; 
              			} 
              			break; 
           		} else { 
              			fprintf(stderr, "Timeout in select() - Cancelling!\n"); 
              			return SOCK_ERROR;
           		} 
        		} while (1);
		}else{
			appLog(LOG_DEBUG, "connect to station failed");
			return SOCK_ERROR;
		}
	}

	// Set to blocking mode again... 
  	if( (sock_mode = fcntl(sd_sock, F_GETFL, NULL)) < 0) { 
     		fprintf(stderr, "Error fcntl(..., F_GETFL) (%s)\n", strerror(errno)); 
     		return SOCK_ERROR;
  	} 
  	sock_mode &= (~O_NONBLOCK); 
  	if( fcntl(sd_sock, F_SETFL, sock_mode) < 0) { 
     		fprintf(stderr, "Error fcntl(..., F_SETFL) (%s)\n", strerror(errno)); 
     		return SOCK_ERROR;
  	} 
	return sd_sock;
}

