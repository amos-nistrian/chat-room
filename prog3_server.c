// server.c
/* prog2_server.c uses server.c - code for example server program that uses TCP */
#include <sys/types.h>
#include <sys/wait.h> // used for forking
#include <sys/socket.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <ctype.h>	// used for isdigit()
#include "trie.h"	// trie tree
#include <errno.h> // used for socket time-out
#include <regex.h>  // used for regex

/*------------------------------------------------------------------------
 * Program: server
 *
 * Purpose: allocate a socket and then repeatedly execute the following:
 * (1) listen for connectins from clients
 * (2) server shows the messages to appropriate audience
 *
 * Syntax: server [ port ]
 *
 * port - protocol port number to use
 *
 * Note: The port argument is optional. If no port is specified,
 * the server uses the default given by PROTOPORT.
 *
 *------------------------------------------------------------------------
 */

#define test2	fprintf(stderr, "\nHERE at line[%d]\n", __LINE__); // macro used for debugging
#define test	fprintf(stderr, "\nHERE at line[%d]\n", __LINE__); // macro used for debugging
#define QLEN 6 /* size of request queue */

bool expired;

int n; /* stores return value for send & recv */
int participants_socket, observers_socket; /* socket descriptors for the two different sockets on the server which the server allocated, one for participants and one for observers */
int sdp_Array[255]; /* socket descriptors of the paticipants, up to 255 */
int sdo_Array[255]; /* socket descriptors of the observers, up to 255 */
int num_participants, num_observers; /* holds the number of participants and observers connected at their respected ports */
//int index; /* keeps track of where you are in the sd_and_time array */
unsigned int addrlen; /* length of address */
char can_accept;
char too_full;
char taken;
char invalid_username;
char valid_username;
char usrname_buf[10]; /* buffer to store usernames of up to 10 characters, set to 11 to include the null terminator if username is 10 chars long */
char msg_buf[1000]; /* buffer to store messages up to 1000 characters long, set to 1002 for detecting words larger than 1000 characters */
fd_set readfds; /* set of socket descriptors for the participants and observers */
struct sockaddr_in sad_p; /* structure to hold server's socket address for participants */
struct sockaddr_in sad_o; /* structure to hold server's socket address for observers */
struct sockaddr_in cad; /* structure to hold any observers or participants socket address */

struct timeval send_time; /* time for calculating 60 second timeout on username send */
struct timeval receive_time; /* time for calculating 60 second timeout on username send */

struct sd_and_time  {
	int sd;
	struct timeval time_started;
};

struct participant_and_observer {
	int participants_sd;
	char * participants_username;
	int observers_sd;
};

struct sd_and_time * sd_and_timeArray[510]; /* array of pointers to sd and time structs */
struct participant_and_observer * participant_and_observerArray[255]; /* array of pointers to particpant and observers structs */

Trie *trie; /* struct for storing usernames */ 

/**********************************************************************************************************************************************/
/*Functions Functions Functions Functions Functions Functions Functions Functions Functions Functions Functions Functions Functions Functions*/

/* function finds out observers sd, this is only called when we know for a fact that the observer is associated w/ a participant and we provide the participants sd */
int get_observers_sd(int sd) {
	int observers_sd = -1;

	for (int i=0; i<255; i++) {
		if (participant_and_observerArray[i] != NULL) {
			if (participant_and_observerArray[i]->participants_sd == sd) {
				observers_sd = participant_and_observerArray[i]->observers_sd;
				break;
			}
		}
	}
	return observers_sd;
}

/* function to remove the participant/observer after 60 seconds of non submittal */
void remove_sd_and_time (int sd) {
	for(int i=0; i<510; i++) {
		if (sd_and_timeArray[i] != NULL) {
			if(sd_and_timeArray[i]->sd == sd) {
				free(sd_and_timeArray[i]); /* remove it */
				sd_and_timeArray[i] = NULL;
				break;
			}
		}
	}
}

/* try to remove the observer from the array if its present */
void remove_observer_from_participants_and_observersArray(int sd) {
	for(int i=0; i<255; i++) {
		if (participant_and_observerArray[i] != NULL) {
			if(participant_and_observerArray[i]->observers_sd == sd || participant_and_observerArray[i]->participants_sd == sd) {
				participant_and_observerArray[i]->observers_sd = -1;
				break;
			}
		}
	}
}

/* function removes the participant and his username by freeing up that index */
void remove_participant_from_participants_and_observersArray (int sd) {

	for(int i=0; i<255; i++) {
		if (participant_and_observerArray[i] != NULL) {
			if(participant_and_observerArray[i]->participants_sd == sd) {
				free(participant_and_observerArray[i]->participants_username);
				free(participant_and_observerArray[i]);
				participant_and_observerArray[i] = NULL;
				break;
			}
		}
	}
}

/* function removes client from their socket array */
void remove_client_from_socketArray(int socket) {
	for (int i=0; i<255; i++) {
		if(sdo_Array[i] == socket ) {
			sdo_Array[i] = 0;
			break;
		}
		if(sdp_Array[i] == socket ) {
			sdp_Array[i] = 0;
			break;
		}
	}
}

/* function checks if a username is set for a participant, and returns it */
char * get_username_out_of_struct(int sd) {
	char * username = NULL;
	for (int i=0; i<255; i++) {
		if (participant_and_observerArray[i] != NULL) {
			if (participant_and_observerArray[i]->participants_sd == sd) {

				if (participant_and_observerArray[i]->participants_username != NULL) {

					username = participant_and_observerArray[i]->participants_username;
				}
				break;
			}
		}
	}
	return username;
}

/* function removes all traces of an observers connection */
void destroy_observer(int socket) {
	for (int i= 0; i<255; i++) {
		if (socket == sdo_Array[i]) { /* an observer is disconnecting */
			remove_sd_and_time (socket);
			remove_observer_from_participants_and_observersArray(socket); /* remove the observer from the participants_and_observersArray */
			remove_client_from_socketArray(socket); /* remove the observer from their socket array */
			FD_CLR(socket, &readfds); /* remove the observer from the readfds */
			num_observers--; /* decrement the number of observers */
			close(socket);
			break;
		}
	}
}

/* function removes all traces of a participants connection */
void destroy_participant(int socket) {

	for (int i= 0; i<255; i++) {
		if (socket == sdp_Array[i]) { /* an observer is disconnecting */
			remove_sd_and_time (socket);

			char participants_username[11];
			memset(participants_username, 0, 11);
			strcpy(participants_username, get_username_out_of_struct(socket));
			trie_remove(trie, participants_username); /* remove the name from the trie */

			remove_participant_from_participants_and_observersArray(socket); /* remove the participant from the participants_and_observersArray */
			remove_client_from_socketArray(socket); /* remove the participant from their socket array */
			FD_CLR(socket, &readfds); /* remove the participant from the readfds */
			num_participants--; /* decrement the number of participants */
			close(socket);
			break;
		}
	}
}

/* function prints the the bytes we received used for debugging */
int recvf(int sd, void * data2, int size) {

    n = recv(sd, data2, size, MSG_WAITALL);
	if (n < 1 ) {
		for(int i=0; i<255; i++) {
			if (sdo_Array[i] == sd) {
				destroy_observer(sd);
				break;
			}
			if (sdp_Array[i] == sd) {
				destroy_observer(get_observers_sd(sd));
				destroy_participant(sd);
				break;
			}
		}
		return n;
	}
	else {
	    char * data = (char *) data2;

		for (int i = 0; i < size; i++) {
			if (data[i] > 57) {
			}
			else if (data[i] > 31 && data[i] < 48) {
			}
			else {
			}
	    }
	    return n;
	}
}

/* function prints the the bytes we send used for debugging */
int sendf(int sd, void * data2, int size, int flag) {

    n = send(sd, data2, size, flag);
    char * data = (char *) data2;

	for (int i = 0; i < size; i++) {
		if (data[i] > 57) {
		}
		else if (data[i] > 31 && data[i] < 48) {
		}
		else {
		}
    }
    return n;
}

/* function updates all observers that a particpant has left */
void broadcast_observer_has_joined() {

	char message[26] = "A new observer has joined";
	message[26] = '\0';
	uint16_t length = strlen(message);

	uint16_t nbo_length = htons(length);

	for(int i=0; i<255; i++) {
		if (participant_and_observerArray[i] != NULL) {
			if (participant_and_observerArray[i]->observers_sd != -1) {
				send(participant_and_observerArray[i]->observers_sd, &nbo_length, sizeof(nbo_length), MSG_NOSIGNAL);
				send(participant_and_observerArray[i]->observers_sd, &message, strlen(message), MSG_NOSIGNAL);
			}
		}
	}
}

/* function updates all observers that a particpant has left */
void broadcast_participant_has_left_or_joined(char * username, char * ending) {
	char * beginning = "User ";

	char * message = malloc(strlen(beginning) + strlen(username) + strlen(ending) +1);
	uint16_t length = strlen(beginning) + strlen(username) + strlen(ending);
	memset(message, 0, length);
	message[length] = '\0';

	strcat(message, beginning);
	strcat(message, username);
	strcat(message, ending);

	uint16_t nbo_length = htons(length);

	int message_length = strlen(message);

	for(int i=0; i<255; i++) {
		if (participant_and_observerArray[i] != NULL) {

			if (participant_and_observerArray[i]->observers_sd != -1) {
				send(participant_and_observerArray[i]->observers_sd, &nbo_length, sizeof(nbo_length), MSG_NOSIGNAL);
				send(participant_and_observerArray[i]->observers_sd, message, message_length, MSG_NOSIGNAL);
			}
		}
	}
	free(message);
}

/* function to broadcast messages to all observers */
void broadcast_public_message(char * message) {

	uint16_t nbo_length = htons(strlen(message));

	int message_length = strlen(message);

	for(int i=0; i<255; i++) {
		if (participant_and_observerArray[i] != NULL) {
			if (participant_and_observerArray[i]->observers_sd != -1) {
				send(participant_and_observerArray[i]->observers_sd, &nbo_length, sizeof(nbo_length), MSG_NOSIGNAL);
				send(participant_and_observerArray[i]->observers_sd, message, message_length, MSG_NOSIGNAL);
			}
		}
	}
}

/* try to remove the observer from the array if its present */
int check_if_observer_set_in_participants_and_observersArray(int sd) {

	//int observer_sd = -1;
	for (int i=0; i<255; i++) {
		if (participant_and_observerArray[i] != NULL) {
			if(participant_and_observerArray[i]->observers_sd == sd || participant_and_observerArray[i]->participants_sd == sd) {
				return participant_and_observerArray[i]->observers_sd;
			}
		}
	}
	return -1;
}

/* function to reset the time they have to send a valid username */
void update_time(int sd, struct timeval new_time_started) {

	for (int i = 0; i<510; i++) {
		if (sd_and_timeArray[i] != NULL) {
			if (sd_and_timeArray[i]->sd == sd) {
				sd_and_timeArray[i]->time_started = new_time_started;
				break;
			}
		}
	}
}

/* function to keep track of time for username submittal */
void insert_sd_and_time (int sd, struct timeval time_started) {
	for(int i = 0; i<510; i++) {
		if (sd_and_timeArray[i] == NULL) {
			struct sd_and_time * new_entry = (struct sd_and_time*)malloc(sizeof(struct sd_and_time));
			new_entry->sd = sd;
			new_entry->time_started = time_started;
			sd_and_timeArray[i] = new_entry; /* insert into the array */
			break;
		}
	}
}

/* function to insert participant into participant_and_observerArray  */
void insert_participant_into_participant_and_observerArray(int sd) {
	for(int i = 0; i<255; i++) {
		if (participant_and_observerArray[i] == NULL) {
			struct participant_and_observer * new_entry = (struct participant_and_observer*)malloc(sizeof(struct participant_and_observer));
			new_entry->participants_sd = sd; /* set the participants sd */
			new_entry->observers_sd = -1;
			new_entry->participants_username = NULL;
			participant_and_observerArray[i] = new_entry; /* insert the new entry into the array */

			break;
		}
	}
}

/* function checks if the username is only alphanumeric with underscores, and 10 characters or less */
bool pass_regex_test( char * username) {

	bool passed_regex = false;
	regex_t re;
	int returns;
	char pattern [] = "^[a-zA-Z0-9_]{1,10}$";

	if (regcomp(&re, pattern, REG_EXTENDED) != 0) {
		passed_regex = false;
		return passed_regex;
	}

	returns = regexec(&re, username, (size_t) 0, NULL, 0);
	regfree(&re);

	if (returns == 0) {
		passed_regex = true;
		return passed_regex;
	}
	return passed_regex;
}

/* function finds the observer to send the private message to */
int get_observer(char * username) {
	for(int i = 0; i<255; i++) {
		if (participant_and_observerArray[i] != NULL) {
			if(strcmp(participant_and_observerArray[i]->participants_username, username) == 0) {
				int observer = participant_and_observerArray[i]->observers_sd;
				return observer;
			}
		}
	}
	return -1;
}

/* function checks if a username is in the participant_and_observerArray used for when observer wants to listen on a user */
bool is_username_in_particpants_and_observersArray(char * username) {
	for(int i = 0; i<255; i++) {
		if (participant_and_observerArray[i] != NULL) {
			if(strcmp(participant_and_observerArray[i]->participants_username, username) == 0) {
				return true;
			}
		}
	}
	return false;
}

/* function sends warning to a user if the username they intend on sending a private message to doesn't exist */
void send_warning(int participants_socket, char * recipients_username) {
	char beginning[] = "Warning: user ";
    char ending[] = " doesn't exist...";

	char * warning = malloc(strlen(beginning) + strlen(recipients_username) + strlen(ending) +1);
	int len = strlen(beginning) + strlen(recipients_username) + strlen(ending);
	memset(warning, 0, len);
	warning[len] = '\0';

    strcat(warning, beginning);
    strcat(warning, recipients_username);
    strcat(warning, ending);

	uint16_t nbo_length = strlen(warning);
	nbo_length = htons(nbo_length);

	int observers_socket = get_observers_sd(participants_socket);

	n = send(observers_socket, &nbo_length, sizeof(nbo_length), MSG_NOSIGNAL);
    n = send(observers_socket, warning, strlen(warning), MSG_NOSIGNAL);

	free(warning);
}

/* function sees if a username request is valid for a particpant */
int can_assign_participant_a_username(char * username) {

	int can_assign_usrname_to_participant;
	bool passed_regex = pass_regex_test(username);

	if (passed_regex == false) { 	/* check if the username breaks the regex rules */
		can_assign_usrname_to_participant = -1;
		return can_assign_usrname_to_participant;
	}

 	else { /* check that the username isnt in the trie */
		if (trie_lookup(trie, username) == TRIE_NULL) {
  			can_assign_usrname_to_participant = 1;
 			return can_assign_usrname_to_participant;
		}
 		else { /* the username is already in the trie */
 			can_assign_usrname_to_participant = 0;
 			return can_assign_usrname_to_participant;
 		}
	}
}

/* fucntion to insert a username into participant_and_observerArray */
void insert_username_into_participant_and_observerArray(int sd, char * username, int username_length) {

	for(int i = 0; i<255; i++) {
		if (participant_and_observerArray[i] != NULL) {
			if (participant_and_observerArray[i]->participants_sd == sd) {
				participant_and_observerArray[i]->participants_username = malloc(username_length);
				strcpy(participant_and_observerArray[i]->participants_username, username);
				break;
			}
		}
	}
}

/* fucntion to insert a username into participant_and_observerArray sd is observers_sd we want to associate with the participant */
int assign_client_to_participant(int sd, char * username) {

	int can_insert_client = 0;
	for(int i = 0; i<255; i++) {
	    if (participant_and_observerArray[i] != NULL) {
	        if ((strcmp(participant_and_observerArray[i]->participants_username, username) == 0)) {
	            if (participant_and_observerArray[i]->observers_sd == -1) {
					participant_and_observerArray[i]->observers_sd = sd;
	                return can_insert_client = 1;
	            }
	            else {
	                can_insert_client = 2;
	                return can_insert_client = 2;
	            }
	        }
	    }
	}
	return can_insert_client;
}

/* function to check the expiration */
bool check_elapsed_time(int sd, struct timeval time_received) {

	bool expired;
	for (int i=0; i<510; i++) {
		if (sd_and_timeArray[i] != NULL) {
			if (sd_and_timeArray[i]->sd == sd) {
				int elapsed_time = sd_and_timeArray[i]->time_started.tv_sec - time_received.tv_sec;
				if (abs(elapsed_time) > 60) {
					expired = true;
				}
				else {
					expired = false;
				}
				break;
			}
		}
	}
	return expired;
}

/* function returns the length of a username a client sent over */
uint8_t receive_username_length_from_client() {
	uint8_t length = msg_buf[0];
	return length;
}

/* function returns the username that a client sent over */
char * receive_username_from_client( uint8_t username_length ) {

	char * username;
	username = malloc((username_length) +1);

	for (int i=0; i<username_length; i++) {
		username[i] = msg_buf[i+1];
	}
	username[username_length] = '\0'; /* Null terminate */
	return username;
}

/* function removes any pending participants connections taking longer than 60 seconds to negotiate a username */
void purge_losers(struct timeval recivedtime) {

	for (int i=0; i<510; i++) {
		if (sd_and_timeArray[i] != NULL) {
			int elapsed_time = sd_and_timeArray[i]->time_started.tv_usec - recivedtime.tv_usec;
			if (abs(elapsed_time) > 60) {
				sd_and_timeArray[i] = NULL;
			}
		}
	}
}

/* function used to negotiate the username an observer wants to listen to */
void negotiate_observer_username (int observers_socket) {

    bool suddn_disconnect;

    gettimeofday(&receive_time, NULL);
    bool expired = check_elapsed_time(observers_socket, receive_time);

    if (expired == true) {
        destroy_observer(observers_socket);
    }

    else {
		if (expired == false) {
	        uint8_t username_length;
	        n = recv(observers_socket, &username_length, sizeof(username_length), MSG_WAITALL);
	        if (n < 0) {
	            suddn_disconnect = true;
	            destroy_observer(observers_socket);
	        }
	        if (suddn_disconnect == false) {
	            char username[username_length +1];
				memset(username, 0, username_length);
	            username[username_length] = '\0';
	            n = recv(observers_socket, &username, username_length, MSG_WAITALL);

	            if (n < 0) {
	                suddn_disconnect = true;
	                destroy_observer(observers_socket);
	            }
	            if (suddn_disconnect == false) {

	                int can_assign_client = assign_client_to_participant(observers_socket, username);

					if (can_assign_client == 1) {
	                    n = send(observers_socket, &can_accept, sizeof(valid_username), MSG_NOSIGNAL);
	                    if (n<0) {
	                        destroy_observer(observers_socket);
	                    }
	                    else {
	                        broadcast_observer_has_joined();
	                    }
	                }
	                else if (can_assign_client == 2) {
	                    n = send(observers_socket, &taken, sizeof(taken), MSG_NOSIGNAL);
	                    if (n<0) {
	                        destroy_observer(observers_socket);
	                    }
	                    destroy_observer(observers_socket);
	                }
	                else {
						char no_active_participant = 'N';
	                    n = send(observers_socket, &no_active_participant, sizeof(no_active_participant), MSG_NOSIGNAL);
	                    if (n<0) {
	                        destroy_observer(observers_socket);
	                    }
	                    destroy_observer(observers_socket);
	                }
	            }
	        }
	  }
    }
}

/* function used to negotiate a participants_username */
void negotiate_participant_username(int participants_socket) {

    gettimeofday(&receive_time, NULL);
    bool expired = check_elapsed_time(participants_socket, receive_time);

    if (expired == true) {

        destroy_participant(participants_socket);
    }

    else {
		if (expired == false) {

	        bool suddn_disconnect;
	        uint8_t username_length;

	        n = recv(participants_socket, &username_length, sizeof(username_length), MSG_WAITALL);
	        if (n < 0) {
	            suddn_disconnect = true;
	            destroy_participant(participants_socket);
	        }
	        if (suddn_disconnect == false) {
	            char username[username_length +1];
				memset(username, 0, username_length +1);
	            username[username_length] = '\0';
	            n = recv(participants_socket, &username, username_length, MSG_WAITALL);
	            if (n < 0) {
	                suddn_disconnect = true;
	                destroy_participant(participants_socket);
	            }
	            if (suddn_disconnect == false) {

	                int can_assign_usrname_to_participant = can_assign_participant_a_username(username);

		            if (can_assign_usrname_to_participant == -1) {
		                n = send(participants_socket, &invalid_username, sizeof(invalid_username), MSG_NOSIGNAL); /* send I*/
	                    if (n < 0) {
	                        destroy_participant(participants_socket);
	                    }
		            }
		            else if (can_assign_usrname_to_participant == 1) {
		                n = send(participants_socket, &valid_username, sizeof(valid_username), MSG_NOSIGNAL); /* send y */
	                    if (n < 0) {
	                        suddn_disconnect = true;
	                        destroy_participant(participants_socket);
	                    }
		                if (suddn_disconnect == false) {
		                    char ending [] = " has joined";
		                    broadcast_participant_has_left_or_joined(username, ending);
		                    trie_insert(trie, username, "1");/* isert the username into trie */
		                    insert_username_into_participant_and_observerArray(participants_socket, username, username_length); /* insert the username in the participants and observers structARAray*/
		                    remove_sd_and_time(participants_socket); /* remove them from sd_and_timeArray */
		                }
		            }
		            else {
		                n = send(participants_socket, &taken, sizeof(taken), MSG_NOSIGNAL); /* tell the participant the username is taken */
	                    if (n<0) {
	                        suddn_disconnect = true;
	                        destroy_participant(participants_socket);
	                    }
						if (suddn_disconnect == false) {
		                    gettimeofday(&receive_time, NULL); /* get the time of the day */
		                    update_time(participants_socket, receive_time); /* update the time */
						}
	                }
	            }
	        }
	    }
	}
}

/* function prepends the messages before they are sent out */
char * prepare_message(char * message, char * senders_username, char beginning[]) {

    char space[] = " ";
    char ending[] = ": ";
    int num_spaces = 10 - strlen(senders_username);

    char * prepended_public_message = malloc(strlen(beginning) + num_spaces + strlen(senders_username) + strlen(ending) + strlen(message) +1);
    int len = strlen(beginning) + num_spaces + strlen(senders_username) + strlen(ending) + strlen(message);
    memset(prepended_public_message, 0, len);
    prepended_public_message[len] = '\0';

    strcat(prepended_public_message, beginning);
    for (int i=0; i < num_spaces; i++) { /* prepend the spaces etc */
        strcat(prepended_public_message, space);
    }
    strcat(prepended_public_message, senders_username);
    strcat(prepended_public_message, ending);
    strcat(prepended_public_message, message);

    return prepended_public_message;
}

/* function delivers private messages or warnings */
void private_message(char * message, int senders_socket) {

	char destination_username_buffer[11];
	memset(destination_username_buffer, 0, 11);
	sscanf(message, "%10s ", destination_username_buffer); /* scan up to the first space into destination username, this will max out at 10 */

	char destination_username[strlen(destination_username_buffer)];
	int max = strlen(destination_username_buffer);
	for (int i=0; i<max; i++ ) {
		destination_username[i] = destination_username_buffer[i+1];
	}
	destination_username[max] = '\0';

    bool username_found = is_username_in_particpants_and_observersArray(destination_username);
    if (username_found == true) {

		char senders_username[11];
		memset(senders_username, 0, 11);
		strcpy(senders_username, get_username_out_of_struct(senders_socket)); /* get the username of the sender */
        int observers_socket = get_observer(destination_username);
        if (observers_socket != -1) {
            char beginning[] = "*";
			char * message_ptr = prepare_message(message, senders_username, beginning);// this returns a char *
			//send_private_message(*message_ptr, observers_socket); // can you do this? *message_ptr shit
			uint16_t nbo_length = htons(strlen(message_ptr));

			n = send(observers_socket, &nbo_length, sizeof(nbo_length), MSG_NOSIGNAL);
			n = send(observers_socket, message_ptr, strlen(message_ptr), MSG_NOSIGNAL);

			free(message_ptr);
        }
        else {
            send_warning(senders_socket, destination_username);
        }
    }
    else {
        send_warning(senders_socket, destination_username);
    }
}

/* function finds out if the message is valid length and whether it is public or private */
void process_message(int participants_socket) { /* its a public or private message */
	uint16_t nbo_message_length;
	bool suddn_disconnect = false;

	char username[11];
	memset(username, 0, 11);
	strcpy(username, get_username_out_of_struct(participants_socket));
	char ending [] = " has left";

	n = recv(participants_socket, &nbo_message_length, sizeof(nbo_message_length), MSG_WAITALL);
	nbo_message_length = ntohs(nbo_message_length);

	if (n<1){

		suddn_disconnect = true;

		broadcast_participant_has_left_or_joined(username, ending);


		int observer_sd = check_if_observer_set_in_participants_and_observersArray(participants_socket);
		if (observer_sd != -1) {

			destroy_observer(observer_sd);
			destroy_participant(participants_socket);
		}
		else {
			destroy_participant(participants_socket);
		}
	}
	if (suddn_disconnect == false) {
		if (nbo_message_length > 1000) {

	        /* remove username from trie tree if its there */
	        char username[11];
	        memset(username, 0, 11);
	        strcpy(username, get_username_out_of_struct(participants_socket));
	        if (username[0] != '0') {
	            trie_remove(trie, username); /* remove it from the trie */
	            char ending [] = " has left\n";

	            broadcast_participant_has_left_or_joined(username, ending);

	        }
	        destroy_participant(participants_socket);
	    }
		else { /* message is the correct length */
			char message[nbo_message_length +1];
			memset(message, 0, nbo_message_length);
			message[nbo_message_length] = '\0';
			n = recv(participants_socket, &message, nbo_message_length, MSG_WAITALL); /* get the message */
			if (n<0){
				suddn_disconnect = true;
				int observer_sd = check_if_observer_set_in_participants_and_observersArray(participants_socket);
				if (observer_sd != -1) {
					destroy_observer(observer_sd);
					destroy_participant(participants_socket);
				}
				else {
					destroy_participant(participants_socket);
				}
			}
			if (suddn_disconnect == false) {
				char senders_username[11];
				memset(senders_username, 0, sizeof(senders_username));
				strcpy(senders_username, get_username_out_of_struct(participants_socket)); /* get the username of the sender */

				if (message[0] != '@') {
					char beginning[] = ">";
					char * message_ptr = prepare_message(message, senders_username, beginning);

					broadcast_public_message(message_ptr);

					free(message_ptr);
				}
				else {

					if (message[0] == '@' && strlen(message) > 1 && message[1] != ' ' && message[1] != '\0') {
						private_message(message, participants_socket);
					}
					else {
						char beginning[] = ">";
						char * message_ptr = prepare_message(message, senders_username, beginning);
						broadcast_public_message(message_ptr);
						free(message_ptr);
					}
				}
			}
		}
	}
}

/* function detects a signal came from the participant connection socket or observer connection socket trying to connect */
void signal_received_check_sockets(int socket, int * num_connected, int socketArray[], int id) {

	/* check that there is room for this new participant or observer connection */
	int i = 0;
	if (*num_connected < 255) {

		/* insert the connection into an open index in the array */
	    while (socketArray[i] > 0 && i < 256) {
	        i++;
	    }
		if (i < 256) {
			/* there must be room for a connection, try to make the connection */
			/* put them in the socket array */
            if ( (socketArray[i] = accept(socket, (struct sockaddr *)&cad, &addrlen)) < 0) {
                fprintf(stderr, "Error: Accept failed 1\n");
                exit(EXIT_FAILURE);
            }
            else {
				/* put them in the readfds */
                FD_SET(socketArray[i], &readfds);
				if (socket == participants_socket) { /* add participant to participants and observers array on connection */

					insert_participant_into_participant_and_observerArray(socketArray[i]); //this function works
				}
                (*num_connected)++; /* increment the number of connected hosts */
                n = send(socketArray[i], &can_accept, sizeof(can_accept), MSG_NOSIGNAL); /* send them yes */
				bool suddn_disconnect = false;
				if (n<0) {
					for (int j=0; j<255; j++) {
						if (socketArray[i] == sdp_Array[j]) {
							destroy_participant(socketArray[i]);
							break;
						}
						if (socketArray[i] == sdo_Array[j]) {
							destroy_observer(socketArray[i]);
							break;
						}
					}
					suddn_disconnect = true;
				}

				if (suddn_disconnect == false) { /* start the timer */

					gettimeofday(&send_time, NULL); /* get the current time */
					insert_sd_and_time(socketArray[i], send_time); /* set them in the stuct sd_and_time */
				}
			}
		}
	}
	else { /* participant/observers connections full */
	    if ( (n = accept(socket, (struct sockaddr *)&cad, &addrlen)) < 0) {
	        fprintf(stderr, "Error: Accept failed 1\n");
	        exit(EXIT_FAILURE);
	    }
	    else { /*send them a character N to signify that there are too many participants connected*/
	        fprintf(stderr, "Too many participants connected\n" );

	        send(n, &too_full, sizeof(too_full), MSG_NOSIGNAL);
	        close(n);
	    }
	}
}

/* End Functions End Functions End Functions End Functions End Functions End Functions End Functions End Functions End Functions End Functions */
/**********************************************************************************************************************************************/
int main(int argc, char **argv) {
	struct protoent *ptrp; /* pointer to a protocol table entry */
	uint16_t port_participants; /* protocol port number for participants */
    uint16_t port_observers; /* protocol port number for observers*/
	int optval = 1; /* boolean value when we set socket option */
    can_accept = 'Y';
    too_full = 'N';
    taken = 'T';
    invalid_username = 'I';
	valid_username = 'Y';

	/* Process command line args */
	if( argc != 3 ) {
		fprintf(stderr,"Error: Wrong number of arguments\n");
		fprintf(stderr,"usage:\n");
		fprintf(stderr,"./server server_port\n");
		exit(EXIT_FAILURE);
	}

	memset((char *)&sad_p,0,sizeof(sad_p)); /* clear out sockaddr structure of connected participants */
	sad_p.sin_family = AF_INET; /* set family to Internet */
	sad_p.sin_addr.s_addr = INADDR_ANY; /* set the local IP address */

	port_participants = atoi(argv[1]); /* convert argument to binary */
	if (port_participants > 0) { /* test for illegal value */
		sad_p.sin_port = htons((u_short)port_participants);
	} else { /* print error message and exit */
		fprintf(stderr,"Error: Bad port number %s\n",argv[1]);
		exit(EXIT_FAILURE);
	}

    memset((char *)&sad_o,0,sizeof(sad_o)); /* clear out sockaddr structure of connected observers */
	sad_o.sin_family = AF_INET; /* set family to Internet */
	sad_o.sin_addr.s_addr = INADDR_ANY; /* set the local IP address */

    port_observers = atoi(argv[2]); /* convert argument to binary */
    if (port_observers > 0) { /* test for illegal value */
        sad_o.sin_port = htons((u_short)port_observers);
    } else { /* print error message and exit */
        fprintf(stderr,"Error: Bad port number %s\n",argv[1]);
        exit(EXIT_FAILURE);
    }

	/* Map TCP transport protocol name to protocol number */
	if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
		fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
		exit(EXIT_FAILURE);
	}

	/* Create a socket for participants */
	participants_socket = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
	if (participants_socket < 0) {
		fprintf(stderr, "Error: Socket creation failed\n");
		exit(EXIT_FAILURE);
	}

    /* Create a socket for observers */
	observers_socket = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
	if (observers_socket < 0) {
		fprintf(stderr, "Error: Socket creation failed\n");
		exit(EXIT_FAILURE);
	}

	/* Allow reuse of port - avoid "Bind failed" issues */
	//set participants_socket to allow multiple connections
	if( setsockopt(participants_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0 ) {
		fprintf(stderr, "Error Setting socket option failed\n");
		exit(EXIT_FAILURE);
	}

    /* Allow reuse of port - avoid "Bind failed" issues */
	//set observers_socket socket to allow multiple connections
    if( setsockopt(observers_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0 ) {
        fprintf(stderr, "Error Setting socket option failed\n");
        exit(EXIT_FAILURE);
    }

	/* Bind participants_socket to localhost port argv[1] */
	if (bind(participants_socket, (struct sockaddr *)&sad_p, sizeof(sad_p)) < 0) {
		fprintf(stderr,"Error: Bind failed\n");
		exit(EXIT_FAILURE);
	}

    /* Bind observers_socket to localhost port argv[2] */
    if (bind(observers_socket, (struct sockaddr *)&sad_o, sizeof(sad_o)) < 0) {
        fprintf(stderr,"Error: Bind failed\n");
        exit(EXIT_FAILURE);
    }

	/* Specify size of 6 pending connections to the participants socket queue */
	if (listen(participants_socket, QLEN) < 0) {
		fprintf(stderr,"Error: Listen failed\n");
		exit(EXIT_FAILURE);
	}

    /* Specify size of 6 pending connections to the observers socket queue */
    if (listen(observers_socket, QLEN) < 0) {
        fprintf(stderr,"Error: Listen failed\n");
        exit(EXIT_FAILURE);
    }

    /* clear out the socket descriptors buffers */
    memset(sdp_Array, 0, 255);
    memset(sdo_Array, 0, 255);

	trie = trie_new(); /* create a trie for usernames */

	FD_ZERO(&readfds);	/* clear the socket set for participants and observers */

	/* infinite loop - accept and handle requests */
	while (1) {

		addrlen = sizeof(cad);

		FD_ZERO(&readfds); /* clear out the readfds bits*/

		memset(msg_buf, 0, 1000); /* clear out the message buffer */

		FD_SET(participants_socket, &readfds); /* add the master socket for listening to new participant connections */

		FD_SET(observers_socket, &readfds); /* add the master socket for listening to new observer connections */

		/* periodically purge participants taking too long */
		if (num_participants > 127  || num_observers > 127 ) {
			gettimeofday(&receive_time, NULL);
			purge_losers(receive_time);
		}

		/* add the participants filedescriptors  to the fd_set */
		/* add the observers filedescriptors to the fd_set*/
		for (int i=0; i<255; i++) {
			if (sdp_Array[i] > 0) {
				FD_SET(sdp_Array[i], &readfds);
			}
			if (sdo_Array[i] > 0) {
				FD_SET(sdo_Array[i], &readfds);
			}
		}

		/* wait for an activity at one of the sockets, timeout is NULL so wait indefinitely */
		select(FD_SETSIZE, &readfds, NULL, NULL, NULL);

		/* check if a signal came from the participant/observer socket. If so, a participant/observer is trying to connect */
		if (FD_ISSET(participants_socket, &readfds)) {
			signal_received_check_sockets(participants_socket, &num_participants, sdp_Array, 1);
		}
		else if (FD_ISSET(observers_socket, &readfds)) {
			signal_received_check_sockets(observers_socket, &num_observers, sdo_Array, -1);
		}
		else { /* its a message or disconnection from a connected host */
			for (int i=0; i<255; i++) {
				/* find the observer in the array with FD_ISSET */
				if (FD_ISSET(sdo_Array[i], &readfds)) {
					negotiate_observer_username(sdo_Array[i]);
					break;
				}
				else {
					/* find the participant in the array with FD_ISSET */
					if (FD_ISSET(sdp_Array[i], &readfds)) {

						char * username_ptr = get_username_out_of_struct(sdp_Array[i]);

						if (username_ptr == NULL) {
							negotiate_participant_username(sdp_Array[i]);
						}
						else {
							process_message(sdp_Array[i]);
						}
						break;
					}
				}
			}
		}
	} // End while 1
	trie_free(trie); // Clear out trie tree
} // End of main
