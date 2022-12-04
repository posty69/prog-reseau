#ifndef CLIENT_H
#define CLIENT_H

#include "server2.h"

typedef struct Client
{
   SOCKET sock;
   char name[BUF_SIZE];
   char password[BUF_SIZE];
   time_t lastConnection;
   int colorId;
   int channelId;
} Client;

#endif /* guard */
