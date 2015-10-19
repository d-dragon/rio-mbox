
#ifndef __PLAY_AUDIO_H__
#define __PLAY_AUDIO_H__

#ifdef AUDIO_ENABLE
#include <portaudio.h>
#include <mpg123.h>
#endif

#include <stdio.h>
#include <pthread.h>

#define FILE_NAME_MAX 256
#define FIFO_PLAYER_PATH "/tmp/omxplayer.fifo"
#define PLAYER_CONTROLLER "omxplayer_dbus_control.sh"
#define error_check(err) \
    do {\
        if (err) { \
            fprintf(stderr, "line %d ", __LINE__); \
            fprintf(stderr, "error number: %d\n", err); \
            fprintf(stderr, "\n\t%s\n\n", Pa_GetErrorText(err)); \
            return err; \
        } \
    } while (0)

typedef int bool;
#define true 1
#define false 0

typedef enum g_audio_status{
	AUDIO_STOP,
	AUDIO_PLAY,
	AUDIO_PAUSE,
	AUDIO_ERROR
}g_audio_status;

#ifdef AUDIO_ENABLE
typedef struct player
{
	PaStreamParameters out_param;
    PaStream * stream;
    PaError err;
   
    mpg123_handle *mh;
    unsigned char *buffer;
    size_t buffer_size;
    size_t done;
    int mherr;
   
    int channels, encoding;
    long rate;
	char* fileName;
	
	bool play;
}mp3Player;
#endif

typedef struct playinfo{
	char *filename;
	char *type; //audio/video
	int length; //by second
}PlayingInfo;
pthread_t g_play_audio_thd;
pthread_t g_media_player_thd;
pthread_mutex_t g_audio_status_mutex;
g_audio_status g_audio_flag;
char *g_file_name_playing;

#ifdef AUDIO_ENABLE
int play(mp3Player* player);// play and stop an mp3Player play(player);see ex in main function
int stop(mp3Player* player);//
#endif

int playAudio(char *message);
int playAudioAlt(char *message);

int initAudioPlayer(char *filename);
int initAudioPlayerAlt(PlayingInfo *info);
int initMediaPlayer(PlayingInfo *info);
void *playAudioThread(void *);
void *playAudioThreadAlt(void *);
int stopAudio(char *message);
int pauseAudio(char *message);

#endif

