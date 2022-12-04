#include <stdio.h>
#include <stdlib.h>
#include <dirent.h> 
#include <errno.h>
#include <string.h>
#include <time.h>

#include "server2.h"
#include "client2.h"

//printf("___________checkpoint_0");

static void init(void)
{
#ifdef WIN32
   WSADATA wsa;
   int err = WSAStartup(MAKEWORD(2, 2), &wsa);
   if(err < 0)
   {
      puts("WSAStartup failed !");
      exit(EXIT_FAILURE);
   }
#endif
}

static void end(void)
{
#ifdef WIN32
   WSACleanup();
#endif
}

static void app(void)
{
   SOCKET sock = init_connection();

   printf("[SERVER] Connected successfully\n");

   char buffer[BUF_SIZE];
   /* the index for the array */
   int actual = 0;
   int max = sock;
   /* an array for all clients */
   Client clients[MAX_CLIENTS];

   FILE *fp = fopen("clients", "r");
   if (fp == NULL)
   {
      printf("[SERVER] An error occured while opening the file 'clients'\n");
      exit(EXIT_FAILURE);
   }

   char *message = NULL;
   size_t len = 0;

   while (getline(&message, &len, fp) != -1)
   {      
      Client c;
      strncpy(c.name, strtok(message, "|"), BUF_SIZE - 1);
      strncpy(c.password, strtok(NULL, "|"), BUF_SIZE - 1);
      c.lastConnection = atoi(strtok(NULL, "|"));
      c.colorId = 31 + actual % 6;
      c.channelId = -1;

      clients[actual] = c;
      actual++;
   }

   fclose(fp);
   
   int actualChannels = 1;
   Channel channels[MAX_CHANNELS];

   DIR *d = opendir("channels");
   struct dirent *dir;

   if (d == NULL)
   {
      printf("[SERVER] An error occured while opening the directory 'channels'\n");
      exit(EXIT_FAILURE);
   }

   Channel channel = {};
   strncpy(channel.name, "global", BUF_SIZE - 1);
   channels[0] = channel;
   
   while ((dir = readdir(d)) != NULL) {
      Channel channel = {};
      strncpy(channel.name, dir->d_name, BUF_SIZE - 1);     
      channels[actualChannels] = channel;
      actualChannels++;
   }

   closedir(d);

   fd_set rdfs;

   while(1)
   {
      int i = 0;
      FD_ZERO(&rdfs);

      /* add STDIN_FILENO */
      FD_SET(STDIN_FILENO, &rdfs);

      /* add the connection socket */
      FD_SET(sock, &rdfs);

      /* add socket of each client */
      for(i = 0; i < actual; i++)
      {
         if (clients[i].channelId != -1)
         {
            FD_SET(clients[i].sock, &rdfs);
         }
      }

      if(select(max + 1, &rdfs, NULL, NULL, NULL) == -1)
      {
         perror("select()");
         exit(errno);
      }

      /* something from standard input : i.e keyboard */
      if(FD_ISSET(STDIN_FILENO, &rdfs))
      {
         /* stop process when type on keyboard */
         break;
      }
      else if(FD_ISSET(sock, &rdfs))
      {
         /* new client */
         SOCKADDR_IN csin = { 0 };
         size_t sinsize = sizeof csin;
         int csock = accept(sock, (SOCKADDR *)&csin, &sinsize);
         if(csock == SOCKET_ERROR)
         {
            perror("accept()");
            continue;
         }

         //after connecting the client sends its name and its password
         if(read_client(csock, buffer) == -1)
         {
            //disconnected
            continue;
         }

         //what is the new maximum fd ?
         max = csock > max ? csock : max;

         FD_SET(csock, &rdfs);

         char *name = strtok(buffer, ":");
         char *password = strtok(NULL, ":");

         printf("[SERVER] Incoming connection : User '%s' authentified with password '%s'\n", name, password);

         int idClient;
         for (idClient = 0; idClient < actual; idClient++)
         {
            if (strcmp(clients[idClient].name, name) == 0)
            {
               if (clients[idClient].channelId == -1)
               {
                  if (strcmp(clients[idClient].password, password) == 0)
                  {
                     clients[idClient].sock = csock;

                     printf("[SERVER] User '%s' connected successfully\n", name);

                     client_joins(clients, actual, channels, actualChannels, idClient, "global");

                     break;
                  }
                  else
                  {
                     // Wrong password
                     printf("[SERVER] Wrong password for user '%s'\n", name);
                     
                     break;
                  }
               }
               else
               {
                  printf("[SERVER] User '%s' is already online\n", name);
               }
            }
         }

         if (idClient == actual)
         {
            // Unknown user
            printf("[SERVER] Unknown user '%s'\n", name);
         }
      }
      else
      {
         for(int idClient = 0; idClient < actual; idClient++)
         {
            /* a client is talking */
            if(FD_ISSET(clients[idClient].sock, &rdfs))
            {
               int c = read_client(clients[idClient].sock, buffer);
               /* client disconnected */
               if(c == 0)
               {
                  closesocket(clients[idClient].sock);
                  
                  clients[idClient].channelId = -1;
                  clients[idClient].lastConnection = time(NULL);

                  // TODO Save last connection in clients.txt file

                  printf("[SERVER] User '%s' disconnected successfully\n", clients[idClient].name);
                  
                  strncpy(buffer, clients[idClient].name, BUF_SIZE - 1);
                  strncat(buffer, " disconnected !", BUF_SIZE - strlen(buffer) - 1);
                  send_message_to_all_clients(clients, actual, channels, clients[idClient], buffer, 1);
               }
               else
               {
                  if (buffer[0] == '/')
                  {
                     // Command
                     printf("[SERVER] Incomming command : '%s'\n", buffer);

                     switch (buffer[1])
                     {
                        case 'c': {
                           // Channel creation
                           strtok(buffer, " ");
                           char *channelName = strtok(NULL, " ");

                           int idChannel;
                           for (idChannel = 0; idChannel < actualChannels; idChannel++)
                           {
                              if (strcmp(channels[idChannel].name, channelName) == 0)
                              {
                                 break;
                              }
                           }

                           if (idChannel < actualChannels)
                           {
                              // Channel already exists
                              printf("[SERVER] Channel '%s' already exists\n", channelName);
                              
                              break;
                           }

                           Channel channel = {};
                           strncpy(channel.name, channelName, BUF_SIZE - 1);
                           channels[actualChannels] = channel;
                           actualChannels++;

                           printf("[SERVER] User '%s' created channel '%s'\n", clients[idClient].name, channelName);

                           break;
                        }

                        case 'j': {
                           // Channel join
                           strtok(buffer, " ");
                           char *channelName = strtok(NULL, " ");

                           client_joins(clients, actual, channels, actualChannels, idClient, channelName);

                           break;
                        }

                        default:
                           // Error
                           break;
                     }
                  }
                  else
                  {
                     send_message_to_all_clients(clients, actual, channels, clients[idClient], buffer, 0);
                  }
               }
            }
         }
      }
   }

   clear_clients(clients, actual);
   end_connection(sock);
}

static void clear_clients(Client *clients, int actual)
{
   FILE *fp = fopen("clients", "w");
   if (fp == NULL)
   {
      printf("[SERVER] An error occured while opening the file 'clients'\n");
      //exit(EXIT_FAILURE);
   } else {
      // Save client data
      for(int clientId = 0; clientId < actual; clientId++)
      {
         closesocket(clients[clientId].sock);
         fprintf(fp, "%s|%s|%lu\n", clients[clientId].name, clients[clientId].password, time(NULL));
      }
   }
}

static void remove_client(Client *clients, int to_remove, int *actual)
{
   /* we remove the client in the array */
   memmove(clients + to_remove, clients + to_remove + 1, (*actual - to_remove - 1) * sizeof(Client));
   /* number client - 1 */
   (*actual)--;
}

static void send_message_to_all_clients(Client *clients, int actual, Channel *channels, Client sender, const char *buffer, char from_server)
{
   // Id of the channel we want to send the message in
   int idChannel = from_server == 1 ? 0 : sender.channelId;

   char message[BUF_SIZE];
   message[0] = 0;
   if(from_server == 0)
   {
      char colorBuffer[BUF_SIZE];
      sprintf(colorBuffer, "%d", sender.colorId);

      strncpy(message, "(", BUF_SIZE - 1);
      strncat(message, channels[idChannel].name, sizeof message - strlen(message) - 1);
      strncat(message, ") [\x1B[", sizeof message - strlen(message) - 1);
      strncat(message, colorBuffer, sizeof message - strlen(message) - 1);
      strncat(message, "m", sizeof message - strlen(message) - 1);
      strncat(message, sender.name, sizeof message - strlen(message) - 1);
      strncat(message, "\033[0m] : ", sizeof message - strlen(message) - 1);
   }

   strncat(message, buffer, sizeof message - strlen(message) - 1);

   if (from_server == 0)
   {
      printf("%s\n", message);

      char channelNameBuffer[BUF_SIZE];
      strncpy(channelNameBuffer, "channels/", BUF_SIZE - 1);
      strncat(channelNameBuffer, channels[idChannel].name, sizeof channelNameBuffer - strlen(channelNameBuffer) - 1);

      FILE *fp = fopen(channelNameBuffer, "a");
      if (fp == NULL)
      {
         printf("[SERVER] An error occured while opening the file '%s'\n", channelNameBuffer);
         //exit(EXIT_FAILURE);
      }
      else
      {
         fprintf(fp, "%lu|%s\n", time(NULL), message);
         fclose(fp);
      }
   }

   for(int idClient = 0; idClient < actual; idClient++)
   {
      if(clients[idClient].channelId == idChannel && sender.sock != clients[idClient].sock)
      {
         write_client(clients[idClient].sock, message);
      }
   }
}

static void client_joins(Client *clients, int actualClients, Channel *channels, int actualChannels, int idClient, const char *channelName)
{
   int idChannel;
   for (idChannel = 0; idChannel < actualChannels; idChannel++)
   {
      if (strcmp(channels[idChannel].name, channelName) == 0)
      {
         break;
      }
   }

   if (idChannel == actualChannels)
   {
      // Unknown channel
      printf("[SERVER] Unknown channel '%s'\n", channelName);
      
      return;
   }

   clients[idClient].channelId = idChannel;

   char welcomeMessage[BUF_SIZE];
   strncpy(welcomeMessage, clients[idClient].name, BUF_SIZE - 1);
   strncat(welcomeMessage, " connected !", BUF_SIZE - strlen(welcomeMessage) - 1);
   send_message_to_all_clients(clients, actualClients, channels, clients[idClient], welcomeMessage, 1);
                           
   printf("[SERVER] User '%s' joined channel '%s'\n", clients[idClient].name, channelName);

   // Print missing conversation to the client
   char channelNameBuffer[BUF_SIZE];
   strncpy(channelNameBuffer, "channels/", BUF_SIZE - 1);
   strncat(channelNameBuffer, channelName, sizeof channelNameBuffer - strlen(channelNameBuffer) - 1);

   FILE *fp = fopen(channelNameBuffer, "r");
   if (fp == NULL)
   {
      printf("[SERVER] An error occured while opening the file '%s'\n", channelNameBuffer);
      //exit(EXIT_FAILURE);
   }
   else
   {
      char *message = NULL;
      size_t len = 0;

      char conversation[1000][BUF_SIZE];

      int messages = 0;

      while (getline(&message, &len, fp) != -1)
      {      
         int t = atoi(strtok(message, "|"));
         
         char content[BUF_SIZE];
         strncpy(content, strtok(NULL, "|"), BUF_SIZE - 1);
         
         if (t > clients[idClient].lastConnection)
         {
            content[strcspn(content, "\n")] = 0;
            strncpy(conversation[messages], content, BUF_SIZE - 1);
            messages++;
         }
      }

      fclose(fp);

      for (int j = 0; j < messages; j++)
      {
         // Print conversation
         write_client(clients[idClient].sock, conversation[j]);
      }
   }
}

static int init_connection(void)
{
   SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
   SOCKADDR_IN sin = { 0 };

   if(sock == INVALID_SOCKET)
   {
      perror("socket()");
      exit(errno);
   }

   sin.sin_addr.s_addr = htonl(INADDR_ANY);
   sin.sin_port = htons(PORT);
   sin.sin_family = AF_INET;

   if(bind(sock,(SOCKADDR *) &sin, sizeof sin) == SOCKET_ERROR)
   {
      perror("bind()");
      exit(errno);
   }

   if(listen(sock, MAX_CLIENTS) == SOCKET_ERROR)
   {
      perror("listen()");
      exit(errno);
   }

   return sock;
}

static void end_connection(int sock)
{
   closesocket(sock);
}

static int read_client(SOCKET sock, char *buffer)
{
   int n = 0;

   if((n = recv(sock, buffer, BUF_SIZE - 1, 0)) < 0)
   {
      perror("recv()");
      /* if recv error we disonnect the client */
      n = 0;
   }

   buffer[n] = 0;

   return n;
}

static void write_client(SOCKET sock, const char *buffer)
{
   if(send(sock, buffer, strlen(buffer), 0) < 0)
   {
      perror("send()");
      exit(errno);
   }
}

int main(int argc, char **argv)
{
   init();

   app();

   end();

   return EXIT_SUCCESS;
}
