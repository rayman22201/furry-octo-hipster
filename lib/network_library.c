/**
* @File network_library.c
* CS 470 Assignment 2
* Ray Imber
* Implements common networking functions into a common library for use with all the various pieces
*/

//std lib stuff
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//networking stuff
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

//so my syntax checker doesn't bother me with false positives
extern FILE *popen();
extern int getdomainname(char *name, size_t len);
extern int gethostname(char *name, size_t len);
extern void herror(const char *s);
extern char *strtok_r(char *str, const char *delim, char **saveptr);

void parse_host_string(char* buffer, char* name, int* portNum)
{
  char* savePtr;
  char myBuffer[512];
  strcpy(myBuffer, buffer);
  strcpy(name, strtok_r(myBuffer, ":", &savePtr));
  (*portNum) = atoi(strtok_r(NULL, ":", &savePtr));
}

void parse_host_info(const char* buffer, char* myHostName, int* myPort)
{
  // Read the port I am supposed to use off the command line
  if(strstr(buffer, ":") == NULL)
  {
    // No Host name was supplied by the user. Attempt to figure out my Network Host name
    getdomainname(myHostName, 255);
    if(strstr(myHostName, "(none)") != NULL)
    {
      gethostname(myHostName, 255);
    }

    (*myPort) = atoi(buffer);
  }
  else
  {
    // Use the network Host name provided by the argument
    char localServerStr[255];
    strcpy(localServerStr, buffer);
    parse_host_string(localServerStr, myHostName, myPort);
  }
}

int hostname_to_ip(char * hostname , char* ip)
{
    struct hostent *he;
    struct in_addr **addr_list;
    int i;
    he = gethostbyname( hostname );
    if ( he == NULL)
    {
        // get the host info
        herror("gethostbyname");
        exit(1);
        return 1;
    }
 
    addr_list = (struct in_addr **) he->h_addr_list;
     
    for(i = 0; addr_list[i] != NULL; i++)
    {
        //Return the first one;
        strcpy(ip , inet_ntoa(*addr_list[i]) );
        return 0;
    }
     
    return 1;
}

int tcp_connect(char* serverName, int serverPort)
{
  int sockfd;
  struct sockaddr_in serv_addr;
  memset(&serv_addr, '0', sizeof(serv_addr));

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if(sockfd < 0)
  {
    printf("\n Error : Could not create socket \n");
    exit(1);
    return -1;
  }

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(serverPort);
  
  char serverIP[100];
  hostname_to_ip(serverName, serverIP);
  if(inet_pton(AF_INET, serverIP, &serv_addr.sin_addr)<=0)
  {
    printf("\n inet_pton error occured\n");
    exit(1);
    return -1;
  }

  if( connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
  {
    printf("\n Error : Connect Failed \n");
    exit(1);
    return -1;
  }

  return sockfd;
}

int tcp_listen(int port)
{
  int listenfd = 0;
  struct sockaddr_in serv_addr;
  memset(&serv_addr, '0', sizeof(serv_addr));

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(port);

  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if( (bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) < 0)
  {
    printf("Unable to bind to port: %d\n", port);
    exit(1);
    return -1;
  }

  listen(listenfd, 10);

  return listenfd;
}
