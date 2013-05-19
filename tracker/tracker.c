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
#include <unistd.h>
#include "../lib/network_library.c"
#include "../lib/random.c"

#define true 1
#define false 0
#define SR_SEEDERS 1
#define SR_SUCCESS 0
#define STR_LEN 512
#define MAX_RETURNED_SEEDERS 5

typedef struct {
	int id; // An ID number for this record, useful for debugging
	pthread_t thread_id;	// Thread ID of this client
	struct sockaddr_storage addr; // Address information from accept() for this client
	socklen_t addr_len; // sizeof(addr)
	int done; // Indicates whether or not this client is still being processed
	int socket; // Stores the socket descriptor returned by accept()
	void* next; // Pointer to next node in the list (singly-linked)
	void* table; // Pointer to the master table
} client_struct;

typedef struct {
	char hash[STR_LEN];
	void* next;
	void* adjacent;
} hash_node;

typedef struct {
	char host[STR_LEN];
	char port[5];
	void* next;
} seeder_node;

// GLOBAL VARIABLES
hash_node master_table;
pthread_mutex_t master_lock = PTHREAD_MUTEX_INITIALIZER;

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

// Debug function for printing the hash list
void printht(hash_node* i)
{
	hash_node* h = i;
	seeder_node* s = NULL;
	
	while(h != NULL)
	{
		printf("[%s]", h->hash);
		s = h->adjacent;
		while(s != NULL)
		{
			printf("[%s:%s]", s->host, s->port);
			s = s->next;
		}
		printf("\n");
		h = h->next;
	}
}

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
	slot->table = &master_table;
	memset(&slot->addr, '\0', slot->addr_len);
}

/**
 add_seeder
 @param  me    pointer to this client_struct
 @param  host  hostname of seeder
 @param  port  port number of seeder
 @param  hash  hash to go with that client
*/
void add_seeder(client_struct* me, char* host, char* port, char* hash)
{
	// LOCK MASTER TABLE
	pthread_mutex_lock(&master_lock);
	
	// See if the hash already exists
	hash_node* c = (hash_node*)me->table;
	hash_node* last_c = NULL;
	while(c != NULL)
	{
		if(strcmp(c->hash, hash) == 0)
			break;
		else
		{
			last_c = c;
			c = c->next;
		}
	}
	
	// If it does not, add a new hash to the end of the list
	hash_node* target = NULL;
	if(c == NULL)
	{
		target = malloc(sizeof(hash_node));
		
		// This might be the first node in the list
		if(last_c == NULL)
			me->table = (void*)target;
		// Or it might be just a new one
		else
			last_c->next = target;
			
		target->next = NULL;
		target->adjacent = NULL;
		strcpy(target->hash, hash);
	}
	else
		target = c;
	
	// Check if the seeder already exists (if he does, end the function - we don't need to add somebody who's already here)
	seeder_node* d = target->adjacent;
	if(d != NULL)
		while(d->next != NULL)
		{
			if(strcmp(d->host, host) == 0 && strcmp(d->port, port))
				return;
			else
				d = d->next;
		}
		
	// Add the new guy
	seeder_node* new_seeder = malloc(sizeof(seeder_node));
	strcpy(new_seeder->host, host);
	strcpy(new_seeder->port, port);
	
	if(d == NULL)
		target->adjacent = new_seeder;
	else
		d->next = new_seeder;
	new_seeder->next = NULL;
	
	// UNLOCK MASTER TABLE
	pthread_mutex_unlock(&master_lock);
	
	printf("\t[%d]: Adding seeder: (%s:%s, %s)\n", me->id, host, port, hash);
}

/**
 remove_seeder
 @param  host  hostname of seeder
 @param  port  port number of seeder
*/
void remove_seeder(client_struct* me, char* host, char* port)
{	
	// LOCK MASTER TABLE
	pthread_mutex_lock(&master_lock);
	
	hash_node* h = me->table;
	seeder_node* s = NULL;
	seeder_node* last_s = NULL;
	seeder_node* temp = NULL;
	
	while(h != NULL)
	{
		s = h->adjacent;
		last_s = NULL;
		while(s != NULL)
		{
			if(strcmp(s->host, host) == 0 && strcmp(s->port, port) == 0)
			{
				// This is the first host in the list
				if(last_s == NULL)
				{
					h->adjacent = s->next;
					temp = s->next;
					free(s);
					s = temp;
				}
				// This is one of the other hosts
				else
				{
					last_s->next = s->next;
					temp = s->next;
					free(s);
					s = temp;
				}
			}
			else
			{
				last_s = s;
				s = s->next;
			}
		}
		h = h->next;
	}
	
	// UNLOCK MASTER TABLE
	pthread_mutex_unlock(&master_lock);
	
	printf("\t[%d]: Removing seeder: (%s:%s)\n", me->id, host, port);
}

/**
 send_response
 @param	 me  this client's client_struct
 @param  sr_mode  can be SR_SUCCESS (we just send success) or SR_SEEDERS (we send a list of 5 or less seeders)
 @param  hash  the hash/filename
*/
void send_response(client_struct* me, int sr_mode, char* hash)
{
	if(sr_mode == SR_SUCCESS)
	{
		char msg[8];
		strcpy(msg, "SUCCESS");
		write(me->socket, msg, sizeof(msg));
		printf("\t[%d]: 'SUCCESS' sent to client.\n", me->id);
	}
	else if(sr_mode == SR_SEEDERS)
	{
		char seeder_hosts[5][STR_LEN];
		char seeder_ports[5][6];
		int  seeder_count = 0;
		
		hash_node* h = me->table;
		seeder_node* s = NULL;
		while(h != NULL)
		{
			if(strcmp(h->hash, hash) == 0)
			{
				s = h->adjacent;
				
				// If any seeders exist
				if(s != NULL)
				{
					// Run through once to get the count
					while(s != NULL)
					{
						seeder_count++;
						s = s->next;
					}
					
					// If there are less than MAX_RETURNED_SEEDERS, send them all
					if(seeder_count < MAX_RETURNED_SEEDERS)
					{
						char msg[2048]; memset(msg, '\0', sizeof(msg));
						char seeder_count_str[64]; memset(seeder_count_str, '\0', sizeof(seeder_count_str));
						sprintf(seeder_count_str, "%d", seeder_count);
						strcpy(msg, "SEEDERS/");
						strcat(msg, hash);
						strcat(msg, "/");
						strcat(msg, seeder_count_str);
						
						s = h->adjacent;
						while(s != NULL)
						{
							strcat(msg, "/");
							strcat(msg, s->host);
							strcat(msg, ":");
							strcat(msg, s->port);
							s = s->next;
						}
						
						write(me->socket, msg, sizeof(msg));
					}
					// Otherwise, pick random ones and send them
					else
					{
						srand(time(NULL));
						int skips = rand() % (seeder_count - MAX_RETURNED_SEEDERS);
						char msg[2048]; memset(msg, '\0', sizeof(msg));
						
						s = h->adjacent;
						while(s != NULL)
						{
							if((rand() & 1) == 0 && skips > 0)
							{
								skips--;
								continue;
							}
							
							strcat(msg, "/");
							strcat(msg, s->host);
							strcat(msg, ":");
							strcat(msg, s->port);
							s = s->next;
						}
						write(me->socket, msg, sizeof(msg));
					}
				}
				// Otherwise...
				else
				{
					seeder_count = 0;
					char msg[8 + strlen(hash) + 2];
					strcpy(msg, "SEEDERS/");
					strcat(msg, hash);
					strcat(msg, "/0");
					write(me->socket, msg, sizeof(msg));
				}
				break;
			}
		}
		
		printf("\t[%d]: List of %d seeders sent to client.\n", me->id, seeder_count);
	}
	else
		printf("ERROR: invalid value for sr_mode\n");
}

/**
* handle_request
* @param  input_string  Unmodified string version of the request
*/
void handle_request(client_struct* me, char* input_string)
{
	char* word;
	char* host;
	char* port;
	char* hash;
	char* r[2];
	
	word = strtok_r(input_string, "/", &r[0]);
	
	if(strcmp(word, "STARTED") == 0)
	{
		word = strtok_r(NULL, "/", &r[0]);
		host = strtok_r(word, ":", &r[1]);
		port = strtok_r(NULL, ":", &r[1]);
		hash = strtok_r(NULL, "/", &r[0]);
		// Take the guy's hostname and hash and get him a list of people with the same hash
		add_seeder(me, host, port, hash);
		send_response(me, SR_SEEDERS, hash);
	}
	else if(strcmp(word, "STOPPED") == 0)
	{
		word = strtok_r(NULL, "/", &r[0]);
		host = strtok_r(word, ":", &r[1]);
		port = strtok_r(NULL, ":", &r[1]);
		// Remove the guy from the list of seeders
		remove_seeder(me, host, port);
	}
	else if(strcmp(word, "NEEDY") == 0)
	{
		word = strtok_r(NULL, "/", &r[0]);
		host = strtok_r(word, ":", &r[1]);
		port = strtok_r(NULL, ":", &r[1]);
		hash = strtok_r(NULL, "/", &r[0]);
		// Take the guy's hostname and hash and get him a list of seeders
		send_response(me, SR_SEEDERS, hash);
	}
	else if(strcmp(word, "INIT") == 0)
	{
		word = strtok_r(NULL, "/", &r[0]);
		host = strtok_r(word, ":", &r[1]);
		port = strtok_r(NULL, ":", &r[1]);
		hash = strtok_r(NULL, "/", &r[0]);
		// Take the guy's hostname and hash and add it to the table
		add_seeder(me, host, port, hash);
		send_response(me, SR_SUCCESS, NULL);
	}
	else
		printf("ERROR: Received misformed request\n");
}

/**
* process_request
* @param  cstruct_in  pointer to a client_struct
*/
void* process_request(void* cstruct_in)
{
	client_struct* me = (client_struct*)cstruct_in;
	printf("\t[%d]: New connection opened\n", me->id);
	
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
		chars_read = read(me->socket, read_buffer + read_offset, buffer_size - read_offset);
		read_offset += chars_read;
	} while(chars_read > 0);
	if(chars_read == -1)
		printf("\t[%d]: recv() error: %s\n", me->id, strerror(errno));	
	//str_trim(read_buffer);
	
	// Evaluate and handle the request
	handle_request(me, read_buffer);
	
	// Close connection
	printht(me->table);
	printf("\t[%d]: Connection closed.\n", me->id);
	close(me->socket);
	me->done = true;
	return 0;
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
		printf("[MAIN]: Launching %d\n", new_client->id);
		pthread_create(&new_client->thread_id, NULL, process_request, (void*)new_client);
	}
	return 0;
}
