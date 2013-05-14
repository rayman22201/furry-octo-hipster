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
  char trackerURL[255];
  unsigned long fileSize;
  unsigned long numSegments;
  unsigned long segmentSize;
  //An array of booleans that tell us which segments are finished downloading
  int* segmentStatus;
  //This is a 2D array of string representations of SHA1 hashes indexed on the segment number. SHA1 strings are always 40 characters.
  char** hash;
} MetaData;

void assemble_torrent_pieces(MetaData* curTorrent)
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
  finishedFilePtr = fopen(finishedFilePath, "w");

  for(i = 0; i < curTorrent->numSegments; i++)
  {
    memset(segmentFilePath, '\0', 512);
    sprintf(segmentFilePath, "./temp/%s.part_%i", curTorrent->fileName, i);
    segmentFilePtr = fopen(segmentFilePath, "r");
    if(segmentFilePtr)
    {
      fread(fileBuffer, sizeof(char), curTorrent->segmentSize, segmentFilePtr);
      fwrite(fileBuffer, sizeof(char), curTorrent->segmentSize, finishedFilePtr);
      fclose(segmentFilePtr);
    }
    else
    {
      printf("Unable to open %s for file assembly.\n", segmentFilePath);
      exit(1);
    }
  }

  // Don't attempt to delete the temp file until we have successfully assembled the whole file.
  // This way we guarantee we won't need the temp file in the future.
  for(i = 0; i < curTorrent->numSegments; i++)
  {
    memset(segmentFilePath, '\0', 512);
    sprintf(segmentFilePath, "./temp/%s.part_%i", curTorrent->fileName, i);
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

void print_MetaData(MetaData* curTorrent, int printHash)
{
  printf("Parsed File:\n");
  printf("  Filename:%s\n", curTorrent->fileName);
  printf("  Tracker URL:%s\n", curTorrent->trackerURL);
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
  strcpy(torrentData->trackerURL, strtok(NULL, "\n"));
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
    if(done == TRUE)
    {
      torrentData->segmentStatus[i] = TRUE;
    }
    else
    {
      memset(doneFilePath, '\0', 512);
      sprintf(doneFilePath, "./temp/%s.part_%i", torrentData->fileName, i);
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

  strcpy(newTorrent->trackerURL, trackerURL);

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
  fprintf(metaFP, "%s\n%s\n%lu\n%lu\n%i\n",newTorrent->fileName, newTorrent->trackerURL, newTorrent->fileSize, newTorrent->numSegments, SEGMENT_SIZE);

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
  strcpy(myBuffer, responseBuffer);
  curToken = strtok_r(myBuffer, "/", &savePtr);
  if(strstr(curToken, "BUSY") != NULL)
  {
    printf("The Client was busy. >:-(\n");
  }
  else if(strstr(curToken, "UNAVAILABLE") != NULL)
  {
    printf("The client did not have the piece we are looking for. :'-(\n");
  }
  else if(strstr(curToken, "HAZ") != NULL)
  {
    curToken = strtok_r(NULL, "/", &savePtr);
    if(verify_hashStr(curToken, hash))
    {
      printf("The Client has the Piece. :-D\n");
      curToken = strtok_r(NULL, "/", &savePtr);
      if(strstr(curToken, "START") != NULL)
      {
        //Can't use strtok because it could destroy the data. Instead, manually increment the pointer by sizeof("START/0");
        curToken += (6 * sizeof(char));
        memcpy(dataBuffer, curToken, ( SEGMENT_SIZE * sizeof(char) ) );
        return TRUE;
      }
      else
      {
        printf("Malformed Transfer packet.\n");
      }
    }
    else
    {
      printf("Unable to verify that this is the piece we are looking for. :-[\n");
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

void* find_piece(char* fileName, int pieceNumber, char* hash)
{
  char doneFilePath[512];
  char segmentFilePath[512];
  char* dataBuffer;
  FILE* filePtr;

  sprintf(doneFilePath, "./done/%s", fileName);
  sprintf(segmentFilePath, "./temp/%s.piece_%i", fileName, pieceNumber);

  filePtr = fopen(doneFilePath, "r");
  if( filePtr )
  {
    // We have a finished copy of the file
    dataBuffer = malloc( (SEGMENT_SIZE * sizeof(char)) );
    memset(dataBuffer, '\0', SEGMENT_SIZE);

    // Go to the correct spot in the file
    unsigned long offset = ((unsigned long)pieceNumber * (unsigned long)SEGMENT_SIZE);
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
      dataBuffer = malloc( (SEGMENT_SIZE * sizeof(char)) );
      memset(dataBuffer, '\0', SEGMENT_SIZE);
      int bytes_read = fread(dataBuffer, sizeof(char), SEGMENT_SIZE, filePtr);
      fclose(filePtr);
      return dataBuffer;
    }
  }
  return NULL;
}

void process_client_request(char* responseBuffer, const char* requestBuffer)
{
  char myBuffer[512];
  char* curToken;
  char* savePtr;

  strcpy(myBuffer, requestBuffer);
  
  // Use strtok_r to make it thread safe
  curToken = strtok_r(myBuffer, "/", &savePtr);
  if(strstr(curToken, "CANHAZ") != NULL)
  {
    // Am I busy?
    // if yes -> respond with BUSY packet
    if(i_am_busy())
    {
      sprintf(responseBuffer, "BUSY");
    }
    else
    {
      char fileName[255];
      char hash[41];
      int pieceNumber;
      //  save the client info for reference
      curToken = strtok_r(NULL, "/", &savePtr);
      //  parse the filename
      strcpy(fileName, strtok_r(NULL, "/", &savePtr));
      //  parse the piecenumber
      pieceNumber = atoi(strtok_r(NULL, "/", &savePtr));
      //  parse the hash
      strcpy(hash, strtok_r(NULL, "/", &savePtr));
      //  Do I have this piece?
      //    If yes -> send the data
      //    If No -> send a 'no' packet
      char* dataBuffer = find_piece(fileName, pieceNumber, hash);
      if( dataBuffer != NULL )
      {
        int numCharsWritten = sprintf(responseBuffer,"HAZ/%s/START/",hash);
        responseBuffer += ((numCharsWritten) * sizeof(char));
        memcpy(responseBuffer, dataBuffer, ( SEGMENT_SIZE * sizeof(char) ) );
        free(dataBuffer);
      }
      else
      {
        sprintf(responseBuffer,"HAZNOT/%s/%i/%s/",fileName, pieceNumber, hash);
      }
    }
  }
  else
  {
    printf("Another client sent a request that I cannot process.\n");
  }

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
    else if( (strstr(argv[1], "start")) )
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
        // Start the download
      }
    }
  }
}
