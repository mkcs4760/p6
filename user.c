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

int randomNum(int min, int max) {
	if (min == max) {
		return min;
	}
	int num = (rand() % (max - min) + min);
	return num;
}

//takes in program name and error string, and runs error message procedure
void errorMessage(char programName[100], char errorString[100]){
	char errorFinal[200];
	sprintf(errorFinal, "%s : Error : %s", programName, errorString);
	perror(errorFinal);
	
	kill(-1*getpid(), SIGKILL);
}

int main(int argc, char *argv[]) {
	srand(time(0) * getpid()); //placed here so we can generate random numbers later on
	
	//this section of code allows us to print the program name in error messages
	char programName[100];
	strcpy(programName, argv[0]);
	if (programName[0] == '.' && programName[1] == '/') {
		memmove(programName, programName + 2, strlen(programName));
	}
	
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
	}
	//we are now ready to send messages whenever we desire
	
	int terminate = 0;
	int loopCount = 0;
	while (terminate != 1) {
		int memoryRequest = randomNum(0, 32000);
		if (randomNum(1, 10) < 4) {
			//it's a write call
			memoryRequest = memoryRequest * -1; //negative values are write calls, while positive are read calls
		}
		
		message.mesg_type = getppid();
		message.mesg_value = memoryRequest;
		message.return_address = getpid();
		
		int send = msgsnd(msqid, &message, sizeof(message), 0);
		if (send == -1) {
			perror("Error on msgsnd\n");
		}
		int receive;
		receive = msgrcv(msqid, &message, sizeof(message), getpid(), 0); //will wait until is receives a message
		if (receive < 0) {
			perror("No message received\n");
		}
		loopCount++;
		
		if ((randomNum(1, 100) > 98) & (loopCount > 20)) {
			terminate = 1;
		}
	}

	return 0;
}