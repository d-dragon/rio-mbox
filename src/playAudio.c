#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "playAudio.h"
#include "logger.h"
#include "FileHandler.h"
#include "acpHandler.h"

void *playMediaThread(void *arg);

#ifdef AUDIO_ENABLE
// Play function use to play a mp3Player instance
// To use this function, must have one mp3Player to call play
// In this function, create a stream to write and play mp3 file
// How to use: 
// Ex: 	
//		mp3Player* player;
//		play(player);
int play(mp3Player* player)
{
	player->play = true;

	player->mherr=0;
	printf("\n Open:%s\n",player->fileName);

	mpg123_init();
	player->mh = mpg123_new(NULL, &(player->mherr));
	player->buffer_size = mpg123_outblock(player->mh);
	player->buffer = (unsigned char*) malloc((player->buffer_size) * sizeof(unsigned char));

	/* open the file and get the decoding format */
	mpg123_open(player->mh, player->fileName);
	mpg123_getformat(player->mh, &(player->rate), &(player->channels), &(player->encoding));

	printf("\n%d encoding %d samplerate %d channels\n", player->encoding,
			player->rate, player->channels);

	/* init portaudio */
	player->err = Pa_Initialize();
	error_check(player->err);

	/* we are using the default device */
	player->out_param.device = Pa_GetDefaultOutputDevice();
	if (player->out_param.device == paNoDevice)
	{
		fprintf(stderr, "Haven't found an audio device!\n");
		return -1;
	}

	/* stero or mono */
	player->out_param.channelCount = player->channels;
	player->out_param.sampleFormat = paInt16;
	player->out_param.suggestedLatency = Pa_GetDeviceInfo(player->out_param.device)->defaultHighOutputLatency;
	player->out_param.hostApiSpecificStreamInfo = NULL;

	appLog(LOG_DEBUG,"checking error");
	player->err = Pa_OpenStream(&(player->stream), NULL, &(player->out_param), player->rate,
			paFramesPerBufferUnspecified, paClipOff,NULL, NULL);
	error_check(player->err);

	player->err = Pa_StartStream(player->stream);
	error_check(player->err);

	//err = Pa_SetStreamFinishedCallback(stream, &end_cb);
	//error_check(err);

	/* decode and play */

	if(g_audio_flag == AUDIO_STOP) {
		appLog(LOG_DEBUG,"\n Playing %s .....",player->fileName);
		pthread_mutex_lock(&g_audio_status_mutex);
		g_audio_flag = AUDIO_PLAY;
		pthread_mutex_unlock(&g_audio_status_mutex);
	}

	while (player->play)
	{

		while(g_audio_flag == AUDIO_PAUSE) {
//			appLog(LOG_DEBUG,"\n Pausing %s .....",player->fileName);
			usleep(10000);
		}

		//check stop flag
		if(g_audio_flag == AUDIO_STOP) {
			break;
		}
		if(mpg123_read(player->mh, player->buffer, player->buffer_size, &player->done) == MPG123_OK)
		{
			Pa_WriteStream(player->stream, player->buffer,(player->done)/4);
		} else
		{

			/*			pthread_mutex_lock(&g_audio_status_mutex);
			 g_audio_flag = AUDIO_ERROR; //audio monitor will notify this play audio error
			 pthread_mutex_unlock(&g_audio_status_mutex);*/
			pthread_mutex_lock(&g_audio_status_mutex);
			g_audio_flag = AUDIO_STOP;
			pthread_mutex_unlock(&g_audio_status_mutex);
			break;
		}
	}
	//return for stop to release memory
	if(!player->play)
	return 1;

	player->err = Pa_StopStream(player->stream);
	error_check(player->err);

	player->err = Pa_CloseStream(player->stream);
	error_check(player->err);

	free(player->buffer);
	//ao_close(dev);
	mpg123_close(player->mh);
	mpg123_delete(player->mh);
	mpg123_exit();

	Pa_Terminate();
	appLog(LOG_DEBUG,"\n Finish playing !!!!!!\n");
	return 0;
}

// stop function will end stream and close portaudio and mpg123 
// when mp3 is playing, call stop() and as play(), its only argument is a mp3Player object
// How to use:
// Ex:
// mp3Player* player;
// play(player);
// stop(player);
int stop(mp3Player* player)
{
	printf("\n End of playing \n");
	player->play = false;
	player->err = Pa_StopStream(player->stream);
	error_check(player->err);
	player->err = Pa_CloseStream(player->stream);
	error_check(player->err);

	free(player->buffer);

	mpg123_close(player->mh);
	mpg123_delete(player->mh);
	mpg123_exit();

	Pa_Terminate();

	return 0;
}
#endif

void *playAudioThread(void *arg) {

	int status;
	appLog(LOG_DEBUG, "inside playAudioThread..........");
	char *filename = arg;

//	appLog(LOG_DEBUG, "address filename: %p||address arg: %p", filename, arg);
	char *FilePath;
	FilePath = malloc(FILE_PATH_LEN_MAX);
	if (!FilePath) {
		appLog(LOG_DEBUG, "allocated memory failed, thread %d exited",
				(int)pthread_self());
		/*notify to client*/
		pthread_exit(NULL);
	}
	memset(FilePath, 0, FILE_PATH_LEN_MAX);
//	g_audio_flag = 0;//flag for stop play
	appLog(LOG_DEBUG, "File to play: %s", filename);
	strcat(FilePath, (char *) DEFAULT_PATH);
	strcat(FilePath, filename);

	appLog(LOG_DEBUG, "File Path: %s", FilePath);

#ifdef AUDIO_ENABLE
	mp3Player *player = malloc(sizeof(mp3Player));
	player->fileName = malloc(1024 * sizeof(char));

	player->fileName = FilePath;
	status = play(player);
	free(player);
#else
	g_audio_flag = AUDIO_PLAY;
	appLog(LOG_DEBUG, "pi is playing %s", filename);
#endif
	appLog(LOG_DEBUG, "deallocating memory");
	memset(g_file_name_playing, 0x00, 128);

	if (status == 0) {
		sendPlayingStatusNotify(NULL, filename, 2, "Finished playing success!");
	} else {
		sendPlayingStatusNotify(NULL, filename, 2, "playing failed/stopped!");
	}
	free(arg);
	free(FilePath);
	appLog(LOG_DEBUG, "exit playAudioThread..........");
	pthread_exit(NULL);
}

void *playAudioThreadAlt(void *arg) {

	PlayingInfo *info = arg;
	int count, status;
	char cmd_buf[256];
	appLog(LOG_DEBUG, "inside %s info->type %s", __FUNCTION__, info->type);
	//this code is temporary while have no complete message formation
	if (info->type == NULL) {
		//play audio
		appLog(LOG_DEBUG, "inside %s", __FUNCTION__);
		snprintf(cmd_buf, 256, "omxplayer -o local \"%s%s\" < %s", DEFAULT_PATH,
				info->filename, FIFO_PLAYER_PATH);
	} else {
		if (strcmp(info->type, "video") == 0) {
			//play video -> hdmi
			appLog(LOG_DEBUG, "inside %s", __FUNCTION__);
			snprintf(cmd_buf, 256, "omxplayer -o hdmi \"%s%s\" < %s",
					DEFAULT_PATH, info->filename, FIFO_PLAYER_PATH);
		} else {
			//play audio -> jack 3.5
			appLog(LOG_DEBUG, "inside %s", __FUNCTION__);
			snprintf(cmd_buf, 256, "omxplayer -o local \"%s%s\" < %s",
					DEFAULT_PATH, info->filename, FIFO_PLAYER_PATH);
		}
	}
	appLog(LOG_DEBUG, "play command: %s", cmd_buf);
	pthread_mutex_lock(&g_audio_status_mutex);
	g_audio_flag = AUDIO_PLAY;
	pthread_mutex_unlock(&g_audio_status_mutex);

	if ((status = system(cmd_buf)) == -1) {
		appLog(LOG_DEBUG, "play audio shell call failed");
	} else if (status == 0) {
		appLog(LOG_DEBUG, "play audio success");
		sendPlayingStatusNotify(NULL, info->filename, 2,
				"Finished playing success!");

	} else {
		appLog(LOG_DEBUG, "playing failed");
		sendPlayingStatusNotify(NULL, info->filename, 2,
				"playing failed/stopped!");
	}
	memset(g_file_name_playing, 0x00, FILE_NAME_MAX);
	free(info->filename);
	free(info->type);
	free(info);
	pthread_mutex_lock(&g_audio_status_mutex);
	g_audio_flag = AUDIO_STOP;
	pthread_mutex_unlock(&g_audio_status_mutex);
	appLog(LOG_DEBUG, "play audio thread exited");
	pthread_exit(NULL);
}

void *playMediaThread(void *arg) {

	PlayingInfo *info = arg;
	int count, status;
	char cmd_buf[256];
	appLog(LOG_DEBUG, "inside %s info->type %s", __FUNCTION__, info->type);
	//this code is temporary while have no complete message formation

	if (strcmp(info->type, "video") == 0) {
		//play video -> hdmi
		appLog(LOG_DEBUG, "inside %s", __FUNCTION__);
		snprintf(cmd_buf, 256, "omxplayer -o hdmi \"%s%s\"", DEFAULT_PATH,
				info->filename);
	} else {
		//play audio -> jack 3.5
		appLog(LOG_DEBUG, "inside %s", __FUNCTION__);
		snprintf(cmd_buf, 256, "omxplayer -o local \"%s%s\"", DEFAULT_PATH,
				info->filename);
	}
	appLog(LOG_DEBUG, "play command: %s", cmd_buf);
	pthread_mutex_lock(&g_audio_status_mutex);
	g_audio_flag = AUDIO_PLAY;
	pthread_mutex_unlock(&g_audio_status_mutex);

	if ((status = system(cmd_buf)) == -1) {
		appLog(LOG_DEBUG, "cannot create shell process");
	} else if (status == 0) {
		appLog(LOG_DEBUG, "play audio success");
		sendPlayingStatusNotify(NULL, info->filename, 2,
				"Finished playing success!");

	} else {
		appLog(LOG_DEBUG, "playing failed");
		sendPlayingStatusNotify(NULL, info->filename, 2,
				"playing failed/stopped!");
	}
	pthread_mutex_lock(&g_audio_status_mutex);
	g_audio_flag = AUDIO_STOP;
	pthread_mutex_unlock(&g_audio_status_mutex);
	appLog(LOG_DEBUG, "clean g_file_name_playing");
	memset(g_file_name_playing, 0x00, FILE_NAME_MAX);
	free(info->filename);
	free(info->type);

	appLog(LOG_DEBUG, "play audio thread exited");
	pthread_exit(NULL);
}

int initAudioPlayer(char *filename) {

	/*FileInfo *file;
	 file = malloc(sizeof(FileInfo));
	 file->filename = malloc(100);
	 file->filename = "m.mp3";
	 file->index = 0;*/
	appLog(LOG_DEBUG, "inside initAudio")
	char *FileName;
	FileName = calloc(FILE_NAME_MAX, sizeof(char));
	if (FileName == NULL) {
		appLog(LOG_DEBUG, "allocated memory failed");
		return ACP_FAILED;
	}
//	appLog(LOG_DEBUG, "address FileName: %p", FileName);
//	memset(FileName, 0, FILE_NAME_MAX);
	//  strlen -1 to truncate '|' charater at end of string
	appLog(LOG_DEBUG, "debug----");
	strncat(FileName, filename, strlen(filename)); //cann't assign FileName = "m.mp3", it change pointer address -> can't free()
	appLog(LOG_DEBUG, "debug----");
	/*Need to parse file index to get file name*/

	//init play audio thread
	pthread_mutex_lock(&g_audio_status_mutex);
	g_audio_flag = AUDIO_STOP;
	pthread_mutex_unlock(&g_audio_status_mutex);

	//FileName will be freed in playAudioThread
	if (pthread_create(&g_play_audio_thd, NULL, &playAudioThread,
			(void *) FileName)) {
		appLog(LOG_DEBUG, "init playAudioThread failed!!!");
		return ACP_FAILED;
	}
	return ACP_SUCCESS;
}

int initAudioPlayerAlt(PlayingInfo *info) {

	int count = 0;
	//delete existed fifo
	unlink(FIFO_PLAYER_PATH);
	mkfifo(FIFO_PLAYER_PATH, 0777);

	if (pthread_create(&g_play_audio_thd, NULL, &playAudioThreadAlt,
			(void *) info)) {
		appLog(LOG_DEBUG, "init playAudioThreadAlt failed!");
		return ACP_FAILED;
	}
	usleep(100000);
	do { // try to check audio flag for 0.5s
		 //sleep short period time for waiting play audio thread start
		usleep(100000);
		if (g_audio_flag == AUDIO_PLAY) {
			int cnt = 0;
			do {
				if (system("echo . > /tmp/omxplayer.fifo") != 0) {
					cnt++;
					if (cnt == 3) {
						//play failed
						pthread_cancel(g_play_audio_thd);
						return ACP_FAILED;
					}
					usleep(200000);
				} else {
					appLog(LOG_DEBUG, "play %s success!", info->filename);
					return ACP_SUCCESS;
				}
			} while (cnt < 3);
			break; //play success
		} else {
			count++;
			if (count == 5) {
				appLog(LOG_DEBUG, "play %s failed!", info->filename);
				pthread_cancel(g_play_audio_thd);
				return ACP_FAILED;
			}
		}
		//check
	} while (count < 5);

}

int initMediaPlayer(PlayingInfo *info) {

	int count = 0;
	char shell_cmd[256];

	if (pthread_create(&g_media_player_thd, NULL, &playMediaThread,
			(void *) info)) {
		free(info->filename);
		free(info->type);
		appLog(LOG_DEBUG, "init playAudioThreadAlt failed!");
		return ACP_FAILED;
	}
	do { // try to check audio flag for 0.5s
		 //sleep short period time for waiting play audio thread start
		usleep(100000);
		//check player started or not
		if (g_audio_flag == AUDIO_PLAY) {
			int count_fail = 0;
			snprintf(shell_cmd,256, "%s %s", PLAYER_CONTROLLER, "status");
			appLog(LOG_DEBUG, "shell cmd: %s", shell_cmd);
			do {
				//deep check player status
				if (system(shell_cmd) == 0) {
					//player started
					appLog(LOG_DEBUG, "start player success!");
					return ACP_SUCCESS;
				} else {
					//start player failed
					count_fail++;
					usleep(100000);
				}
			} while (count_fail < 10);
			appLog(LOG_DEBUG, "start player failed!");
			return ACP_FAILED;
		} else {
			count++;
			if (count == 5) {
				pthread_mutex_lock(&g_audio_status_mutex);
				g_audio_flag = AUDIO_PLAY;
				pthread_mutex_unlock(&g_audio_status_mutex);
				appLog(LOG_DEBUG, "play %s failed!", info->filename);
				pthread_cancel(g_play_audio_thd);
				return ACP_FAILED;
			}
		}
		//check
	} while (count < 5);

}
int stopAudio(char *message) {

	char *resp_for;
	char *msg_id;
	char shell_cmd[256];

	memset(shell_cmd, 0x00, 256);
	msg_id = getXmlElementByName(message, "id");
	resp_for = getXmlElementByName(message, "command");
	if (g_audio_flag == AUDIO_STOP) {
		sendResultResponse(msg_id, resp_for, ACP_SUCCESS, "Stopped");
	} else {
		snprintf(shell_cmd, 256, "echo -n q > %s", FIFO_PLAYER_PATH);
		if (system(shell_cmd) != 0) {
			pthread_cancel(g_play_audio_thd);
			pthread_mutex_lock(&g_audio_status_mutex);
			g_audio_flag = AUDIO_STOP;
			pthread_mutex_unlock(&g_audio_status_mutex);
			sendPlayingStatusNotify(NULL, g_file_name_playing, 2, "stopped!");
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
	free(resp_for);
	free(msg_id);
	return ACP_SUCCESS;
}

int pauseAudio(char *message) {

	char *resp_for;
	char *msg_id;
	char shell_cmd[256];

	memset(shell_cmd, 0x00, 256);
	msg_id = getXmlElementByName(message, "id");
	resp_for = getXmlElementByName(message, "command");
	snprintf(shell_cmd, 256, "echo -n p > %s", FIFO_PLAYER_PATH);
	appLog(LOG_DEBUG, "inside %s", __FUNCTION__);
	if (system(shell_cmd) == 0) {
		appLog(LOG_DEBUG, "inside %s", __FUNCTION__);
		sendResultResponse(msg_id, resp_for, ACP_SUCCESS, g_file_name_playing);
		pthread_mutex_lock(&g_audio_status_mutex);
		appLog(LOG_DEBUG, "inside %s", __FUNCTION__);
		g_audio_flag = AUDIO_PAUSE;
		pthread_mutex_unlock(&g_audio_status_mutex);
	} else {
		sendResultResponse(msg_id, resp_for, ACP_FAILED, g_file_name_playing);
	}
	free(msg_id);
	free(resp_for);
	return ACP_SUCCESS;
}
//#endif
