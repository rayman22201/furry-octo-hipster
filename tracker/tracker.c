/**
* @File tracker.c
* CS 470 Final Project
* Patrick Anderson
* This is the tracker program, which coordinates downloads for clients.
*/
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include "../lib/hash_table.c"
#include "../lib/linked_list.c"
#include "../lib/network_library.c"
#include "../lib/random.c"

#define true 1
#define false 0

typedef struct {
	int id; // An ID number for this record, useful for debugging
	pthread_t thread_id;	// Thread ID of this client
	struct sockaddr_storage addr; // Address information from accept() for this client
	socklen_t addr_len; // sizeof(addr)
	int done; // Indicates whether or not this client is still being processed
	int socket; // Stores the socket descriptor returned by accept()
	void* next; // Pointer to next node in the list (singly-linked)
} client_struct;

/**
* new_slot
* @param  schedule  pointer to a client_struct, which should be the first in the linked list of clients
* @return           pointer to the newly created linked list node
*/
client_struct* new_slot(client_struct* schedule)
{
	client_struct* new_cstruct = malloc(sizeof(client_struct));
	client_struct* c = schedule;
	
	new_cstruct->next = NULL;
	new_cstruct->id = 0;
	
	if(c == NULL)
		return new_cstruct;
	
	while(c->next != NULL)
		c = (client_struct*)c->next;
	
	c->next = new_cstruct;
	new_cstruct->id = c->id + 1;
	return new_cstruct;
}

/**
* get_next_available_slot
* @param  schedule  pointer to a client_struct, which should be the first in the linked list of clients
* @return           pointer to the first available linked list node
*/
client_struct* get_next_available_slot(client_struct* schedule)
{
	client_struct* c = schedule;
	while(c != NULL)
	{
		if(c->done == true)
			return c;
		else
			c = (client_struct*)c->next;
	}
	return new_slot(schedule);
}

/**
* initialize_slot
* @param  slot  pointer to a client_struct, which should be the first in the linked list of clients
*/
void initialize_slot(client_struct* slot)
{
	slot->addr_len = sizeof(struct sockaddr_storage);
	slot->done = false;
	memset(&slot->addr, '\0', slot->addr_len);
}

/**
* process_request
* @param  data_in  pointer to a client_struct
*/
void* process_request(void* cstruct_in)
{
	client_struct* me = (client_struct*)cstruct_in;
	printf("[Thread %d]: New connection opened\n", me->id);
	
	// REQUEST PROCESSING CODE
	// We just make a buffer and read into it until we see a newline
	int chars_read = -1;
	int buffer_size = 256;
	int read_offset = 0;
	char* read_buffer = malloc(buffer_size);
	
	memset(read_buffer, '\0', buffer_size);
	
	// Get the request
	do
	{
		if((double)(buffer_size - read_offset) < (double)((double)buffer_size * 0.5))
		{
			read_buffer = realloc(read_buffer, buffer_size*2);
			buffer_size = buffer_size*2;
		}
		chars_read = recv(me->socket, read_buffer + read_offset, buffer_size - read_offset, 0);
		read_offset += chars_read;
	} while(chars_read > 0);
	if(chars_read == -1)
		printf("[Thread %d]: recv() error: %s\n", me->id, strerror(errno));	
	str_trim(read_buffer);
	printf("[Thread %d]: (DEBUG) Trimmed input:\n\"%s\"\n", me->id, read_buffer);
	
	// Evaluate and handle the request
	handle_request(read_buffer);
	
	// Close connection
	printf("[Thread %d]: Connection closed.\n", me->id);
	close(me->socket);
	me->done = true;
	return NULL;
}

/**
* handle_request
* @param  input_string  Unmodified string version of the request
*/
void handle_request(char* input_string)
{
	char* word, host, port, hash;
	char* r[2];
	
	word = strtok_r(input_string, "/", &r[0]);
	
	if(strcmp(word, "STARTED") == 0)
	{
		word = strtok_r(NULL, "/", &r[0]);
		host = strtok_r(word, ":", &r[1]);
		port = strtok_r(NULL, ":", &r[1]);
		hash = strtok_r(NULL, "/", &r[0]);
		add_seeder(host, port, hash);
		send_response(me->socket, -1);
	}
	else if(strcmp(word, "STOPPED") == 0)
	{
		word = strtok_r(NULL, "/", &r[0]);
		host = strtok_r(word, ":", &r[1]);
		port = strtok_r(NULL, ":", &r[1]);
		remove_seeder(host, port);
	}
	else if(strcmp(word, "NEEDY") == 0)
	{
		word = strtok_r(NULL, "/", &r[0]);
		host = strtok_r(word, ":", &r[1]);
		port = strtok_r(NULL, ":", &r[1]);
		hash = strtok_r(NULL, "/", &r[0]);
		send_response(me->socket, 5);
	}
	else if(strcmp(word, "INIT") == 0)
	{
		word = strtok_r(NULL, "/", &r[0]);
		host = strtok_r(word, ":", &r[1]);
		port = strtok_r(NULL, ":", &r[1]);
		hash = strtok_r(NULL, "/", &r[0]);
		add_seeder(host, port, hash);
		send_response(me->socket, 0);
	}
	else
		printf("ERROR: Received misformed request\n");
}

/**
 add_seeder
 @param  host  hostname of seeder
 @param  port  port number of seeder
 @param  hash  hash to go with that client
*/
void add_seeder(char* host, char* port, char* hash)
{

}

/**
 remove_seeder
 @param  host  hostname of seeder
 @param  port  port number of seeder
*/
void remove_seeder(char* host, char* port)
{

}

/**
 send_response
 @param	 socket  connection socket to send on
 @param  count   number of seeders to send, -1 = all, 0 = just "SUCCESS"
*/
void send_response(int socket, int count)
{
	
}

// Debug function for printing the linked list
void printll(client_struct* s)
{
	client_struct* c = s;
	while(c != NULL)
	{
		printf("[%p, %d, %d, %p]\n", c, c->id, c->done, c->next);
		c = (client_struct*)c->next;
	}
}

/**
* Main
* @param  argc	number of arguments
* @param  argv	array of arguments
* @return 	boolean success or failure
*/
int main(int argc, char* argv[])
{
	// Initialize variables as neccessary
	int		port = 0;		// Stores the port we're running on
	int		main_socket = 0;	// Stores the socket we're listening on
	int		return_status = 0;	// Used to check the return status of various statements
	client_struct* 	new_client = NULL;	// Stores a pointer to our new client in the linked list of clients
	client_struct*	schedule = NULL;	// Stores a pointer to the linked list of clients
	
	// Get the port name from command line arguments
	if(argc != 2)
	{
		printf("Usage: ./tracker <port>\n");
		return -1;
	}
	port = str_to_int(argv[1]);
	
	// Set up socket to listen on
	main_socket = tcp_listen(port);
	
	// Loop and accept connections, create threads to handle requests
	while(true)
	{
		new_client = get_next_available_slot(schedule);
		if(schedule == NULL)
			schedule = new_client;
		initialize_slot(new_client);
		return_status = accept(main_socket, (struct sockaddr *)&new_client->addr, &new_client->addr_len);
		if(return_status == -1)
		{
			printf("ERROR: Failed to accept new connection: %s", strerror(errno));
			continue;
		}
		new_client->socket = return_status;
		printf("[Main thread]: Launching %d\n", new_client->id);
		pthread_create(&new_client->thread_id, NULL, process_request, (void*)new_client);
	}
	return 0;
}
