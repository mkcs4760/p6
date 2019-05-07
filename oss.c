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

#define PAGECOUNT 32
#define FRAMECOUNT 16//256
#define PROCESSCOUNT 18

struct frame {
	//a frame consists of a dirty bit, a reference byte, the process that has a page being stored here, and the page of that process that is being stored here
	bool dirtyBit;
	unsigned char referenceByte;
	int processStored;
	int pageStored;
};

struct page {
	int myPID;
	int pageTable[PAGECOUNT];
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

void printPCB(struct page PCB[]) {
	int i, j;
	for (i = 0; i < PROCESSCOUNT; i++) {
		printf("%d: ", PCB[i].myPID);
		for (j = 0; j < PAGECOUNT; j++) {
			printf("%d ", PCB[i].pageTable[j]);
		}
		printf("\n");
	}
}

void printFrameTable(struct frame frameTable[]) {
	int i;
	printf("Frame#\tDB\tRB\tProcess\tPage\n");
	for (i = 0; i < FRAMECOUNT; i++) { //SHOULD BE 256, NOT 40...
		printf("%d\t%d\t%d\t%d\t%d\n", i, frameTable[i].dirtyBit, frameTable[i].referenceByte, frameTable[i].processStored, frameTable[i].pageStored);
	}
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
	//printf("we made it here\n");
	int i;
	for (i = 0; i < PROCESSCOUNT; i++) {
		//printf("Let's compare %d and %d...", PCB[i].myPID, 0);
		if (PCB[i].myPID < 1) {
			PCB[i].myPID = childPID;
			return 0;
		}
	}
	//printf("and now we're done\n");
	return 1;
}	

int findPIDInPCT(int PID, struct page PCB[]) {
	int i;
	for (i = 0; i < PROCESSCOUNT; i++) {
				
		if (PCB[i].myPID == PID) {
			//this is the slot that we're in
			return i;		
		}
	}
	return -1;	
}

int findFrameByPage(int process, int page, struct frame frameTable[]) {
	int i;
	for(i = 0; i < FRAMECOUNT; i++) {
		if ((frameTable[i].processStored == process) && (frameTable[i].pageStored == page)) {
			return i;
		}
	}
	return -1;
}

int getAFrame(struct frame frameTable[]) {
	int i;
	for (i = 0; i < FRAMECOUNT; i++) {
		if (frameTable[i].processStored == -1) {
			return i;
		}
	}
	return -1;
}

int getSmallestFrame(struct frame frameTable[]) { //untested, but makes sense
	
	int i;
	int smallestFrame = -1;
	int smallestFrameValue = 256;
	for (i = 0; i < FRAMECOUNT; i++) {
		//printf("SMALL FRAME: Let's compare %d and %d\n", frameTable[i].referenceByte, smallestFrameValue);
		if (frameTable[i].referenceByte < smallestFrameValue) {
		//int cmp = memcmp(frameTable[i].referenceByte, smallestFrameValue, 1);
		//if (cmp < 0);
			smallestFrame = i;
			smallestFrameValue = frameTable[i].referenceByte;
		}
	}
	return smallestFrame;
}

int clearPage(int myProcess, int myPage, struct page PCB[]) {
	int i;
	for (i = 0; i < PROCESSCOUNT; i++) {
		//printf("Let's compare %d and %d...\n", PCB[i].myPID, myProcess);
		if (PCB[i].myPID == myProcess) {
			PCB[i].pageTable[myPage] = -1;
			return 0;
		}
	}
	return -1;
}

void clearProcessMemory(int process, struct page PCB[], struct frame frameTable[], char programName[100]) {
	printf("Before clearing...\n");
	printPCB(PCB);
	printFrameTable(frameTable);
	int result = findPIDInPCT(process, PCB);
	if (result < 0) {
		errorMessage(programName, "Error validating return address of message received. Process does not appear to be currently running. ");
	}
	PCB[result].myPID = 0;
	int i;
	for (i = 0; i < PAGECOUNT; i++) {
		PCB[result].pageTable[i] = -1; //we set an empty value to -1, since 0 could be a valid entry
	}
	//now clear frame table
	for (i = 0; i < FRAMECOUNT; i++) {
		if (frameTable[i].processStored == process) {
			frameTable[i].dirtyBit = false;
			frameTable[i].referenceByte = 0;
			frameTable[i].processStored = frameTable[i].pageStored = -1; //-1 means empty, since 0 is a valid entry
		}
	}
	//this should be cleared, double check to be sure
	printf("Process %d has been removed from the system\n", process);
	printPCB(PCB);
	printFrameTable(frameTable);
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
	struct frame frameTable[FRAMECOUNT];
	//int page[32]; //each process has 32 pages of memory
	//int page[32] pageTable[18];
	
	//each process needs an array of 32 ints
	struct page PCB[PROCESSCOUNT];
	
	
	int i, j;
	//printf("Frame#\tDB\tRB\tProcess:Page\n");
	for (i = 0; i < FRAMECOUNT; i++) {
		//preset everything to starting values
		frameTable[i].dirtyBit = false;
		frameTable[i].referenceByte = 0;
		frameTable[i].processStored = frameTable[i].pageStored = -1; //-1 means empty, since 0 is a valid entry
		//printf("%d\t%d\t%d\t%d:%d\n", i, frameTable[i].dirtyBit, frameTable[i].referenceByte, frameTable[i].processStored, frameTable[i].pageStored);
	}
	
	for (i = 0; i < PROCESSCOUNT; i++) {
		PCB[i].myPID = 0;
		//printf("%d: ", PCB[i].myPID);
		for (j = 0; j < PAGECOUNT; j++) {
			PCB[i].pageTable[j] = -1; //we set an empty value to -1, since 0 could be a valid entry
			//printf("%d ", PCB[i].pageTable[j]);
		}
		//printf("\n");
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
	
	//printf("We've got shared memory!\n");
	
	//now message queue
	key_t mqKey = 2094;
    //msgget creates a message queue and returns identifier 
    int msqid = msgget(mqKey, 0666 | IPC_CREAT);  //create the message queue
	if (msqid < 0) {
		errorMessage(programName, "Error using msgget for message queue ");
	} else {
		//printf("We have message queue!\n");
	}
	//we are now ready to send messages whenever we desire
	
	
	
	bool terminate = false;
	bool makeChild = true;
	int temp;
	int numMessageCalls = 0;
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
			//printf("Message received from child: %s\n", message.mesg_text);
			
			//NOW WE PROCESS THIS REQUEST
			//3 options
				//no page fault
				//page fault and free frame
				//page fault and no free frame
			printf("\n");	
			int ourPage = abs(message.mesg_value) / 1024;	
			printf("Process %d is request access to memory bank %d, which can be found in page %d\n", message.return_address, abs(message.mesg_value), ourPage);
			
			int result = findPIDInPCT(message.return_address, PCB);
			if (result < 0) {
				errorMessage(programName, "Error validating return address of message received. Process does not appear to be currently running. ");
			}
			
			//printf("We found that PID is stored in PCT[%d]\n", result);
			if (PCB[result].pageTable[ourPage] > -1) {
				//no page fault - our data is there - handle correctly
				printf("Our data already exists in in frame table!!\n");
				int myFrame = findFrameByPage(message.return_address, ourPage, frameTable);
				if (result < 0) {
					errorMessage(programName, "Errpr fomdomg page value in frame table. Inconsistent data ");
				}
				printf("We found it in frame %d\n", myFrame);
				
				//frameTable[myFrame].
				printf("Changing referenceByte from %d", frameTable[myFrame].referenceByte);
				frameTable[myFrame].referenceByte = setMostSignificantBit(frameTable[myFrame].referenceByte); //make sure this works!!!
				printf(" to %d\n", frameTable[myFrame].referenceByte);
				if (message.mesg_value < 0) {
					frameTable[myFrame].dirtyBit = true;
				}
				printf("%d\t%d\t%d\t%d\t%d\n", myFrame, frameTable[myFrame].dirtyBit, frameTable[myFrame].referenceByte, frameTable[myFrame].processStored, frameTable[myFrame].pageStored);
				
			} else {
				//a page fault has occured. We now check if there are any frames available.
				printf("Page fault!!\n");
				int myFrame = getAFrame(frameTable);
				if (myFrame == -1) {
					//no frames available...
					printf("and there don't appear to be any frames available...\n");
					//find the one with the smallest referenceByte and swap it outp
					
					//printFrameTable(frameTable);
					int smallestFrame = getSmallestFrame(frameTable);
					//this is the frame we want to swap outp
					printf("We believe that frame #%d has the smallest referencyByte and therfore should be swapped out\n", smallestFrame);
					
					//first we need to clear it from it's page table
					int result = clearPage(frameTable[smallestFrame].processStored, frameTable[smallestFrame].pageStored, PCB);
					if (result < 0) {
						errorMessage(programName, "Failed to find page. Unable to remove it and satisfy page fault ");
					}
					//increment clock, more if the dirty bit was set
					if (frameTable[smallestFrame].dirtyBit) {
						*clockNano += 50000;
					} else {
						*clockNano += 20000;
					}
					if (*clockNano >= 1000000000) { //increment the next unit
						*clockSecs += 1;
						*clockNano -= 1000000000;
					}
					
					//now we need to clear it in the frame table
					
					frameTable[smallestFrame].dirtyBit = false;
					frameTable[smallestFrame].referenceByte = 0;
					frameTable[smallestFrame].processStored = frameTable[i].pageStored = -1; //-1 means empty, since 0 is a valid entry
					
					//now we need to save our value in frame table
					frameTable[smallestFrame].referenceByte = resetReferenceByte(frameTable[smallestFrame].referenceByte);
					if (message.mesg_value < 0) {
						frameTable[smallestFrame].dirtyBit = true;
					}
					frameTable[smallestFrame].processStored = message.return_address;
					frameTable[smallestFrame].pageStored = ourPage;
					PCB[result].pageTable[ourPage] = smallestFrame; //save a link back
					//THIS NEEDS TO BE TESTED!!!
					
					
				} else {
					//store in this frame
					printf("But frame %d is open for the taking!!\n", myFrame);
					
					frameTable[myFrame].referenceByte = resetReferenceByte(frameTable[myFrame].referenceByte);
					if (message.mesg_value < 0) {
						frameTable[myFrame].dirtyBit = true;
					}
					frameTable[myFrame].processStored = message.return_address;
					frameTable[myFrame].pageStored = ourPage;
					PCB[result].pageTable[ourPage] = myFrame; //save a link back
					//THE ABOVE HUNK OF CODE APPEARS TO WORK
					
					printf("%d\t%d\t%d\t%d:%d\n", myFrame, frameTable[myFrame].dirtyBit, frameTable[myFrame].referenceByte, frameTable[myFrame].processStored, frameTable[myFrame].pageStored);
	
				}
			}
			
			//for testing, let's print values
			/*printFrameTable(frameTable);
			printPCB(PCB);*/
			
			message.mesg_type = message.return_address;
			strncpy(message.mesg_text, "Parent to child", 100);
			
			message.return_address = getpid();
			int send = msgsnd(msqid, &message, sizeof(message), 0); //send message
			if (send == -1) {
				errorMessage(programName, "Error on msgsnd");
			}

			numMessageCalls++;
			if (numMessageCalls % 10 == 0) { //shift every 10 message calls - NOTE THIS SHOULD PROBABLY BE A SEPARATE FUNCTION!!!
				printf("Shifting all referenceBytes rights...\n");
				int i;
				for (i = 0; i < FRAMECOUNT; i++) {
					frameTable[i].referenceByte = shiftRight(frameTable[i].referenceByte);
				}
			}
		}
		
		temp = waitpid(-1, NULL, WNOHANG);
		if (temp > 0) {
			printf("Child %d ended at %d:%d\n", temp, *clockSecs, *clockNano);
			//deallocate all values
			clearProcessMemory(temp, PCB, frameTable, programName);
			
			terminate = true;
		}
		
	}
	
	//print values for testing
	printFrameTable(frameTable);
	printPCB(PCB);
	
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