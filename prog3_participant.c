//participant.c
/* client.c - code for example client program that uses TCP */
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h> // used for socket time-out
#include <regex.h>  // used for regex


/*------------------------------------------------------------------------
 * Program: client
 *
 * Purpose: allocate a socket, connect to a server, and print all output
 *
 * Syntax: client [ host [port] ]
 *
 * host - name of a computer on which server is executing
 * port - protocol port number server is using
 *
 * Note: Both arguments are optional. If no host name is specified,
 * the client uses "localhost"; if no protocol port is
 * specified, the client uses the default given by PROTOPORT.
 *
 *------------------------------------------------------------------------
 */
#define test    printf( "HERE at line[%d]\n", __LINE__); // macro used for debugging only"
int n; /* stores return value for send & recv */
int sd; /* socket descriptor */

/**********************************************************************************************************************************************/
/*Functions Functions Functions Functions Functions Functions Functions Functions Functions Functions Functions Functions Functions Functions*/

/* fucntion checks if a send or receive return -1 which is an error, you should close */
void check_valid_send_and_recv(int n) {
	if (n < 1) {
		printf("server disconnected\n");
		close(sd);
		exit(EXIT_FAILURE);
	}
}

/* function prints the the bytes we received used for debugging */
void recvf(int sd, void * data2, int size) {

    n = recv(sd, data2, size, MSG_WAITALL);
    check_valid_send_and_recv(n);
    char * data = (char *) data2;

    printf("\n      RECEIVED: ");
	for (int i = 0; i < size; i++) {
		if (data[i] > 57) {
			printf("[%c]", data[i]);
		}
		else if (data[i] > 31 && data[i] < 48) {
			printf("[%c]", data[i]);
		}
		else {
			printf("[%d]", data[i]);
		}
    }
    printf("\n\n" );

}

/* function prints the the bytes we send used for debugging */
void sendf(int sd, void * data2, int size) {

    n = send(sd, data2, size, MSG_NOSIGNAL);
    check_valid_send_and_recv(n);
    char * data = (char *) data2;

    printf("\n      PARTICIPANT SENT: ");
	for (int i = 0; i < size; i++) {
		if (data[i] > 57) {
			printf("[%c]", data[i]);
		}
		else if (data[i] > 31 && data[i] < 48) {
			printf("[%c]", data[i]);
		}
		else {
			printf("[%d]", data[i]);
		}
    }
    printf("\n\n" );
}

/* function checks if the username is only alphanumeric with underscores, and 10 characters or less */
bool pass_regex_test( char *username, char *pattern) {

	regex_t re;
	int status;

	if (regcomp(&re, pattern, REG_EXTENDED|REG_NOSUB) != 0) {
		return false;
	}

	status = regexec(&re, username, (size_t) 0, NULL, 0);
	regfree(&re);


	if (status != 0) {
		return false;
	}
	return true;
}

/*Functions Functions Functions Functions Functions Functions Functions Functions Functions Functions Functions Functions Functions Functions*/
/**********************************************************************************************************************************************/

int main( int argc, char **argv) {
	struct hostent *ptrh; /* pointer to a host table entry */
	struct protoent *ptrp; /* pointer to a protocol table entry */
	int port; /* protocol port number */
	char *host; /* pointer to host name */
	struct sockaddr_in sad; /* structure to hold servers address */
	memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */
	sad.sin_family = AF_INET; /* set family to Internet */
	char pattern[] = "^[a-zA-Z0-9_]{1,10}$";

	if( argc != 3 ) {
		fprintf(stderr,"Error: Wrong number of arguments\n");
		fprintf(stderr,"usage:\n");
		fprintf(stderr,"./client server_address server_port\n");
		exit(EXIT_FAILURE);
	}

	port = atoi(argv[2]); /* convert to binary */
	if (port > 0) /* test for legal value */
		sad.sin_port = htons((u_short)port); // port number in network byte order
	else {
		fprintf(stderr,"Error: bad port number %s\n",argv[2]);
		exit(EXIT_FAILURE);
	}

	host = argv[1]; /* if host argument specified */

	/* Convert host name to equivalent IP address and copy to sad. */
	ptrh = gethostbyname(host);
	if ( ptrh == NULL ) {
		fprintf(stderr,"Error: Invalid host: %s\n", host);
		exit(EXIT_FAILURE);
	}

	memcpy(&sad.sin_addr, ptrh->h_addr, ptrh->h_length);

	/* Map TCP transport protocol name to protocol number. */
	if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
		fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
		exit(EXIT_FAILURE);
	}

	/* Create a socket. */
	sd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
	if (sd < 0) {
		fprintf(stderr, "Error: Socket creation failed\n");
		exit(EXIT_FAILURE);
	}

	/* Connect the socket to the specified server. */
	if (connect(sd, (struct sockaddr *)&sad, sizeof(sad)) < 0) {
		fprintf(stderr,"connect failed\n");
		exit(EXIT_FAILURE);
	}
	/* receive if you can join */
	char can_join;
	n = recv(sd, &can_join, sizeof(can_join), MSG_WAITALL);
	check_valid_send_and_recv(n);

	if(can_join == 'Y') {
		uint8_t username_length;
		bool valid_length_and_regex = false;
		bool valid = false;
		char * username2;

		while (valid == false) {
			char username_buf[256];
			memset(username_buf, 0, 256);
			username_buf[255] = '\0';


			while (valid_length_and_regex == false) {
				printf("Enter a username: ");
				memset(username_buf, 0, 256);
				fgets(username_buf, sizeof(username_buf), stdin);

				username_length = strlen(username_buf)-1; /* minus 1 to remove the newline char */
				if (username_length > 10) {
					printf("username must be 10 or fewer characters\n");
				}
				else {
					char username[username_length +1];
					username[username_length] = '\0'; /* Null terminate */

					for (int i=0; i<username_length; i++) {
						username[i] = username_buf[i];
					}

					bool passed = pass_regex_test(username, pattern);

					if (passed == true) {
						valid_length_and_regex = true;
						username2 = malloc(username_length);
						strcpy(username2, username);
					}
					else {
						printf("username must consist of only lower/uppercase letters, numbers, and underscores\n");
						//free(username2);
					}
				}
			}

			char username[username_length +1];
			strcpy(username, username2);
			username[username_length] = '\0'; /* null terminate */
			free(username2);

			/* send the length and username */
			n = send(sd, &username_length, sizeof(username_length), MSG_NOSIGNAL);
			check_valid_send_and_recv(n);

			n = send(sd, &username, strlen(username), MSG_NOSIGNAL);
			check_valid_send_and_recv(n);

			/* receive if it is a good username */
			char valid_username;
			n = recv(sd, &valid_username, sizeof(valid_username), MSG_WAITALL);
			check_valid_send_and_recv(n);

			if( valid_username == 'T' || valid_username == 'I') {
				valid_length_and_regex = false;
			}
			else {
				valid = true;
			}
		}

		while (1) {

			bool message_too_long = true;
			char message_buf[1000];
            uint16_t length;

			while (message_too_long == true) {
				printf("Enter a message: ");

				memset(message_buf, 0, 1000);
				fgets(message_buf, sizeof(message_buf), stdin);

				length = strlen(message_buf)-1; /* remove the new line charater */

                if (length > 1000) {
					printf("message can't be greater than 1000 characters\n");
				}
				else {
					message_too_long = false;
				}
			}

            uint16_t nbo_msg_length = htons(length);
            n = send(sd, &nbo_msg_length, sizeof(nbo_msg_length), MSG_NOSIGNAL);
			check_valid_send_and_recv(n);

			char message[length +1];
            message[length] = '\0';
			for (int i=0; i<length; i++) {
				message[i] = message_buf[i];
			}
			n = send(sd, &message, strlen(message), MSG_NOSIGNAL);
			check_valid_send_and_recv(n);
		}
	}
	else {
		close(sd);
	}
} // main
