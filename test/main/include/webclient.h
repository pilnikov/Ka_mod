/*
 * Copyright 2016 karawin (http://www.karawin.fr)
*/
#ifndef __WEBCLIENT_H__
#define __WEBCLIENT_H__
#include "esp_system.h"
#include "esp_log.h"
//#include "websocket.h"

#define METADATA 9
#define METAINT 8
#define BITRATE 5
#define METANAME 0
#define METAGENRE 4
#define ICY_HEADERS_COUNT 9
#define ICY_HEADER_COUNT 10
//2000 1440 1460
#define RECEIVE 1436
//#define RECEIVE 3000


typedef enum
{
    KMIME_UNKNOWN = 1, KOCTET_STREAM, KAUDIO_AAC, KAUDIO_MP4, KAUDIO_MPEG
} contentType_t;

struct shoutcast_info {
	char domain[73]; //url
	char file[116];  //path
	char name[64];
	uint16_t port;	//port
};

struct icyHeader
{
	union
	{
		struct
		{
			char* name;
			char* notice1;
			char* notice2;
			char* url;
			char* genre;
			char* bitrate;
			char* description;
			char* audioinfo;
			int metaint;
			char* metadata;
		} single;
		char* mArr[ICY_HEADER_COUNT];
	} members;
};


enum clientStatus {C_HEADER0, C_HEADER, C_HEADER1,C_METADATA, C_DATA, C_PLAYLIST, C_PLAYLIST1 };

void clientInit();
uint8_t clientIsConnected();
bool clientParsePlaylist(char* s);
void clientSetURL(char* url);
void clientSetName(const char* name,uint16_t index);
void clientSetPath(char* path);
void clientSetPort(uint16_t port);
void clientPrintState();
bool getState();
char* getMeta();

void clientConnect();
void clientSilentConnect(); 
void clientConnectOnce();
void clientDisconnect(const char* from);
void clientSilentDisconnect();
void clientTask(void *pvParams);
void playStationInt(int sid);
#endif
