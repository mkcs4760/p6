#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ipc.h> 
#include <sys/shm.h>
#include <sys/msg.h> 
#include <string.h>
#include <time.h>
#include <signal.h>
#include "messageQueue.h"

//takes in program name and error string, and runs error message procedure
void errorMessage(char programName[100], char errorString[100]){
	char errorFinal[200];
	sprintf(errorFinal, "%s : Error : %s", programName, errorString);
	perror(errorFinal);
	
	kill(-1*getpid(), SIGKILL);
}

int main(int argc, char *argv[]) {
	//this section of code allows us to print the program name in error messages
	char programName[100];
	strcpy(programName, argv[0]);
	if (programName[0] == '.' && programName[1] == '/') {
		memmove(programName, programName + 2, strlen(programName));
	}
	
	
	printf("Child %d is alive!!\n", getpid());
	
	int shmid;
	key_t key = 1094;
	int *clockSecs, *clockNano;
	shmid = shmget(key, sizeof(int*) + sizeof(long*), 0666);
	if(shmid < 0) {
		perror("Shmget error in user process ");
		exit(1);
	}
	//attach ourselves to that shared memory
	clockSecs = shmat(shmid, NULL, 0); //attempting to store 2 numbers in shared memory
	clockNano = clockSecs + 1;
	if((clockSecs == (int *) -1) || (clockNano == (int *) -1)) {
		perror("shmat error in user process");
		exit(1);
	}

	
	//now message queue
	key_t mqKey = 2094;
    //msgget creates a message queue and returns identifier 
    int msqid = msgget(mqKey, 0666 | IPC_CREAT);  //create the message queue
	if (msqid < 0) {
		errorMessage(programName, "Error using msgget for message queue ");
	} else {
		printf("We have shared memory!");
	}
	//we are now ready to send messages whenever we desire
	
	sleep(2);
	
	message.mesg_type = getppid();
	strncpy(message.mesg_text, "child to parent", 100);
	message.return_address = getpid();
	
	int send = msgsnd(msqid, &message, sizeof(message), 0);
	if (send == -1) {
		perror("Error on msgsnd\n");
	}
	printf("CHILD: %d sent message --- awaiting response...\n", getpid());
	int receive;
	receive = msgrcv(msqid, &message, sizeof(message), getpid(), 0); //will wait until is receives a message
	if (receive < 0) {
		perror("No message received\n");
	}
	printf("Data Received is: %s \n", message.mesg_text);
	
	printf("Child %d is shutting down at time %d:%d\n", getpid(), *clockSecs, *clockNano);
	return 0;
}