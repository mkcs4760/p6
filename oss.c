#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <sys/ipc.h> 
#include <sys/shm.h>
#include <sys/msg.h> 
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/time.h>
#include "messageQueue.h"

struct frame {
	//a frame consists of a dirty bit, a reference byte, the process that has a page being stored here, and the page of that process that is being stored here
	bool dirtyBit;
	unsigned char referenceByte;
	int processStored;
	int pageStored;
};

struct page {
	int myPID;
	int pageTable[32];
};

//int shmid; //shared memory id, made a global so we can use it easily in errorMessage

//takes in program name and error string, and runs error message procedure
void errorMessage(char programName[100], char errorString[100]){
	char errorFinal[200];
	sprintf(errorFinal, "%s : Error : %s", programName, errorString);
	perror(errorFinal);
	
	//destroy message queue...NEED TO FIND A WAY TO DO THAT WITHOUT GLOBAL...
	
	//destroy shared memory


	kill(-1*getpid(), SIGKILL);
}

unsigned char shiftRight(unsigned char value) {
	return value / 2;
}

unsigned char setMostSignificantBit(unsigned char value) {
	if (value < 128) { //most significant bit is not set
		value += 128;
	}
	return value;
}

unsigned char resetReferenceByte() {
	return 128;
}
	
int savePID(int childPID, struct page PCB[]) {
	printf("we made it here\n");
	int i;
	for (i = 0; i < 18; i++) {
		printf("Let's compare %d and %d...", PCB[i].myPID, 0);
		if (PCB[i].myPID < 1) {
			PCB[i].myPID = childPID;
			return 0;
		}
	}
	printf("and now we're done\n");
	return 1;
}	

int findPIDInPCT(int PID, struct page PCB[]) {
	int i;
	for (i = 0; i < 18; i++) {
				
		if (PCB[i].myPID == PID) {
			//this is the slot that we're in
			return i;		
		}
	}
	return -1;	
}

int getAFrame(struct frame frameTable[]) {
	int i;
	for (i = 0; i < 256; i++) {
		if (frameTable[i].processStored == -1) {
			return i;
		}
	}
	return -1;
}

void printPCB(struct page PCB[]) {
	int i, j;
	for (i = 0; i < 18; i++) {
		printf("%d: ", PCB[i].myPID);
		for (j = 0; j < 32; j++) {
			printf("%d ", PCB[i].pageTable[j]);
		}
		printf("\n");
	}
}

void printFrameTable(struct frame frameTable[]) {
	int i;
	printf("Frame#\tDB\tRB\tProcess:Page\n");
	for (i = 0; i < 256; i++) {
		printf("%d\t%d\t%d\t%d:%d\n", i, frameTable[i].dirtyBit, frameTable[i].referenceByte, frameTable[i].processStored, frameTable[i].pageStored);
	}
}

int main(int argc, char *argv[]) {
	
	//this section of code allows us to print the program name in error messages
	char programName[100];
	strcpy(programName, argv[0]);
	if (programName[0] == '.' && programName[1] == '/') {
		memmove(programName, programName + 2, strlen(programName));
	}
	
	printf("Welcome to Project 6\n");
	
	//we will need a frame table, as well as 18 page tables
	struct frame frameTable[256];
	//int page[32]; //each process has 32 pages of memory
	//int page[32] pageTable[18];
	
	//each process needs an array of 32 ints
	struct page PCB[18];
	
	
	int i, j;
	//printf("Frame#\tDB\tRB\tProcess:Page\n");
	for (i = 0; i < 256; i++) {
		//preset everything to starting values
		frameTable[i].dirtyBit = false;
		frameTable[i].referenceByte = 0;
		frameTable[i].processStored = frameTable[i].pageStored = -1; //-1 means empty, since 0 is a valid entry
		//printf("%d\t%d\t%d\t%d:%d\n", i, frameTable[i].dirtyBit, frameTable[i].referenceByte, frameTable[i].processStored, frameTable[i].pageStored);
	}
	
	for (i = 0; i < 18; i++) {
		PCB[i].myPID = 0;
		printf("%d: ", PCB[i].myPID);
		for (j = 0; j < 32; j++) {
			PCB[i].pageTable[j] = -1; //we set an empty value to -1, since 0 could be a valid entry
			printf("%d ", PCB[i].pageTable[j]);
		}
		printf("\n");
	}
	
	key_t smKey = 1094;
	int *clockSecs, *clockNano;
	int shmid = shmget(smKey, sizeof(int*) + sizeof(long*), IPC_CREAT | 0666); //this is where we create shared memory
	if(shmid < 0) {
		errorMessage(programName, "Function shmget failed. ");
	}
	//attach ourselves to that shared memory
	clockSecs = shmat(shmid, NULL, 0); //attempting to store 2 numbers in shared memory
	clockNano = clockSecs + 1;
	if((clockSecs == (int *) -1) || (clockNano == (int *) -1)) {
		errorMessage(programName, "Function shmat failed. ");
	}
	*clockSecs = 0;
	*clockNano = 0;
	
	printf("We've got shared memory!\n");
	
	//now message queue
	key_t mqKey = 2094;
    //msgget creates a message queue and returns identifier 
    int msqid = msgget(mqKey, 0666 | IPC_CREAT);  //create the message queue
	if (msqid < 0) {
		errorMessage(programName, "Error using msgget for message queue ");
	} else {
		printf("We have message queue!\n");
	}
	//we are now ready to send messages whenever we desire
	
	
	
	bool terminate = false;
	bool makeChild = true;
	int temp;
	while (terminate != true) {
		
		*clockNano += 10000;
		if (*clockNano >= 1000000000) { //increment the next unit
			*clockSecs += 1;
			*clockNano -= 1000000000;
		}
		
		
		if (makeChild == true) {
			pid_t pid;
			pid = fork();
							
			if (pid == 0) { //child
				execl ("user", "user", NULL);
				errorMessage(programName, "execl function failed. ");
			}
			else if (pid > 0) { //parent
				//numKidsRunning += 1;
				//write to output file the time this process was launched
				printf("Created child %d at %d:%d\n", pid, *clockSecs, *clockNano);
				//save this process id in the PCT
				if (savePID(pid, PCB) == 1) {
					errorMessage(programName, "Could not save process PID in PCB - no space available ");
				} else {
					printf("Process PID saved successfully\n");
				}
				
				makeChild = false;
			}
		}
		
		int receive;
		receive = msgrcv(msqid, &message, sizeof(message), getpid(), IPC_NOWAIT); //will wait until is receives a message
		if (receive > 0) {
			//we received a message
			printf("Message received from child: %s\n", message.mesg_text);
			printf("Process %d is request access to memory bank %d\n", message.return_address, message.mesg_value);
			
			//NOW WE PROCESS THIS REQUEST
			//3 options
				//no page fault
				//page fault and free frame
				//page fault and no free frame
				
			int ourPage = message.mesg_value / 1024;	
				
			
			int result = findPIDInPCT(message.return_address, PCB);
			if (result < 0) {
				errorMessage(programName, "Error validating return address of message received. Process does not appear to be currently running. ");
			}
			
			printf("We found that PID is stored in PCT[%d]\n", result);
			if (PCB[result].pageTable[ourPage] > -1) {
				//no page fault - our data is ther - handle correctly
				printf("Our data already exists in frame #%d\n", ourPage);
			} else {
				//a page fault has occured. We now check if there are any frames available.
				printf("Page fault!!\n");
				int myFrame = getAFrame(frameTable);
				if (myFrame == -1) {
					//no frames available...
					printf("and there don't appear to be any frames available...\n");
				} else {
					//store in this frame
					printf("But frame %d is open for the taking!!\n", myFrame);
				}
			}
			
			//for testing, let's print values
			printFrameTable(frameTable);
			printPCB(PCB);
			
			message.mesg_type = message.return_address;
			strncpy(message.mesg_text, "Parent to child", 100);
			
			message.return_address = getpid();
			int send = msgsnd(msqid, &message, sizeof(message), 0); //send message
			if (send == -1) {
				errorMessage(programName, "Error on msgsnd");
			}
		}
		
		
		temp = waitpid(-1, NULL, WNOHANG);
		if (temp > 0) {
			printf("Child %d ended at %d:%d\n", temp, *clockSecs, *clockNano);
			terminate = true;
		}
		
	}
	
	
	//destroy shared memory
	printf("Parent terminating %d:%d\n", *clockSecs, *clockNano);
	int ctl_return = shmctl(shmid, IPC_RMID, NULL);
	if (ctl_return == -1) {
		errorMessage(programName, "Function scmctl failed. ");
	}
	//close message queue
	int mqDestroy = msgctl(msqid, IPC_RMID, NULL);
	if (mqDestroy == -1) {
		errorMessage(programName, " Error with msgctl command: Could not remove message queue ");
		exit(1);
	}	
	
	printf("End of program\n");
	
	
	return 0;
}