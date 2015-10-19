#include "sock_infra.h"
//#include "FileHandler.h"
#include "advertisement.h"
#include "logger.h"
#include "acpHandler.h"
#include <pthread.h>
#include <syslog.h>
#include <string.h>

#ifdef AUDIO_ENABLE
#include "playAudio.h"
#endif

#define APP_SUCCESS 1
#define APP_ERROR 0

int main(int argc, char *argv[]) {

/*
#ifdef AUDIO_ENABLE
	int status;
	 mp3Player* player = malloc(sizeof(mp3Player));

	 //player->fileName = malloc(strlen(argv[1]) * sizeof(char));
	 player->fileName = malloc(1024 * sizeof(char));

	 player->fileName = ("/home/pi/Audio/Test1.mp3");

	 //player->fileName = argv[1];

	 status = play(player);
	 free(player);
#endif
*/

//	pthread_t acp_thread;
//	pthread_t adv_info_thread;
	int ret;

	/*call for init Logger*/
	initLogger();

	/*get network interface address*/
	getInterfaceAddress();
	initDeviceInfo(argv[1]);
	TaskReceiver();
//	startMulticastListener(argv[1]);


	/*init socket semaphore*/
/*	ret = sem_init(&sem_sock, 0, 0);
	if (ret != 0) {
		appLog(LOG_ERR, "sock semaphore init failed\n");
	} else {
		appLog(LOG_DEBUG, "sock semaphore was inittialized success\n");
	}

	//create receive file thread
	appLog(LOG_DEBUG, "create recv file thread\n");
	ret = pthread_create(&acp_thread, NULL, &waitingConnectionThread,
			(void *) "receive file thread!\n");
	if (ret) {
		appLog(LOG_ERR,
				"pthread_create %d (failed) while create recv file thread\n",
				ret);
		exit(EXIT_FAILURE);
	} else {
		appLog(LOG_DEBUG, "receive file thread created success\n");
	}

	//create advertise server info thread
	appLog(LOG_DEBUG, "create adv thread\n");
	ret = pthread_create(&adv_info_thread, NULL, &advertiseServerInfoThread, NULL);
	if (ret) {
		appLog(LOG_ERR, "pthread_create %d (failed) while create adv thread\n",
				ret);
		exit(EXIT_FAILURE);
	}

	pthread_join(acp_thread, NULL);
	pthread_join(adv_info_thread, NULL);
	return APP_SUCCESS;*/
}
