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

// The size in bytes of 1 file Segment
#define SEGMENT_SIZE  256

int create_meta_file(char* filePath)
{
  char* fileName;
  char* fileName_no_ext;
  char* tempBufferA;
  char* tempBufferB;
  char metaFilePath[255];
  struct stat fileStats;
  unsigned long fileSize;
  unsigned long numSegments;
  FILE* filePtr;
  FILE* metaFP;

  //Get File Name
  tempBufferA = strdup(filePath);
  fileName = basename(tempBufferA);
  
  // Cut off any extension the file may have
  tempBufferB = strdup(fileName);
  fileName_no_ext = strtok(tempBufferB, ".");

  //File Size
  stat(filePath, &fileStats);
  fileSize = fileStats.st_size;

  //Calculate # of Segments
  numSegments = fileSize / SEGMENT_SIZE;
  if( (fileSize % SEGMENT_SIZE) > 0)
  {
    // Add an extra segment for the remainder
    numSegments ++;
  }
  
  //Create new meta file for writing
  //For simplicity just create the meta file in the current directory
  sprintf(metaFilePath, "./%s.torrent",fileName_no_ext);
  metaFP = fopen(metaFilePath, "w");
  
  //write the meta file
  fprintf(metaFP, "%s\n%lu\n%lu\n%i\n",fileName, fileSize, numSegments, SEGMENT_SIZE);

  //Open the file
  filePtr = fopen(filePath, "r");

  //Calculate Hash for each segment

  //clean up after myself
  free(tempBufferA);
  free(tempBufferB);
}

/**
* Main
* @param  argc  number of arguments
* @param  argv  array of arguments
* @return   boolean success or failure
*/
int main(int argc, char* argv[])
{
}
