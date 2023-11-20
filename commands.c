#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define MAX_UID 7
#define MAX_PASSWORD 9
/*
login UID password – following this command the User application sends 
a  message  to  the  AS,  using  the  UDP  protocol,  confirm  the  ID,  UID,  and 
password  of  this  user, or  register  it  if  this  UID  is  not  present  in  the  AS 
database.  
The  result  of  the  request  should  be  displayed:  successful  login,  incorrect  login 
attempt, or new user registered.
Login. Each user is identified by a user ID UID, a 6-digit IST student number, and a 
password composed of 8 alphanumeric (only letters and digits) characters. When the 
AS receives a login request  it will inform of a successful or incorrect login attempt 
or, in case the UID was not present at the AS, register a new user. 
*/
void login_command(char *buffer, int fd, struct addrinfo *res) {
	char UID[MAX_UID];
	char password[MAX_PASSWORD];
	int n;

	char *message = malloc(sizeof(char) * 128);
	char *reply = malloc(sizeof(char) * 128);

	sscanf(buffer, "%*s %s %s", UID, password);
	// validar comando !

	
	sprintf(message, "LIN %s %s", UID, password);
	n = sendto(fd, message, strlen(message), 0, res->ai_addr, res->ai_addrlen);
	if (n == -1)
		exit(1);

	n = recvfrom(fd, reply, 128, 0, NULL, NULL);
	if (n == -1)
		exit(1);

	if (strncmp(reply, "status = OK", 11) == 0) {
        printf("Successful login.\n");
    } else if (strncmp(reply, "status = NOK", 12) == 0) {
        printf("Incorrect login attempt.\n");
    } else if (strncmp(reply, "status = REG", 12) == 0) {
        printf("New user registered.\n");
    } else {
        printf("Unexpected response from the server.\n");
    }

	free(message);
	free(reply);
}
