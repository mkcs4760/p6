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
	//bool referenceByte[8];
	unsigned char referenceByte;
	int processStored;
	int pageStored;
};

struct page {
	int myPID;
	int pageTable[32];
};

int shmid; //shared memory id, made a global so we can use it easily in errorMessage

//takes in program name and error string, and runs error message procedure
void errorMessage(char programName[100], char errorString[100]){
	char errorFinal[200];
	sprintf(errorFinal, "%s : Error : %s", programName, errorString);
	perror(errorFinal);
	
	//destroy shared memory
	int ctl_return = shmctl(shmid, IPC_RMID, NULL);
	if (ctl_return == -1) {
		perror("Emergency shutdown of shared memory failed");
	}

	kill(-1*getpid(), SIGKILL);
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
	printf("Frame#\tDB\tRB\tProcess:Page\n");
	for (i = 0; i < 256; i++) {
		//preset everything to starting values
		frameTable[i].dirtyBit = false;
		frameTable[i].referenceByte = 00000000;
		frameTable[i].processStored = frameTable[i].pageStored = 0;
		printf("%d\t%d\t%d\t%d:%d\n", i, frameTable[i].dirtyBit, frameTable[i].referenceByte, frameTable[i].processStored, frameTable[i].pageStored);
	}
	
	for (i = 0; i < 18; i++) {
		PCB[i].myPID = 0;
		printf("%d: ", PCB[i].myPID);
		for (j = 0; j < 32; j++) {
			PCB[i].pageTable[j] = 0;
			printf("%d ", PCB[i].pageTable[j]);
		}
		printf("\n");
	}
	
	key_t key = 1094;
	int *clockSeconds, *clockNano;
	shmid = shmget(key, sizeof(int*) + sizeof(long*), IPC_CREAT | 0666); //this is where we create shared memory
	if(shmid < 0) {
		errorMessage(programName, "Function shmget failed. ");
	}
	//attach ourselves to that shared memory
	clockSeconds = shmat(shmid, NULL, 0); //attempting to store 2 numbers in shared memory
	clockNano = clockSeconds + 1;
	if((clockSeconds == (int *) -1) || (clockNano == (int *) -1)) {
		errorMessage(programName, "Function shmat failed. ");
	}
	
	printf("We've got shared memory!\n");
	
		//destroy shared memory
	printf("Parent terminating %d:%d\n", *clockSeconds, *clockNano);
	int ctl_return = shmctl(shmid, IPC_RMID, NULL);
	if (ctl_return == -1) {
		errorMessage(programName, "Function scmctl failed. ");
	}
	
	printf("End of program\n");
	
	
	return 0;
}