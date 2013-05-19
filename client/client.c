/**
 * @File
 * Client.c
 * Ray Imber
 * CS 470
 * The client implementation for the custom Torrent protocol. A DSFT Protocol: Distributed Segmented File Transfer Protocol. :-P
 */

// System Libraries
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <libgen.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <dirent.h>

// Custom Libraries.
#include "../lib/sha1/xsha1.h"
#include "../lib/hash_table.c"
#include "../lib/linked_list.c"
#include "../lib/network_library.c"

// Make my syntax checker leave me alone.
extern char *strdup(const char *s);
extern char *strtok_r(char *str, const char *delim, char **saveptr);

// The size in bytes of 1 file Segment. This is used for Torrents I create only.
#define SEGMENT_SIZE  256
#define META_EXTENSION ".trrnt"

#define TRUE 1
#define FALSE 0

typedef struct {
  char fileName[255];
  char trackerName[255];
  int  trackerPort;
  unsigned long fileSize;
  unsigned long numSegments;
  unsigned long segmentSize;
  
  // An array of booleans that tell us which segments are finished downloading
  int* segmentStatus;
  
  // A queue of segments to download
  linkedListStruct* downloadQueue;

  // This is a 2D array of string representations of SHA1 hashes indexed on the segment number. SHA1 strings are always 40 characters.
  char** hash;
} MetaData;

typedef struct {
  MetaData* torrent;
  linkedListNode* threadPoolID;
  char targetClientName[255];
  int targetClientPort;
} ThreadArgs;

char myHostName[255];
int myPort;

unsigned int nextThreadPoolID;
linkedListStruct* threadPool;
pthread_cond_t  threadPoolCondition;

void assemble_torrent_segments(MetaData* curTorrent)
{
  char segmentFilePath[512];
  char finishedFilePath[512];
  char fileBuffer[curTorrent->segmentSize];
  FILE* segmentFilePtr;
  FILE* finishedFilePtr;
  int i;

  //open finished file for writing
  memset(finishedFilePath, '\0', 512);
  sprintf(finishedFilePath, "./done/%s", curTorrent->fileName);

  //first check if the file was already downloaded
  finishedFilePtr = fopen(finishedFilePath, "r");
  if(finishedFilePtr)
  {
    printf("File Already Downloaded!\n");
    fclose(finishedFilePtr);
    exit(1);
  }

  finishedFilePtr = fopen(finishedFilePath, "w");

  for(i = 0; i < curTorrent->numSegments; i++)
  {
    memset(segmentFilePath, '\0', 512);
    sprintf(segmentFilePath, "./temp/%s.segment_%i", curTorrent->fileName, i);
    segmentFilePtr = fopen(segmentFilePath, "r");
    if(segmentFilePtr)
    {
      fread(fileBuffer, sizeof(char), curTorrent->segmentSize, segmentFilePtr);
      fwrite(fileBuffer, sizeof(char), curTorrent->segmentSize, finishedFilePtr);
      fclose(segmentFilePtr);
    }
    else
    {
      fclose(finishedFilePtr);
      remove(finishedFilePath);
      printf("Unable to open %s for file assembly.\n", segmentFilePath);
      exit(1);
    }
  }

  // Don't attempt to delete the temp file until we have successfully assembled the whole file.
  // This way we guarantee we won't need the temp file in the future.
  for(i = 0; i < curTorrent->numSegments; i++)
  {
    memset(segmentFilePath, '\0', 512);
    sprintf(segmentFilePath, "./temp/%s.segment_%i", curTorrent->fileName, i);
    remove(segmentFilePath);
  }

  fclose(finishedFilePtr);
}

int verify_hashStr(char* hashA, char* hashB)
{
  if(strstr(hashA, hashB) != NULL)
  {
    return TRUE;
  }
  return FALSE;
}

int verify_fileHash(FILE* filePtr, char* hash)
{
  char fileBuffer[SEGMENT_SIZE];
  char fileHashStr[41];
  uint32_t hashBuffer[5];

  //Generate Hash from the file
  memset(fileBuffer, '\0', SEGMENT_SIZE);
  int bytes_read = fread(fileBuffer, sizeof(char), SEGMENT_SIZE, filePtr);
  xsha1_calcHashBuf(fileBuffer, SEGMENT_SIZE, (uint32_t *)hashBuffer);
  sprintf(fileHashStr, "%08x%08x%08x%08x%08x", hashBuffer[0], hashBuffer[1], hashBuffer[2], hashBuffer[3], hashBuffer[4]);
  
  //verify the hash
  if(verify_hashStr(fileHashStr, hash))
  {
    return TRUE;
  }
  return FALSE;
}

int verify_bufferHash(void* buffer, char* hash)
{
  char bufferHashStr[41];
  uint32_t hashBuffer[5];

  xsha1_calcHashBuf(buffer, SEGMENT_SIZE, (uint32_t *)hashBuffer);
  sprintf(bufferHashStr, "%08x%08x%08x%08x%08x", hashBuffer[0], hashBuffer[1], hashBuffer[2], hashBuffer[3], hashBuffer[4]);
  //printf("buffer hash: %s\n", bufferHashStr);
  //printf("arg hash   : %s\n", hash);
  //verify the hash
  if(verify_hashStr(bufferHashStr, hash))
  {
    return TRUE;
  }
  return FALSE;

}

void print_MetaData(MetaData* curTorrent, int printHash)
{
  printf("Parsed File:\n");
  printf("  Filename:%s\n", curTorrent->fileName);
  printf("  Tracker URL:%s:%i\n", curTorrent->trackerName, curTorrent->trackerPort);
  printf("  FileSize:%lu\n", curTorrent->fileSize);
  printf("  numSegments:%lu\n", curTorrent->numSegments);
  printf("  segmentSize:%lu\n", curTorrent->segmentSize);
  int done = TRUE;

  if(printHash == TRUE)
  {
    printf("  Hash:\n");
    int i;
    for(i = 0; i < curTorrent->numSegments; i++)
    {
      printf("    Segment %i: %s\n", i, curTorrent->hash[i]);
      if(curTorrent->segmentStatus[i] == TRUE)
      {
        printf("      Finished: Yes\n");
      }
      else
      {
        done = FALSE;
        printf("      Finished: No\n");
      }
    }

    if(done == TRUE)
    {
      printf("  Torrent Status: Done\n");
    }
    else
    {
      printf("  Torrent Status: Incomplete\n");
    }
  }
  else
  {
    int i;
    for(i = 0; i < curTorrent->numSegments; i++)
    {
      if(curTorrent->segmentStatus[i] == FALSE)
      {
        done = FALSE;
      }
    }
  }

  if(done == TRUE)
  {
    printf("  Torrent Status: Done\n");
  }
  else
  {
    printf("  Torrent Status: Incomplete\n");
  }
}

MetaData* parse_meta_file(char* filePath)
{
  MetaData* torrentData;
  torrentData = malloc(sizeof(MetaData));

  char* fileBuffer;
  FILE* filePtr;
  struct stat fileStats;
  filePtr = fopen(filePath, "r");
  stat(filePath, &fileStats);

  // Read the whole damn file into a buffer
  fileBuffer = malloc( fileStats.st_size );
  fread(fileBuffer, sizeof(char), fileStats.st_size, filePtr);

  strcpy(torrentData->fileName, strtok(fileBuffer, "\n"));
  
  char trackerURL[255]; 
  strcpy(trackerURL, strtok(NULL, "\n"));
  parse_host_info(trackerURL, torrentData->trackerName, &(torrentData->trackerPort));

  torrentData->fileSize = strtoul(strtok(NULL, "\n"), NULL, 0);
  torrentData->numSegments = strtoul(strtok(NULL, "\n"), NULL, 0);
  torrentData->segmentSize = strtoul(strtok(NULL, "\n"), NULL, 0);

  //attempt to find the file in the done folder
  char doneFilePath[512];
  FILE* doneFilePtr;
  int done = FALSE;
  sprintf(doneFilePath, "./done/%s", torrentData->fileName);
  doneFilePtr = fopen(doneFilePath, "r");
  if( doneFilePtr )
  {
    done = TRUE;
    fclose(doneFilePtr);
  }

  torrentData->hash = malloc( (sizeof(char*) * torrentData->numSegments) );
  torrentData->segmentStatus = malloc( sizeof(int) * torrentData->numSegments );
  int i;
  char* curHash;
  for(i = 0; i < torrentData->numSegments; i++)
  {
    curHash = malloc(sizeof(char) * 41);
    strcpy(curHash, strtok(NULL, "\n"));
    torrentData->hash[i] = curHash;
    torrentData->segmentStatus[i] = FALSE;
    if(done == TRUE)
    {
      torrentData->segmentStatus[i] = TRUE;
    }
    else
    {
      memset(doneFilePath, '\0', 512);
      sprintf(doneFilePath, "./temp/%s.segment_%i", torrentData->fileName, i);
      doneFilePtr = fopen(doneFilePath, "r");
      if( doneFilePtr )
      {
        if(verify_fileHash(doneFilePtr, torrentData->hash[i]))
        {
          torrentData->segmentStatus[i] = TRUE;
        }
        fclose(doneFilePtr);
      }
    }
  }

  return torrentData;
}

char** generate_hash_from_file(FILE* fp, unsigned long numSegments)
{
  char** fullHash;
  char* segmentHash;
  char segmentBuffer[SEGMENT_SIZE];

  fullHash = malloc( (sizeof(char*) * numSegments) );

  int i;
  int bytes_read;
  uint32_t hashBuffer[5];
  for(i = 0; i < numSegments; i++)
  {
    memset(segmentBuffer, '\0', SEGMENT_SIZE);
    bytes_read = fread(segmentBuffer, sizeof(char), SEGMENT_SIZE, fp);
    xsha1_calcHashBuf(segmentBuffer, SEGMENT_SIZE, (uint32_t *)hashBuffer);
    //The hash is 40 characters + 1 end of string character
    segmentHash = malloc( (sizeof(char) * 41) );
    sprintf(segmentHash, "%08x%08x%08x%08x%08x", hashBuffer[0], hashBuffer[1], hashBuffer[2], hashBuffer[3], hashBuffer[4]);
    fullHash[i] = segmentHash;
  }

  return fullHash;
}

MetaData* create_meta_file(char* filePath, char* trackerURL)
{
  char fileName_no_ext[255];
  char tempBuffer[255];
  char metaFilePath[255];
  struct stat fileStats;
  MetaData* newTorrent;
  FILE* filePtr;
  FILE* metaFP;

  newTorrent = malloc(sizeof(MetaData));

  //Get File Name
  memset(tempBuffer, '\0',255);
  strcpy(tempBuffer,filePath);
  strcpy(newTorrent->fileName, basename(tempBuffer));

  char trackerName[255];
  int trackerPort;
  parse_host_info(trackerURL, trackerName, &trackerPort);
  strcpy(newTorrent->trackerName, trackerName);
  newTorrent->trackerPort = trackerPort;

  // Cut off any extension the file may have
  memset(tempBuffer, '\0',255);
  strcpy(tempBuffer, newTorrent->fileName);
  strcpy(fileName_no_ext, strtok(tempBuffer, "."));

  //File Size
  stat(filePath, &fileStats);
  newTorrent->fileSize = fileStats.st_size;

  //Calculate # of Segments
  newTorrent->numSegments = newTorrent->fileSize / SEGMENT_SIZE;
  if( (newTorrent->fileSize % SEGMENT_SIZE) > 0)
  {
    // Add an extra segment for the remainder
    newTorrent->numSegments ++;
  }
  
  //Create new meta file for writing
  //For simplicity just create the meta file in the current directory
  sprintf(metaFilePath, "./%s%s",fileName_no_ext, META_EXTENSION);
  metaFP = fopen(metaFilePath, "w");
  
  //write the meta file
  fprintf(metaFP, "%s\n%s:%i\n%lu\n%lu\n%i\n",newTorrent->fileName, newTorrent->trackerName, newTorrent->trackerPort, newTorrent->fileSize, newTorrent->numSegments, SEGMENT_SIZE);

  //Open the file
  filePtr = fopen(filePath, "r");

  //Calculate Hash
  newTorrent->hash = generate_hash_from_file(filePtr, newTorrent->numSegments);
  fclose(filePtr);

  //write the hash to the meta file
  int i;
  for(i = 0; i < newTorrent->numSegments; i++)
  {
    fprintf(metaFP, "%s\n", newTorrent->hash[i]);
  }
  fclose(metaFP);

  return newTorrent;
}

int process_client_response(void* dataBuffer, const char* responseBuffer, char* hash)
{
  char myBuffer[1024];
  char* curToken;
  char* savePtr;

  memset(myBuffer, '\0', 1024);
  memcpy(myBuffer, responseBuffer, sizeof(myBuffer));
  curToken = strtok_r(myBuffer, "/", &savePtr);
  if(strstr(curToken, "BUSY") != NULL)
  {
    printf("The Client was busy. >:-(\n");
  }
  else if(strstr(curToken, "UNAVAILABLE") != NULL)
  {
    printf("The client did not have the segment we are looking for. :'-(\n");
  }
  else if(strstr(curToken, "HAZ") != NULL)
  {
    curToken = strtok_r(NULL, "/", &savePtr);
    if(verify_hashStr(curToken, hash))
    {
      printf("    The Client has the segment. :-D\n");
      curToken = strtok_r(NULL, "/", &savePtr);
      if(strstr(curToken, "START") != NULL)
      {
        // Can't use strtok because it could destroy the data. Instead, manually increment the pointer by sizeof("START/0");
        curToken += 6;
        memset(dataBuffer, '\0', SEGMENT_SIZE);
        memcpy(dataBuffer, curToken, SEGMENT_SIZE );
        if(verify_bufferHash(dataBuffer, hash))
        { 
          printf("    Segment Hash successfully verified. :-)\n");
          return TRUE;
        }
        else
        {
          printf("    Segment Hash Verfication failed.\n");
        }
      }
      else
      {
        printf("Malformed Transfer packet.\n");
      }
    }
    else
    {
      printf("Unable to verify that this is the segment we are looking for. :-[\n");
    }
  }
  else
  {
    printf("The Client sent a response that I don't understand. :-S\n");
  }
  return FALSE;
}

int i_am_busy()
{
  return FALSE;
}

void* find_segment(char* fileName, int segmentNumber, char* hash)
{
  char doneFilePath[512];
  char segmentFilePath[512];
  char* dataBuffer;
  FILE* filePtr;

  sprintf(doneFilePath, "./done/%s", fileName);
  sprintf(segmentFilePath, "./temp/%s.segment_%i", fileName, segmentNumber);

  filePtr = fopen(doneFilePath, "r");
  if( filePtr )
  {
    // We have a finished copy of the file
    dataBuffer = malloc( SEGMENT_SIZE );
    memset(dataBuffer, '\0', SEGMENT_SIZE);

    // Go to the correct spot in the file
    unsigned long offset = ((unsigned long)segmentNumber * (unsigned long)SEGMENT_SIZE);
    fseek(filePtr, offset, SEEK_SET);
    
    // Read the segment
    int bytes_read = fread(dataBuffer, sizeof(char), SEGMENT_SIZE, filePtr);
    fclose(filePtr);
    return dataBuffer;
  }
  else
  {
    filePtr = fopen(segmentFilePath, "r");
    if( filePtr )
    {
      // We have the segment we are looking for.
      dataBuffer = malloc( SEGMENT_SIZE  );
      memset(dataBuffer, '\0', SEGMENT_SIZE);
      int bytes_read = fread(dataBuffer, sizeof(char), SEGMENT_SIZE, filePtr);
      fclose(filePtr);
      return dataBuffer;
    }
  }
  return NULL;
}

int process_client_request(char* responseBuffer, const char* requestBuffer)
{
  char myBuffer[512];
  char* curToken;
  char* savePtr;
  int charCount = 0;

  strcpy(myBuffer, requestBuffer);
  
  // Use strtok_r to make it thread safe
  curToken = strtok_r(myBuffer, "/", &savePtr);
  if(strstr(curToken, "CANHAZ") != NULL)
  {
    // Am I busy?
    // if yes -> respond with BUSY packet
    if(i_am_busy())
    {
      charCount = sprintf(responseBuffer, "BUSY");
    }
    else
    {
      char fileName[255];
      char hash[41];
      int segmentNumber;
      //  save the client info for reference
      curToken = strtok_r(NULL, "/", &savePtr);
      //  parse the filename
      strcpy(fileName, strtok_r(NULL, "/", &savePtr));
      //  parse the segmentnumber
      segmentNumber = atoi(strtok_r(NULL, "/", &savePtr));
      //  parse the hash
      strcpy(hash, strtok_r(NULL, "/", &savePtr));
      //  Do I have this segment?
      //    If yes -> send the data
      //    If No -> send a 'no' packet
      char* dataBuffer = find_segment(fileName, segmentNumber, hash);
      if( dataBuffer != NULL )
      {
        int numCharsWritten = sprintf(responseBuffer,"HAZ/%s/START/",hash);
        responseBuffer += numCharsWritten;
        memcpy(responseBuffer, dataBuffer, SEGMENT_SIZE  );
        free(dataBuffer);
        charCount = (numCharsWritten + SEGMENT_SIZE);
      }
      else
      {
        charCount = sprintf(responseBuffer,"HAZNOT/%s/%i/%s/",fileName, segmentNumber, hash);
      }
    }
  }
  else
  {
    printf("Another client sent a request that I cannot process.\n");
  }
  //account for a final terminating character
  charCount++;
  return charCount;
}

void listen_for_requests()
{
  char requestBuffer[512];
  char responseBuffer[1024];
  int connfd;
  int listenfd = tcp_listen(myPort);
  printf("Listening for Requests.\n");
  while(TRUE)
  {
    memset(requestBuffer, '\0', sizeof(requestBuffer));
    memset(responseBuffer, '\0', sizeof(responseBuffer));

    connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);
    read(connfd, requestBuffer, sizeof(requestBuffer)-1);
    int bufferSize = process_client_request(responseBuffer, requestBuffer);
    if(bufferSize > 0)
    {
      write(connfd, responseBuffer, bufferSize);
    }
    close(connfd);
  }
}

void prepare_download_queue(MetaData* curTorrent)
{
  curTorrent->downloadQueue = linkedList_newList_ts();
  
  int i;
  int* pieceNumber;
  for(i = 0; i < curTorrent->numSegments; i++)
  {
    if( curTorrent->segmentStatus[i] == FALSE )
    {
      pieceNumber = malloc(sizeof(int));
      (*pieceNumber) = i;
      linkedList_addNode(curTorrent->downloadQueue, pieceNumber);
    }
  }
}

int save_segment_to_file(char* dataBuffer, char* fileName, int segmentNumber)
{
  FILE* filePtr;
  char segmentFilePath[512];
  memset(segmentFilePath,'\0',sizeof(segmentFilePath));
  sprintf(segmentFilePath, "./temp/%s.segment_%i", fileName, segmentNumber);
  filePtr = fopen(segmentFilePath, "w");
  if( filePtr )
  {
    fwrite(dataBuffer, sizeof(char), SEGMENT_SIZE, filePtr);
    fclose(filePtr);
    return TRUE;
  }
  return FALSE;
}

void* request_listener_thread(void* arg)
{
  listen_for_requests();
  pthread_exit(NULL);
}

void* download_worker_thread(void* arg)
{
  ThreadArgs* myArgs = (ThreadArgs*)arg;
  int* segmentNumber;
  char *dataBuffer;
  char responseBuffer[1024];

  char requestString[512];
  int bytes_read;
  int response;
  dataBuffer = malloc(1024);

  while( linkedList_isEmptyList_ts(myArgs->torrent->downloadQueue) == FALSE )
  {
    segmentNumber = (int*)linkedList_pop_ts(myArgs->torrent->downloadQueue);
    printf("Downloading Segment: %i of %lu\n", (*segmentNumber), myArgs->torrent->numSegments);
    memset(dataBuffer, '\0', 1024);
    memset(responseBuffer, '\0', sizeof(responseBuffer));
    
    sprintf(requestString, "CANHAZ/%s:%i/%s/%i/%s/", myHostName, myPort, myArgs->torrent->fileName, (*segmentNumber), myArgs->torrent->hash[(*segmentNumber)]);

    response = FALSE;
    int connfd = tcp_connect(myArgs->targetClientName, myArgs->targetClientPort);
    if(connfd != -1)
    {
      write(connfd, requestString, strlen(requestString));
      bytes_read = read(connfd, responseBuffer, sizeof(responseBuffer));
      close(connfd);
      response = process_client_response(dataBuffer, responseBuffer, myArgs->torrent->hash[(*segmentNumber)]);
    }

    if(response == FALSE)
    {
      linkedList_addNode_ts(myArgs->torrent->downloadQueue, segmentNumber);
      pthread_mutex_lock(&(threadPool->lock));
      linkedList_removeNode(threadPool, myArgs->threadPoolID);
      pthread_cond_signal(&threadPoolCondition);
      pthread_mutex_unlock(&(threadPool->lock));
      pthread_exit(NULL);
    }
    else
    {
      // Save the dataBuffer to a file
      save_segment_to_file(dataBuffer, myArgs->torrent->fileName, (*segmentNumber));
      free(segmentNumber);    
    }
  }

  // Wake up the manager thread so that it can put all the pieces together.
  pthread_mutex_lock(&(threadPool->lock));
  linkedList_removeNode(threadPool, myArgs->threadPoolID);
  pthread_cond_signal(&threadPoolCondition);
  pthread_mutex_unlock(&(threadPool->lock));
  pthread_exit(NULL);
}

int parse_tracker_response(char* responseBuffer, int* numSeeders, char*** seederNames, int** seederPorts)
{
  char* curToken;
  char* savePtr;
  curToken = strtok_r(responseBuffer, "/", &savePtr);
  if(strstr(curToken, "SEEDERS"))
  {
    // Filename
    strtok_r(NULL, "/", &savePtr);
    (*numSeeders) = atoi(strtok_r(NULL, "/", &savePtr));
    if((*numSeeders) < 1)
    {
      printf("No Seeders Found. \n");
      exit(0);
      return FALSE;
    }
    
    (*seederNames) = malloc(sizeof(char*) * (*numSeeders));  
    (*seederPorts) = malloc(sizeof(int) * (*numSeeders));
    char* curSeederName;

    int i;
    for(i = 0; i < (*numSeeders); i++)
    {
      curSeederName = malloc(sizeof(char) * 512);
      curToken = strtok_r(NULL, "/", &savePtr);
      parse_host_info(curToken, curSeederName, &((*seederPorts)[i]));
      (*seederNames)[i] = curSeederName;
    }
    return TRUE;
  }
  else
  {
    printf("Invalid Tracker response");
  }
  return FALSE;
}

int broadcast_file_to_tracker(char* fileName, char* trackerName, int trackerPort)
{
  char trackerRequestStr[512];
  char trackerResponseStr[512];

  // Become a seeder
  memset(trackerRequestStr, '\0', sizeof(trackerRequestStr));
  sprintf(trackerRequestStr, "INIT/%s:%i/%s", myHostName, myPort, fileName);

  int connfd = tcp_connect(trackerName, trackerPort);
  if(connfd == -1)
  {
    printf("failed to connect to the tracker.\n");
    return FALSE;
  }

  write(connfd, trackerRequestStr, strlen(trackerRequestStr));
  read(connfd, trackerResponseStr, (sizeof(trackerResponseStr) - 1) );
  close(connfd);

  if(strstr(trackerResponseStr, "SUCCESS") == NULL)
  {
    printf("Tracker sent an invalid response to our seed request for %s.\n", fileName);
  }
  else
  {
    printf("Now Seeding %s\n", fileName);
    return TRUE;
  }
  return FALSE;
}

void* download_manager_thread(void* arg)
{
  MetaData* curTorrent = (MetaData*)arg;
  prepare_download_queue(curTorrent);

  threadPool = linkedList_newList_ts();
  pthread_cond_init(&threadPoolCondition, NULL);
  nextThreadPoolID = 0;

  int connfd;
  int threadsToAdd;
  char** targetClientName;
  int* targetClientPort;

  char trackerRequestStr[512];
  char trackerResponseStr[512];

  // While we still have parts to download. _ts = thread-safe check
  while(linkedList_isEmptyList_ts(curTorrent->downloadQueue) == FALSE)
  {
    pthread_mutex_lock(&(threadPool->lock));
    if( threadPool->numNodes < 1 )
    {

      memset(trackerRequestStr, '\0', sizeof(trackerRequestStr));
      memset(trackerResponseStr, '\0', sizeof(trackerResponseStr));
      // First tracker request
      if(nextThreadPoolID == 0)
      {
        sprintf(trackerRequestStr, "STARTED/%s:%i/%s", myHostName, myPort, curTorrent->fileName);
      }
      else
      {
        sprintf(trackerRequestStr, "NEEDY/%s:%i/%s", myHostName, myPort, curTorrent->fileName);
      }
      //Ask the tracker for some seeders
      connfd = tcp_connect(curTorrent->trackerName, curTorrent->trackerPort);
      write(connfd, trackerRequestStr, strlen(trackerRequestStr));
      read(connfd, trackerResponseStr, sizeof(trackerResponseStr));
      close(connfd);
      parse_tracker_response(trackerResponseStr, &threadsToAdd, &targetClientName, &targetClientPort);    

      //spin off a thread for each seeder
      int* threadPoolID;
      pthread_t threadID;
      ThreadArgs* newArg;
      linkedListNode* threadPoolNode;

      int i;
      for(i = 0; i < threadsToAdd; i++)
      {
        nextThreadPoolID ++;
        newArg = malloc(sizeof(ThreadArgs));
        threadPoolID = malloc(sizeof(int));
        (*threadPoolID) = nextThreadPoolID;
        threadPoolNode = linkedList_addNode(threadPool, threadPoolID);
        newArg->threadPoolID = threadPoolNode;
        newArg->torrent = curTorrent;
        strcpy(newArg->targetClientName, targetClientName[i]);
        free(targetClientName[i]);
        newArg->targetClientPort = targetClientPort[i];

        pthread_create( &threadID, NULL, download_worker_thread, (void*)newArg);

      }

      free(targetClientName);
      free(targetClientPort);
    }
    pthread_cond_wait(&threadPoolCondition, &(threadPool->lock));
    pthread_mutex_unlock(&(threadPool->lock));
  }

  // Tell the tracker we are done
  memset(trackerRequestStr, '\0', sizeof(trackerRequestStr));
  sprintf(trackerRequestStr, "STOPPED/%s:%i", myHostName, myPort);
  connfd = tcp_connect(curTorrent->trackerName, curTorrent->trackerPort);
  write(connfd, trackerRequestStr, strlen(trackerRequestStr));
  close(connfd);

  assemble_torrent_segments(curTorrent);
  printf("%s Has finished Downloading!\n\n", curTorrent->fileName);

  // Become a seeder
  broadcast_file_to_tracker(curTorrent->fileName, curTorrent->trackerName, curTorrent->trackerPort);

  pthread_exit(NULL);
}

void broadcast_finished_files(char* trackerName, int trackerPort)
{
  DIR *dirPtr;
  struct dirent *curFile;
  dirPtr = opendir ("./done");

  if (dirPtr != NULL)
  {
    curFile = readdir(dirPtr);
    while (curFile != NULL)
    {
      if( strcspn(curFile->d_name, ".") > 0 )
      {
        broadcast_file_to_tracker(curFile->d_name, trackerName, trackerPort);
      }
      curFile = readdir(dirPtr);
    }
    (void) closedir (dirPtr);
  }
  else
  {
    perror ("Couldn't open the directory");
  }
  return;
}

/**
* Main
* @param  argc  number of arguments
* @param  argv  array of arguments
* @return   boolean success or failure
*/
int main(int argc, char* argv[])
{
  if(argc > 1)
  {
    if( (strstr(argv[1], "new") != NULL) )
    {
      if(argc < 4)
      {
        printf("Please enter a filename and a tracker url\n");
        exit(1);
      }
      else
      {
        MetaData* newTorrent;
        newTorrent = create_meta_file(argv[2], argv[3]);
      }
    }
    else if( (strstr(argv[1], "read")) )
    {
      if(argc != 3)
      {
        printf("Please enter the name of a torrent file!\n");
        exit(1);
      }
      else
      {
        MetaData* curTorrent;
        curTorrent = parse_meta_file(argv[2]);
        print_MetaData(curTorrent, FALSE);
      }
    }
    else if( (strstr(argv[2], "start")) )
    {
      if(argc != 4)
      {
        printf("Invalid arguments.\n  Proper usage:\n   client.o name:port start filename.trrnt\n");
        exit(1);
      }
      else
      {
        parse_host_info(argv[1], myHostName, &myPort);
        printf("Using HostName: %s:%i\n", myHostName, myPort);

        MetaData* curTorrent;
        curTorrent = parse_meta_file(argv[3]);
        
        pthread_t threads[2];
        // Listen for requests
        pthread_create(&(threads[0]), NULL, request_listener_thread, NULL);
        // Start the download
        pthread_create(&(threads[1]), NULL, download_manager_thread, (void*)curTorrent);
        
        //sync on the threads
        pthread_join(threads[0], NULL);
        pthread_join(threads[1], NULL);

        linkedList_free_ts(threadPool);
        pthread_cond_destroy(&threadPoolCondition);
      }
    }
    else if( (strstr(argv[2], "listen")) )
    {
      if( !(argc == 3 || argc == 5) )
      {
        printf("Invalid arguments. Please enter a port and/or hostname.\n");
        exit(1);
      }
      else
      {
        parse_host_info(argv[1], myHostName, &myPort);
        printf("Using HostName: %s:%i\n", myHostName, myPort);
        
        if( (argc == 5 && strstr(argv[3], "tracker") != NULL) )
        {
            char trackerName[255];
            int trackerPort;
            parse_host_info(argv[4], trackerName, &trackerPort);
            broadcast_finished_files(trackerName, trackerPort);
        }

        listen_for_requests();
      }
    }
  }
}
