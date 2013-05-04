/**
* @File tracker.c
* CS 470 Final Project
* Patrick Anderson
* This is the tracker program, which coordinates downloads for clients.
*/
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "../lib/hash_table.c"
#include "../lib/linked_list.c"
#include "../lib/network_library.c"

/**
* Main
* @param  argc	number of arguments
* @param  argv	array of arguments
* @return 	boolean success or failure
*/
int main(int argc, char* argv[])
{
	int fd = 0;
	int stat = 0;
	char buffer[1024];
		memset(buffer, '\0', sizeof(buffer));
	fd = tcp_listen(4000);

	while(1)
	{
		stat = recv(fd, buffer, 1024, 0);
		printf("%s", buffer);
		memset(buffer, '\0', 1024);
	}
	return 0;
}
