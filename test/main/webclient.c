/*
 * Copyright 2016 karawin (http://www.karawin.fr)
*/
#define TAG "webclient"

#include "webclient.h"
#include <string.h>
#include "lwip/sockets.h"
#include "lwip/api.h"
#include "lwip/netdb.h"
#include "esp_wifi.h"
#include "freertos/semphr.h"

#include "vs10xx.h"
#include "audio_player.h"
#include "spiram_fifo.h"
#include "app_main.h"
//#include "eeprom.h"

extern player_t* player_config;
#define min(a, b) (((a) < (b)) ? (a) : (b))

enum clientStatus cstatus;
//static uint32_t metacount = 0;
//static uint16_t metasize = 0;

xSemaphoreHandle sConnect, sConnected, sDisconnect, sHeader;

const char* sterr;


static uint8_t once = 0;
static uint8_t playing = 0;

static const char* icyHeaders[] = { "icy-name:", "icy-notice1:", "icy-notice2:", "icy-url:", "icy-genre:", "icy-br:", "icy-description:", "ice-audio-info:", "icy-metaint:" };
contentType_t contentType;

static char notfound[] = { "Not Found" };
static char nodata[] = { "No Data" };
static char parEmpty[] = { " " };
const char CLIPLAY[] = { "##CLI.PLAYING#%c%c" };
const char CLISTOP[] = { "##CLI.STOPPED# from %s\n" };

#define strcMALLOC "Client: incmalloc fails for %d"
#define strcMALLOC1 "%s malloc fails"

/* TODO:
	- METADATA HANDLING
	- IP SETTINGS
	- VS1053 - DELAY USING vTaskDelay
*/
static struct icyHeader header = { {{NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL}} };

static char metaint[10];
static char clientURL[256] = { 0, 0 };
static char clientPath[256] = { 0, 0 };
static uint16_t clientPort = 8000;

static struct hostent* server = NULL;

void* incmalloc(size_t n)
{
	void* ret;
	//printf ("Client malloc of %d %d,  Heap size: %d\n",n,((n / 32) + 1) * 32,xPortGetFreeHeapSize( ));
	ret = malloc(n);
	if (ret == NULL)
		ESP_LOGV(TAG, strcMALLOC, n);
	//	if (n <4) printf("Client: incmalloc size:%d\n",n);
	ESP_LOGV(TAG, "Client malloc after of %d bytes ret:%x  Heap size: %d", n, (int)ret, xPortGetFreeHeapSize());
	return ret;
}
void incfree(void* p, const char* from)
{
	if (p != NULL)
		free(p);
	//	else printf ("Client incfree from %s NULL\n",from);
	ESP_LOGV(TAG, "Client incfree of %x, from %s           Heap size: %d", (int)p, from, xPortGetFreeHeapSize());
}

bool getState()
{
	return playing;
}

void clientPrintState()
{
	if (playing)
		kprintf(CLIPLAY, 0x0d, 0x0a);
	else
		kprintf(CLISTOP, "State");
}

void clientInit()
{
	vSemaphoreCreateBinary(sHeader);
	vSemaphoreCreateBinary(sConnect);
	vSemaphoreCreateBinary(sConnected);
	vSemaphoreCreateBinary(sDisconnect);
	xSemaphoreTake(sConnect, portMAX_DELAY);
	xSemaphoreTake(sConnected, portMAX_DELAY);
	xSemaphoreTake(sDisconnect, portMAX_DELAY);
}

uint8_t clientIsConnected()
{
	if (xSemaphoreTake(sConnected, 0))
	{
		xSemaphoreGive(sConnected);
		return 0;
	}
	return 1;
}

// for debug only

void dump(uint8_t* from, uint32_t len)
{
	uint32_t i;
	uint8_t* addr;
	addr = from;
	for (i = 0; i < len; i += 16)
	{
		printf("\n%x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x    \t\
		%c %c %c %c %c %c %c %c %c %c %c %c %c %c %c %c",
			(unsigned int)addr, (unsigned int)addr[0], (unsigned int)addr[1], (unsigned int)addr[2], (unsigned int)addr[3], (unsigned int)addr[4], (unsigned int)addr[5], (unsigned int)addr[6], (unsigned int)addr[7], (unsigned int)addr[8], (unsigned int)addr[9], (unsigned int)addr[10], (unsigned int)addr[11], (unsigned int)addr[12], (unsigned int)addr[13], (unsigned int)addr[14], (unsigned int)addr[15], addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7], addr[8], addr[9], addr[10], addr[11], addr[12], addr[13], addr[14], addr[15]);
		addr += 16;
	}
	printf("\n");
}

struct icyHeader* clientGetHeader()
{
	return &header;
}

bool clientParsePlaylist(char* s)
{
	char* str;
	char* ns;
	char path[255] = "/";
	char url[78] = "";
	char port[6] = "80";
	int remove = 0;
	int i = 0;
	int j = 0;

	ESP_LOGV(TAG, "clientParsePlaylist  %s", s);
	// for extm3u skip line with #EXTINF
	str = strstr(s, "#EXTINF");
	if (str != NULL) //skip to next line
	{
		ns = str;
		while ((strlen(ns) > 1) && (ns[0] != 0x0A))
			ns++;
		ESP_LOGV(TAG, "EXTM3U: %s", ns);
		s = ns;
	}
	str = strstr(s, "<location>http://"); //for xspf
	if (str != NULL)
		remove = 17;

	if (str == NULL)
	{
		str = strstr(s, "<REF href = \"http://"); //for asx
		if (str != NULL)
			remove = 20;
	}
	if (str == NULL)
	{
		str = strstr(s, "http://");
		if (str != NULL)
			remove = 7;
		else
		{
			str = strstr(s, "HTTP://");
			if (str != NULL)
				remove = 7;
		}
	}

	if (str != NULL)
	{
		str += remove; //skip http://
		ESP_LOGV(TAG, "parse str %s", str);

		while ((str[i] != '/') && (str[i] != ':') && (str[i] != 0x0a) && (str[i] != 0x0d) && (j < 77))
		{
			url[j] = str[i];
			i++;
			j++;
		}
		url[j] = 0;
		ESP_LOGV(TAG, "parse str url %s", url);
		j = 0;
		if (str[i] == ':') //port
		{
			i++;
			while ((str[i] != '/') && (str[i] != 0x0a) && (str[i] != 0x0d) && (j < 5))
			{
				port[j] = str[i];
				i++;
				j++;
			}
			port[j] = 0;
		}
		ESP_LOGV(TAG, "parse str port %s", port);
		j = 0;
		if (str[i] == '/') //path
		{
			if ((str[i] != 0x0a) && (str[i] != 0x0d) && (str[i] != 0) && (str[i] != '"') && (str[i] != '<') && (j < 254))
			{
				while ((str[i] != 0x0a) && (str[i] != 0x0d) && (str[i] != 0) && (str[i] != '"') && (str[i] != '<') && (j < 254))
				{
					path[j] = str[i];
					i++;
					j++;
				}
				path[j] = 0;
			}
		}
		ESP_LOGV(TAG, "parse str path %s", path);

		if (strncmp(url, "localhost", 9) != 0)
			clientSetURL(url);
		clientSetPath(path);
		clientSetPort(atoi(port));
		ESP_LOGV(TAG, "clientParsePlaylist: url: %s, path: %s, port: %s", url, path, port);
		return true;
	}
	else
	{
		cstatus = C_DATA;
		return false;
	}
}

//---------------------------------------
// add escape char to the string
static char* stringify(char* str, int len)
{
#define MORE 20
	//		if ((strchr(str,'"') == NULL)&&(strchr(str,'/') == NULL)) return str;
	if (len == 0)
		return str;
	char* new = incmalloc(len + MORE);
	int nlen = len + MORE;
	if (new != NULL)
	{
		ESP_LOGV(TAG, "stringify: enter: len:%d  \"%s\"", len, str);
		int i = 0, j = 0;
		for (i = 0; i < len + 10; i++)
			new[i] = 0;
		for (i = 0; i < len; i++)
		{
			if (str[i] == '"')
			{
				new[j++] = '\\';
				new[j++] = (str)[i];
			}
			else if (str[i] == '/')
			{
				new[j++] = '\\';
				new[j++] = (str)[i];
			}
			else if (str[i] == '\\')
			{
				new[j++] = '\\';
				new[j++] = (str)[i];
			}
			else
				new[j++] = (str)[i];

			if (j + MORE > nlen)
			{
				nlen += MORE;
				new = realloc(new, nlen); // some room
			}
		}
		incfree(str, "str");

		new = realloc(new, j + 1); // adjust
		ESP_LOGV(TAG, "stringify: exit: len:%d  \"%s\"", j, new);
		return new;
	}
	else
	{
		ESP_LOGV(TAG, strcMALLOC1, "stringify");
	}
	return str;
}

bool clientPrintMeta()
{
	if (header.members.mArr[METADATA] != NULL)
		kprintf("##CLI.META#: %s\n", header.members.mArr[METADATA]);
	else
		kprintf("##CLI.META#:%c", 0x0D);

	return true;
}

char* getMeta()
{
	return (header.members.mArr[METADATA] == NULL) ? parEmpty : header.members.mArr[METADATA];
}

static void removePartOfString(char* origine, const char* remove)
{
	if (strlen(origine) == 0)
		return;
	char* copy = incmalloc(strlen(origine));
	char* t_end;
	if (copy != NULL)
	{
		while ((t_end = strstr(origine, remove)) != NULL)
		{
			*t_end = 0;
			strcpy(copy, origine);
			strcat(copy, t_end + (strlen(remove)));
			strcpy(origine, copy);
		}
		incfree(copy, "removePt");
	}
}

// A metadata found. Extract the Stream title
static void clientSaveMetadata(char* s, int len)
{
	char* t_end = NULL;
	char* t;
	bool found = false;
	if ((len == 0) || (s == NULL))
		ESP_LOGV(TAG, "clientSaveMetadata:  len:%d", len);
	if ((len > 256) || (s == NULL) || (len == 0)) // if not valid
	{
		if (header.members.mArr[METADATA] != NULL)
			incfree(header.members.mArr[METADATA], "metad"); // clear the old one
		header.members.mArr[METADATA] = NULL;				 // and exit
		return;
	}

	//remove all but title
	t = s;
	len = strlen(t);
	ESP_LOGV(TAG, "clientSaveMetadata:  len:%d   char:%s", len, s);
	t_end = strstr(t, "song_spot=");
	if (t_end != NULL)
	{
		*t_end = 0;
		found = true;
		removePartOfString(t, "text=");
		removePartOfString(t, "\"");
	}
	else
	{
		t_end = strstr(t, ";StreamUrl='");
		if (t_end != NULL)
		{
			*t_end = 0;
			found = true;
		}
	}
	t = strstr(t, "StreamTitle='");
	if (t != NULL)
	{
		t += 13;
		found = true;
	}
	else
		t = s;
	len = strlen(t);
	if ((t_end != NULL) && (len >= 3))
		t_end -= 3;
	else
	{
		if (t_end != NULL)
			t_end -= 1;
		else if (len >= 2)
		{
			t_end = t + len - 2;
			found = true;
		}
		else
			t_end = t + len;
	}

	if (found)
	{
		t_end = strstr(t_end, "'");
		if (t_end != NULL)
			*t_end = 0;

		if (t != NULL)
		{
			t_end = strstr(t, "||");
			if (t_end != NULL)
				*t_end = 0;
		}
	}
	else
	{
		if (len >= 2)
			len -= 2;
	}
	// the expurged str
	ESP_LOGV(TAG, "clientSaveMetadata0:  len:%d   char:%s", strlen(t), t);

	// see if meta is != of the old one
	char* tt;
	tt = incmalloc((len + 3) * sizeof(char));
	if (tt != NULL)
	{
		strcpy(tt, t);
		tt = stringify(tt, len); // to compare we need to stringify
	}
	if ((header.members.mArr[METADATA] == NULL) ||
		((header.members.mArr[METADATA] != NULL) && (t != NULL) && (strcmp(tt, header.members.mArr[METADATA]) != 0)))
	{
		incfree(tt, "");
		if (header.members.mArr[METADATA] != NULL)
			incfree(header.members.mArr[METADATA], "metad"); //clear the old one
		header.members.mArr[METADATA] = (char*)incmalloc((len + 3) * sizeof(char));
		if (header.members.mArr[METADATA] == NULL)
		{
			ESP_LOGV(TAG, strcMALLOC1, "metad");
			return;
		}

		strcpy(header.members.mArr[METADATA], t);
		//			dump((uint8_t*)(header.members.mArr[METADATA]),strlen(header.members.mArr[METADATA]));
		header.members.mArr[METADATA] = stringify(header.members.mArr[METADATA], len);
		clientPrintMeta();
		while ((header.members.mArr[METADATA][strlen(header.members.mArr[METADATA]) - 1] == ' ') ||
			(header.members.mArr[METADATA][strlen(header.members.mArr[METADATA]) - 1] == '\r') ||
			(header.members.mArr[METADATA][strlen(header.members.mArr[METADATA]) - 1] == '\n'))
		{
			header.members.mArr[METADATA][strlen(header.members.mArr[METADATA]) - 1] = 0; // avoid blank at end
		}

		// send station name if no metadata
		if (strlen(header.members.mArr[METADATA]) != 0)
			t_end = header.members.mArr[METADATA];
		else
			t_end = (header.members.single.name == NULL) ? (char*)"" : header.members.single.name;
		//
		char* title = incmalloc(strlen(t_end) + 15);
		if (title != NULL) // broadcast to all websockets
		{
			sprintf(title, "{\"meta\":\"%s\"}", t_end);
			incfree(title, "title");
		}
		else
			ESP_LOGV(TAG, strcMALLOC1, "Title");
	}
}


//Clear all ICY and META infos

static void clearHeaders()
{
	uint8_t header_num;
	for (header_num = 0; header_num < ICY_HEADER_COUNT; header_num++)
	{
		if (header_num != METAINT)
			if (header.members.mArr[header_num] != NULL)
			{
				incfree(header.members.mArr[header_num], "header");
				//				header.members.mArr[header_num][0] = 0;
				header.members.mArr[header_num] = NULL;
			}
	}
	header.members.mArr[METAINT] = 0;
}

bool clientPrintOneHeader(uint8_t header_num)
{
	if (header.members.mArr[header_num] != NULL)
		kprintf("##CLI.ICY%d#: %s\n", header_num, header.members.mArr[header_num]);
	return true;
}

bool clientPrintHeaders()
{
	uint8_t header_num;
	for (header_num = 0; header_num < ICY_HEADER_COUNT; header_num++)
	{
		if ((header_num != METAINT) && (header_num != METADATA))
			if (header.members.mArr[header_num] != NULL)
			{
				kprintf("##CLI.ICY%d#: %s\n", header_num, header.members.mArr[header_num]);
			}
	}
	clientPrintMeta();
	return true;
}

bool clientSaveOneHeader(const char* t, uint16_t len, uint8_t header_num)
{
	char* tt;
	if (header.members.mArr[header_num] != NULL)
		incfree(header.members.mArr[header_num], "headernum");
	tt = incmalloc((len + 1) * sizeof(char));
	if (tt == NULL)
	{
		ESP_LOGV(TAG, strcMALLOC1, "clientSOneH");
		return false;
	}

	int i;
	for (i = 0; i < len + 1; i++)
		tt[i] = 0;
	strncpy(tt, t, len);
	header.members.mArr[header_num] = stringify(tt, len); //tt is freed here
	vTaskDelay(10);
	clientPrintOneHeader(header_num);
	ESP_LOGV(TAG, "header after num:%d addr:0x%x  cont:\"%s\"", header_num, (int)header.members.mArr[header_num], header.members.mArr[header_num]);
	return true;
}


bool clientParseHeader(char* s)
{
	// icy-notice1 icy-notice2 icy-name icy-genre icy-url icy-br
	uint8_t header_num;
	char* t;
	bool ret = false;
	ESP_LOGV(TAG, "ParseHeader: %s", s);
	xSemaphoreTake(sHeader, portMAX_DELAY);
	if ((cstatus != C_HEADER1) && (cstatus != C_PLAYLIST)) // not ended. dont clear
	{
		clearHeaders();
	}

	t = strstr(s, "Content-Type:");
	if (t == NULL)
		t = strstr(s, "content-type:");
	if (t != NULL)
	{
		contentType = KMIME_UNKNOWN;
		if (strstr(t, "application/octet-stream"))
			contentType = KOCTET_STREAM;
		if (strstr(t, "audio/aac"))
			contentType = KAUDIO_AAC;
		if (strstr(t, "audio/mp4"))
			contentType = KAUDIO_MP4;
		if (strstr(t, "audio/x-m4a"))
			contentType = KAUDIO_MP4;
		if (strstr(t, "audio/mpeg"))
			contentType = KAUDIO_MPEG;

		if (contentType == KMIME_UNKNOWN)
		{
			ESP_LOGD(TAG, "unknown contentType: %s", t);
		}
		ESP_LOGD(TAG, "contentType: %d", contentType);
		player_config->media_stream->content_type = contentType;
	}

	for (header_num = 0; header_num < ICY_HEADERS_COUNT; header_num++)
	{
		ESP_LOGV(TAG, "icy deb: %d", header_num);
		t = strstr(s, icyHeaders[header_num]);
		if (t != NULL)
		{
			t += strlen(icyHeaders[header_num]);
			char* t_end = strstr(t, "\r\n");
			if (t_end != NULL)
			{
				ESP_LOGV(TAG, "icy in: %d", header_num);
				uint16_t len = t_end - t;
				if (header_num != METAINT) // Text header field
				{
					ret = clientSaveOneHeader(t, len, header_num);
				}
				else // Numerical header field
				{
					int i;
					for (i = 0; i < len + 1; i++)
						metaint[i] = 0;
					strncpy(metaint, t, len);
					header.members.single.metaint = atoi(metaint);
					ESP_LOGV(TAG, "len = %d,MetaInt= %s, Metaint= %d", len, metaint, header.members.single.metaint);
					ret = true;
					ESP_LOGV(TAG, "icy: %s, %d", icyHeaders[header_num], header.members.single.metaint);
				}
			}
		}
	}
	xSemaphoreGive(sHeader);
	return ret;
}

void clientSetName(const char* name, uint16_t index)
{
	kprintf("##CLI.NAMESET#: %d %s\n", index, name);
}

void clientSetURL(char* url)
{
	//remove	int l = strlen(url)+1;
	if (url[0] == 0xff)
		return; // wrong url
	strcpy(clientURL, url);
	kprintf("##CLI.URLSET#: %s\n", clientURL);
}

void clientSetPath(char* path)
{
	//remove	int l = strlen(path)+1;
	if (path[0] == 0xff)
		return; // wrong path
	strcpy(clientPath, path);
	kprintf("##CLI.PATHSET#: %s\n", clientPath);
}

void clientSetPort(uint16_t port)
{
	clientPort = port;
	kprintf("##CLI.PORTSET#: %d\n", port);
}

void clientConnect()
{
	cstatus = C_HEADER;
	once = 0;
	if ((server = (struct hostent*)gethostbyname(clientURL)))
	{
		xSemaphoreGive(sConnect);
		esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
	}
	else
	{
		clientDisconnect("clientConnect");
		vTaskDelay(1);
	}
}

void clientConnectOnce()
{
	cstatus = C_HEADER;
	if ((server = (struct hostent*)gethostbyname(clientURL)))
	{
		xSemaphoreGive(sConnect);
		esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
	}
	else
	{
		clientDisconnect("clientConnectOnce");
	}
	once = 1; // play one time
}

void clientSilentConnect()
{
	cstatus = C_HEADER;
	once = 0;
	if (server != NULL)
	{
		xSemaphoreGive(sConnect);
		esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
	}
	else
	{
		clientSilentDisconnect();
	}
}

void clientSilentDisconnect()
{
	xSemaphoreGive(sDisconnect);

	ESP_LOGW(TAG, "Client silent Disconnect!!!! Stopping player");

	if (get_player_status() != STOPPED)
		audio_player_stop();

	int i = 0;
	while (!clientIsConnected() && i < 100)
	{
		i++;
		vTaskDelay(1);
	}
	esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
}

void clientDisconnect(const char* from)
{
	kprintf(CLISTOP, from);
	xSemaphoreGive(sDisconnect);

	ESP_LOGW(TAG, "Client Disconnect!!!! Stopping player");

	if (get_player_status() != STOPPED)
		audio_player_stop();

	int i = 0;
	while (!clientIsConnected() && i < 100)
	{
		i++;
		vTaskDelay(1);
	}

	if ((from[0] != 'C') || (from[1] != '_'))
	esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
	vTaskDelay(5);
	// save the volume if needed on stop state
}

void clientReceiveCallback(int sockfd, char* pdata, int len)
{
	static int metad;
	static int rest;
	static IRAM_ATTR uint32_t chunked;
	static IRAM_ATTR uint32_t cchunk;
	static char* metadata = NULL;
	uint16_t l = 0;
	uint32_t lc;
	char* inpdata;
	char* inpchr;
	uint32_t clen;
	int bread;
	char* t1;
	char* t2;
	bool icyfound;

	//	if (cstatus != C_DATA) {printf("cstatus= %d\n",cstatus);  printf("Len=%d, Byte_list = %s\n",len,pdata);}
	if (cstatus != C_DATA)
	{
		t1 = strstr(pdata, "404");
		if (t1 != NULL)
		{
			t1 = strstr(pdata, notfound);
			if (t1 == NULL)
				t1 = strstr(pdata, "not be found");
		}
		if (t1 != NULL)
		{ //
			kprintf(CLIPLAY, 0x0d, 0x0a);
			clientSaveOneHeader(notfound, 9, METANAME);
			vTaskDelay(1);
			cstatus = C_HEADER;
			return;
		}
	}
	switch (cstatus)
	{
	case C_PLAYLIST:
		if (!clientParsePlaylist(pdata)) //need more
			cstatus = C_PLAYLIST1;
		else
		{
			clientDisconnect("C_PLIST");
		}
		break;
	case C_PLAYLIST1:
		clientDisconnect("C_PLIST1");
		clientParsePlaylist(pdata); //more?
		cstatus = C_PLAYLIST;
		break;
	case C_HEADER0:
	case C_HEADER:
		clearHeaders();
		metad = -1;
		t1 = strstr(pdata, "302 ");
		if (t1 == NULL)
			t1 = strstr(pdata, "301 ");
		if (t1 != NULL)
		{ // moved to a new address
			if (strcmp(t1, "Found") || strcmp(t1, "Temporarily") || strcmp(t1, "Moved"))
			{
				ESP_LOGV(TAG, "Header Len=%d,\n %s", len, pdata);
				ESP_LOGI(TAG, "Header: Moved");
				clientDisconnect("C_HDER");
				clientParsePlaylist(pdata);
				cstatus = C_PLAYLIST;
			}
			break;
		}
		//no break here
		/* fall through */
	case C_HEADER1: // not ended
	{
		int i = 0;
		cstatus = C_HEADER1;
		do
		{
			t1 = strstr(pdata, "\r\n\r\n"); // END OF HEADER
			ESP_LOGV(TAG, "Header1 len: %d,  Header: %s", len, pdata);
			if ((t1 != NULL) && (t1 <= pdata + len - 4))
			{
				t2 = strstr(pdata, "Internal Server Error");
				if (t2 != NULL)
				{
					ESP_LOGV(TAG, "Internal Server Error");
					clientDisconnect("Internal Server Error");
					cstatus = C_HEADER;
				}
				icyfound = clientParseHeader(pdata);
				if (header.members.single.metaint > 0)
					metad = header.members.single.metaint;
				ESP_LOGD(TAG, "t1: 0x%x, cstatus: %d, icyfound: %d  metad:%d Metaint:%d\n", (int)t1, cstatus, icyfound, metad, (header.members.single.metaint));
				cstatus = C_DATA; // a stream found

				ESP_LOGW(TAG, "Stream found!!!! Start player");

				/////////////////////////////////////////////////////////////////////////////////////////////////
				player_config->media_stream->eof = false;
				audio_player_start();
				/////////////////////////////////////////////////////////////////////////////////////////////////

				t2 = strstr(pdata, "Transfer-Encoding: chunked"); // chunked stream?
				chunked = 0;
				t1 += 4;
				if (t2 != NULL)
				{
					while (len - (t1 - pdata) < 8)
					{
						vTaskDelay(1);
						len += recv(sockfd, pdata + len, RECEIVE + 8 - len, 0);
					}
					chunked = (uint32_t)strtol(t1, NULL, 16) + 2;
					if (strchr((t1), 0x0A) != NULL)
						*strchr(t1, 0x0A) = 0;

					ESP_LOGD(TAG, "chunked: %d,  strlen: %d  \"%s\"", chunked, strlen(t1) + 1, t1);
					t1 += strlen(t1) + 1; //+1 for char 0,
				}

				int newlen = len - (t1 - pdata);
				cchunk = chunked;
				ESP_LOGD(TAG, "newlen: %d   len: %d   chunked:%d  pdata:%x", newlen, len, chunked, (int)pdata);
				if (newlen > 0)
				{
					clientReceiveCallback(sockfd, t1, newlen);
				}
			}
			else
			{
				t1 = NULL;
				if (i++ > 20)
				{
					clientDisconnect("header1");
					break;
				}
				vTaskDelay(1); //avoid watchdog is infernal loop
				bread = recvfrom(sockfd, pdata + len, RECEIVE - len, 0, NULL, NULL);
				if (bread > 0)
					len += bread;
			}
		} while (t1 == NULL);
	}
	break;
	default:
		// -----------

		// Chunk computing
		lc = len;													  // lc rest after chunk
																	  //	 printf("CDATAIN: chunked: %d, cchunk: %d, len: %d\n",chunked,cchunk,len);
		if ((chunked != 0) && ((cchunk == 0) || (len >= cchunk - 1))) //if in chunked mode and chunk received or complete in data
		{
			//	 printf("CDATA1: chunked: %d, cchunk: %d, len: %d\n",chunked,cchunk,len);
			if (len == cchunk) // if a complete chunk in pdata, remove crlf
			{
				len -= 2;
				cchunk = 0;
				//	printf("lenoe:%d, chunked:%d  cchunk:%d, lc:%d, metad:%d\n",len,chunked,cchunk, lc,metad );
			}
			else // an incomplete chunk in progress
			{
				if (len == cchunk - 1) // missing lf: remove cr only, wait lf in next data
				{
					len -= 1;
					cchunk = 1;
					//	printf("leno1:%d, chunked:%d  cchunk:%d, lc:%d, metad:%d\n",len,chunked,cchunk, lc,metad );
				}
				else // a part of end of chunk 	and beginnining of a new one
				{
					inpdata = pdata;

					while (lc != 0)
					{
						while (lc < cchunk + 9)
						{
							vTaskDelay(1);
							bread = recvfrom(sockfd, pdata + len, 9, 0, NULL, NULL);
							if (bread > 0)
								clen = bread;
							else
								clen = 0;
							lc += clen;
							len += clen;
							//ESP_LOGV(TAG,"more:%d, lc:%d\n",clen,lc);
						} //security to be sure to receive the new length

						//	printf("leni0:%d, inpdata:%x, chunked:%d  cchunk:%d, lc:%d, \n",len,inpdata,chunked,cchunk, lc );
						inpchr = strchr(inpdata + cchunk, 0x0D);
						if ((inpchr != NULL) && (inpchr - (inpdata + cchunk) < 16))
							*inpchr = 0; // replace lf by a end of string
						else
						{
							clientDisconnect("chunk");
							clientConnect();
							lc = 0;
							break;
						}
						chunked = (uint32_t)strtol(inpdata + cchunk, NULL, 16) + 2; // new chunk lenght including cr lf
						clen = strlen(inpdata + cchunk) + 2;
						lc = lc - cchunk - clen; // rest after
												 //	printf("leni:%d, inpdata:%x, chunked:%d  cchunk:%d, lc:%d, clen:%d, str: %s\n",len,inpdata,chunked,cchunk, lc,clen,inpdata+cchunk );
						// compact data without chunklen and crlf
						if (cchunk > 1)
						{
							memcpy(inpdata + cchunk - 2, pdata + len - lc, lc);
							//	printf("lenm:%d, inpdata:%x, chunked:%d  cchunk:%d, lc:%d\n",len,inpdata,chunked,cchunk, lc);
							len -= (clen + 2);
							inpdata += (cchunk - 2);
							//	printf("memcpy1 at %x from %x, lc:%d\n",inpdata+cchunk-2,pdata+len-lc,lc);
						}
						else
						{
							memcpy(inpdata, inpdata + cchunk + clen, lc);
							//	printf("lenm:%d, inpdata:%x, chunked:%d  cchunk:%d, lc:%d\n",len,inpdata,chunked,cchunk, lc);
							len -= (clen + cchunk);
							//	printf("memcpy2 at %x from %x, lc:%d, len:%d\n",inpdata,inpdata+cchunk+clen,lc,len);
						}

						if (chunked > lc)
						{
							cchunk = chunked - lc;
							if (cchunk == 1)
								len--;
							if (cchunk == 0)
								len -= 2;
							lc = 0;
						}
						else
						{
							cchunk = chunked;
						}
						//	printf("leniout:%d, inpdata:%x, chunked:%d  cchunk:%d, lc:%d, metad:%d  clen:%d \n",len,inpdata,chunked,cchunk, lc,metad,clen );
					}
				}
			}
		}
		else
		{
			if (chunked != 0)
				cchunk -= len;
			lc = 0;
		}

		// printf("CDATAOUT: chunked: %d, cchunk: %d, len: %d\n",chunked,cchunk,len);

		// meta data computing
		if (rest < 0)
		{
			ESP_LOGD(TAG, "clientReceiveCallback: pdata: %x, pdataend: %x, len: %d", (int)pdata, (int)pdata + len, len);
			ESP_LOGD(TAG, "Negative enter len= %d, metad= %d  rest= %d   pdata= %x :\"%s\"", len, metad, rest, (int)pdata, pdata);
			if (len > -rest)
				*(pdata - rest) = 0; //truncated
			else
				*(pdata + len) = 0; //truncated
			strcat(metadata, pdata);
			if (len > -rest)
			{
				ESP_LOGD(TAG, "Negaposi   len= %d, metad= %d  rest= %d   pdata= %x :\"%s\"", len, metad, rest, (int)pdata, pdata);
				clientSaveMetadata(metadata, strlen(metadata));
				metad = header.members.single.metaint;
				pdata -= rest;
				len += rest;
				rest = 0;
			}
			else
			{
				ESP_LOGD(TAG, "Negative   len= %d, metad= %d  rest= %d   pdata= %x :\"%s\"", len, metad, rest, (int)pdata, pdata);
				pdata += len;
				rest += len;
				len = 0;
				ESP_LOGD(TAG, "Negatafter len= %d, metad= %d  rest= %d   pdata= %x :\"%s\"", len, metad, rest, (int)pdata, pdata);
			}
			//printf("Negative len out = %d, pdata: %x,metad= %d  rest= %d \n",len,pdata,metad,rest);
		}
		inpdata = pdata;
		clen = len;
		if ((header.members.single.metaint != 0) && (clen > metad))
		{
			int jj = 0;
			while ((clen > metad) && (header.members.single.metaint != 0)) // in buffer
			{
				//				ESP_LOGD(TAG,"metainb len:%d, clen:%d, metad:%d, l:%d, inpdata:%x, rest:%d\n",len,clen,metad, l,(int)inpdata,rest );
				jj++;
				l = inpdata[metad] * 16; //new meta length
				rest = clen - metad - l - 1;

				if (l != 0)
				{
					ESP_LOGD(TAG, "clientReceiveCallback: pdata: %x, pdataend: %x, len: %d", (int)pdata, (int)pdata + len, len);
					ESP_LOGD(TAG, "mt len:%d, clen:%d, metad:%d ,&l:%x, l:%d, inpdata:%x, rest:%d, str: %s", len, clen, metad, (int)inpdata + metad, l, (int)inpdata, rest, inpdata + metad + 1);
					if (rest < 0)
					{
						*(inpdata + clen) = 0; //truncated
						ESP_LOGD(TAG, "mtlen len:%d, clen:%d, metad:%d, l:%d, inpdata:%x,  rest:%d", len, clen, metad, l, (int)inpdata, rest);

						if (metadata != NULL)
							incfree(metadata, "meta");
						metadata = incmalloc(l + 1);
						strcpy(metadata, inpdata + metad + 1);
					}
					else
						clientSaveMetadata(inpdata + metad + 1, l);
				}
				if (metad > 0)
				{
					//					if (spiRamFifoFree() < metad) ESP_LOGV(TAG,"metaout wait metad: %d, bufferfree: %d",metad,spiRamFifoFree());
					while (spiRamFifoFree() < metad) // wait some room
						vTaskDelay(20);
					audio_stream_consumer((char*)inpdata, metad, (void*)player_config); //write stream data in bufer 
				}
				metad = header.members.single.metaint;
				inpdata = inpdata + clen - rest;
				if (rest < 0)
					kprintf("mt1 len:%d, clen:%d, metad:%d, l:%d, inpdata:%x,  rest:%d", len, clen, metad, l, (int)inpdata, rest);
				clen = rest;
				if (rest < 0)
				{
					kprintf("mt2 len:%d, clen:%d, metad:%d, l:%d, inpdata:%x,  rest:%d", len, clen, metad, l, (int)inpdata, rest);
					clen = 0;
					break;
				}
			} // while in buffer
			if (rest >= 0)
			{
				metad = header.members.single.metaint - rest; //until next
				if (rest > 0)
				{
					while (spiRamFifoFree() < rest) // wait some room
						vTaskDelay(20);				//
					audio_stream_consumer((char*)inpdata, rest, (void*)player_config); //write stream data in bufer 
				}
				rest = 0;
			}
		}
		else
		{

			if (header.members.single.metaint != 0)
				metad -= len;
			if (len > 0)
			{
				while (spiRamFifoFree() < len) // wait some room
					vTaskDelay(20);
				audio_stream_consumer((char*)(pdata + rest), len, (void*)player_config); //write stream data in bufer 
			}
		}
		// ---------------
		if (!playing)
		{
			playing = 1;
			if (once == 0)
				vTaskDelay(20);
			else
				vTaskDelay(1);

			kprintf(CLIPLAY, 0x0d, 0x0a);
		}
	}
}


void playStationInt(int st_n)
{

	ESP_LOGE(TAG, "Start PSI");

	//struct shoutcast_info _si = {"stream.radioparadise.com", "/flac", "Sector", 80};
	//struct shoutcast_info _si = {"live.m2stream.fr", "/m2rock-128.mp3", "M2 Rock", 80};
	struct shoutcast_info _si = {"mp3.polskieradio.pl", "/", "Pink Floyd", 8070};
	struct shoutcast_info *si = &_si;

	ESP_LOGE(TAG,"playstationInt: %d, Name: %s, url: %s, path: %s, port: %d\n", st_n, si->name, si->domain, si->file, si->port);


	vTaskDelay(4);
	
	clientSetName(si->name, st_n);
	clientSetURL(si->domain);
	clientSetPath(si->file);
	clientSetPort(si->port);
	

	clientConnect();
	int i = 0;
	while (i < 100 && !clientIsConnected())
	{
		i++;
		vTaskDelay(5);
	}

	ESP_LOGE(TAG, "playstationInt: %d, g_device: %d", st_n, g_device.currentstation);
}

uint8_t bufrec[RECEIVE + 20];
static char useragent[40];

void clientTask(void* pvParams)
{
	portBASE_TYPE uxHighWaterMark;
	struct timeval timeout;
	timeout.tv_usec = 0;
	timeout.tv_sec = 3;
	int sockfd;
	int bytes_read;
	uint8_t cnterror;

	strcpy(useragent, "Karadio/1.5");
	
	struct sockaddr_in dest;

	vTaskDelay(30);

	//	portBASE_TYPE uxHighWaterMark;
	//	uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
	//	printf("watermark webclient:%d  heap:%d\n",uxHighWaterMark,xPortGetFreeHeapSize( ));

	while (1)
	{
		xSemaphoreGive(sConnected);
		if (xSemaphoreTake(sConnect, portMAX_DELAY))
		{
			xSemaphoreTake(sDisconnect, 0);
			sockfd = socket(AF_INET, SOCK_STREAM, 0);
			sterr = lwip_strerr(errno);
			ESP_LOGI(TAG, "Webclient socket: %d, err: %s", sockfd, sterr);
			if (sockfd < 0)
			{
				sterr = lwip_strerr(errno);
				ESP_LOGE(TAG, "Webclient socket create, err: %s", sterr);
				xSemaphoreGive(sDisconnect);
				continue;
			}
			bzero(&dest, sizeof(dest));
			dest.sin_family = AF_INET;
			dest.sin_port = htons(clientPort);
			dest.sin_addr.s_addr = inet_addr(inet_ntoa(*(struct in_addr*)(server->h_addr_list[0])));
			bytes_read = 0;

			/*---Connect to server---*/
			ESP_LOGI(TAG, "Webclient socket: %d, addr: %u", sockfd, dest.sin_addr.s_addr);
			if (connect(sockfd, (struct sockaddr*)&dest, sizeof(dest)) >= 0)
			{
				//				printf("WebClient Socket connected\n");
				memset(bufrec, 0, RECEIVE + 20);

				char* t0 = strstr(clientPath, ".m3u");
				if (t0 == NULL)
					t0 = strstr(clientPath, ".pls");
				if (t0 == NULL)
					t0 = strstr(clientPath, ".xspf");
				if (t0 == NULL)
					t0 = strstr(clientPath, ".m3u8");
				if (t0 == NULL)
					t0 = strstr(clientPath, ".asx");
				if (t0 != NULL) // a playlist asked
				{
					cstatus = C_PLAYLIST;
					//printf("sprint%d\n",6);
					sprintf((char*)bufrec, "GET %s HTTP/1.1\r\nHOST: %s\r\nUser-Agent: %s\r\n\r\n", clientPath, clientURL, useragent); //ask for the playlist
				}
				else
				{
					if (strcmp(clientURL, "stream.pcradio.biz") == 0)
						strcpy(useragent, "pcradio");
					//printf("sprint%d\n",7);
					sprintf((char*)bufrec, "GET %s HTTP/1.1\r\nHost: %s\r\nicy-metadata: 1\r\nUser-Agent: %s\r\n\r\n", clientPath, clientURL, useragent);
				}
				xSemaphoreTake(sConnected, 0);

				kprintf((char*)bufrec, "GET %s HTTP/1.1\r\nHost: %s\r\nicy-metadata: 1\r\nUser-Agent: %s\r\n\r\n", clientPath, clientURL, useragent);
				send(sockfd, (char*)bufrec, strlen((char*)bufrec), 0);

				if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) < 0)
					sterr = lwip_strerr(errno);
				ESP_LOGE(TAG, "Client socket: %d  setsockopt: %d  err: %s", sockfd, bytes_read, sterr);
				//////
				cnterror = 0;
				do
				{
					bytes_read = recvfrom(sockfd, bufrec, RECEIVE, 0, NULL, NULL);
					if (bytes_read < 0)
					{
						sterr = lwip_strerr(errno);
						ESP_LOGE(TAG, "Client socket: %d  read: %d  err: %s", sockfd, bytes_read, sterr);
						if (errno == 11)
							bytes_read = 0;
					}
					if (bytes_read > 0)
					{
						cnterror = 0;
						clientReceiveCallback(sockfd, (char*)bufrec, bytes_read);
					}
					else
					{
						sterr = lwip_strerr(errno);
						ESP_LOGE(TAG, "No data in recv. Err: %s", sterr);
						cnterror++;
						if (errno != 11)
							vTaskDelay(20); //timeout
						else
							vTaskDelay(2);
						if ((errno == 128) || (cnterror > 9))
							break;
					}
					vTaskDelay(2);
					// if a stop is asked
					if (xSemaphoreTake(sDisconnect, 0))
					{
						clearHeaders();
						break;
					}
				} while ((bytes_read > 0) || (playing && (bytes_read == 0)));
			}
			else
			{
				sterr = lwip_strerr(errno);
				ESP_LOGE(TAG, "Client socket: %d  connect: %d  err: %s ", sockfd, bytes_read, sterr);
				vTaskDelay(1);
				clientDisconnect("Invalid");
				close(sockfd);
				continue;
			}
			/*---Clean up---*/
			if (bytes_read <= 0) //nothing received or error or disconnected
			{
				if ((playing) && (once == 0)) // try restart
				{
					clientDisconnect("try restart");
					clientConnect();
					playing = 1; // force
				}
				else if ((!playing) && (once == 1))
				{ // nothing played. Force the read of the buffer
					// some data not played
					if ((!playing) && (spiRamFifoFill()))
					{
						playing = 1;
						vTaskDelay(1);
						kprintf(CLIPLAY, 0x0d, 0x0a);
						while (spiRamFifoFill())
							vTaskDelay(100);
						vTaskDelay(150);
						playing = 0;
						clientDisconnect("data not played");
					}
				}
				//
				else if ((!playing) && (once == 0))
				{ // nothing received
					clientDisconnect(nodata);
					clientSaveOneHeader(nodata, 7, METANAME);
					vTaskDelay(1);
				}
				else
				{ //playing & once=1 and no more received stream
					while (spiRamFifoFill())
						vTaskDelay(100);
					vTaskDelay(200);
					clientDisconnect("once");
				}
			} //jpc

			if (playing) // stop clean
			{

				ESP_LOGE(TAG, "Stop clean!!!! Stopping player");

				if (get_player_status() != STOPPED)
					audio_player_stop();

				player_config->media_stream->eof = true;

				vsflush_cancel(2);

				playing = 0;
				vTaskDelay(40); // stop without click

			}

			shutdown(sockfd, SHUT_RDWR); // stop the socket
			vTaskDelay(1);
			close(sockfd);

			if (cstatus == C_PLAYLIST)
			{
				clientConnect();
			}
			uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
			ESP_LOGI(TAG, "watermark : %x  %d", uxHighWaterMark, uxHighWaterMark);
		}
	}
}