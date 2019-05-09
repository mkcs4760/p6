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
#define FRAMECOUNT 256

int totalProcessesToLaunch = 18;
int sm_id;
int mq_id;

struct frame { //a frame consists of a dirty bit, a reference byte, the process that has a page being stored here, and the page of that process that is being stored here
	bool dirtyBit;
	unsigned char referenceByte;
	int processStored;
	int pageStored;
};

struct page {
	int myPID;
	int pageTable[PAGECOUNT];
	int numMemoryAccesses;
	int memoryAccessSecs;
	int memoryAccessNano;
};

//called whenever we terminate program
void endAll(int error) {
	//destroy shared memory
	shmctl(sm_id, IPC_RMID, NULL);

	//close message queue
	msgctl(mq_id, IPC_RMID, NULL);
	
	//destroy master process
	if (error)
		kill(-1*getpid(), SIGKILL);	
}

//takes in program name and error string, and runs error message procedure
void errorMessage(char programName[100], char errorString[100]){
	char errorFinal[200];
	sprintf(errorFinal, "%s : Error : %s", programName, errorString);
	perror(errorFinal);
	
	endAll(1);
	/*
	//destroy shared memory
	printf("Parent terminating %d:%d\n", *clockSecs, *clockNano);
	int ctl_return = shmctl(sm_id, IPC_RMID, NULL);
	if (ctl_return == -1) {
		errorMessage(programName, "Function scmctl failed. ");
	}
	//close message queue
	int mqDestroy = msgctl(mq_id, IPC_RMID, NULL);
	if (mqDestroy == -1) {
		errorMessage(programName, " Error with msgctl command: Could not remove message queue ");
		exit(1);
	}	

	kill(-1*getpid(), SIGKILL);*/
}

void printPCB(struct page PCB[], FILE * output) {
	int i, j;
	for (i = 0; i < totalProcessesToLaunch; i++) {
		fprintf(output, "%d: ", PCB[i].myPID);
		for (j = 0; j < PAGECOUNT; j++) {
			fprintf(output, "%d ", PCB[i].pageTable[j]);
		}
		fprintf(output, "\n");
	}
}

void printFrameTable(struct frame frameTable[], FILE * output) {
	int i;
	fprintf(output, "Frame#\tDB\tRB\tProcess\tPage\n");
	for (i = 0; i < FRAMECOUNT; i++) { //SHOULD BE 256, NOT 40...
		fprintf(output,"%d\t%d\t%d\t%d\t%d\n", i, frameTable[i].dirtyBit, frameTable[i].referenceByte, frameTable[i].processStored, frameTable[i].pageStored);
		if (frameTable[i].pageStored == -1 && frameTable[i].processStored != -1) {  //TESTING!!
			printf("ERROR!! -1 value found in frameTable!!\n");
			kill(-1*getpid(), SIGKILL);
		}
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
	int i;
	for (i = 0; i < totalProcessesToLaunch; i++) {
		if (PCB[i].myPID < 1) {
			PCB[i].myPID = childPID;
			return 0;
		}
	}
	return 1;
}	

int findPIDInPCT(int PID, struct page PCB[]) {
	int i;
	for (i = 0; i < totalProcessesToLaunch; i++) {				
		if (PCB[i].myPID == PID) {
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
		if (frameTable[i].referenceByte < smallestFrameValue) {
			smallestFrame = i;
			smallestFrameValue = frameTable[i].referenceByte;
		}
	}
	return smallestFrame;
}

int clearPage(int myProcess, int myPage, struct page PCB[]) {
	int i;
	for (i = 0; i < totalProcessesToLaunch; i++) {
		if (PCB[i].myPID == myProcess) {
			PCB[i].pageTable[myPage] = -1;
			return 0;
		}
	}
	return -1;
}

void clearProcessMemory(int process, struct page PCB[], struct frame frameTable[], char programName[100], FILE * output) {
	int result = findPIDInPCT(process, PCB);
	if (result < 0) {
		printPCB(PCB, output);
		errorMessage(programName, "Error validating return address of message received. Process does not appear to be currently running. ");
	}
	PCB[result].myPID *= -1;//set it to negative to signify it has terminated without clearing all the data
	fprintf(output, "%s: Process %d's memory access time is %d:%d over %d requests\n", programName, process, PCB[result].memoryAccessSecs, PCB[result].memoryAccessNano, PCB[result].numMemoryAccesses);
	int i;
	for (i = 0; i < PAGECOUNT; i++) {
		PCB[result].pageTable[i] = -1; 
	} //we intentionally don't clear any other values so we can use them for the final stats without having to build a new data structure
	//now clear frame table
	for (i = 0; i < FRAMECOUNT; i++) {
		if (frameTable[i].processStored == process) {
			frameTable[i].dirtyBit = false;
			frameTable[i].referenceByte = 0;
			frameTable[i].processStored = frameTable[i].pageStored = -1; //-1 means empty, since 0 is a valid entry
		}
	}
}

//called when interupt signal (^C) is called
void intHandler(int dummy) {
	printf(" Interupt signal received.\n");
	endAll(1);
}

	//double memoryAccessPerSecond = getMemoryAccessPerSecond();
	//double pageFaultsPerAccess = getPageFaultsPerAccess();
	//double averageMemoryAccessSpeed = getAverageMemoryAccessSpeed();
	
double getMemoryAccessPerSecond(int secs, int nano, int memoryAccess) {
	double fullTime = secs;
	double nanoDouble = (double)nano / 1000000000;
	//printf("We changed %d into %f\n", nano, nanoDouble);
	fullTime += nanoDouble;
	//printf("Full time is therefore %f\n", fullTime);
	double memoryAccessPerSecond = memoryAccess / fullTime;
	//printf("Since we had %d memoryAccesses, the memoryAccessPerSecond is %f\n", memoryAccess, memoryAccessPerSecond);
	return memoryAccessPerSecond;
}	

double getPageFaultsPerAccess(int numPageFaults, int memoryAccess) {
	double pageFaultsPerAccess = (double) numPageFaults / (double) memoryAccess;
	//printf("pageFaultsPerAccess is %f\n", pageFaultsPerAccess);
	return pageFaultsPerAccess;
}

double getAverageMemoryAccessSpeed(struct page PCB[]) {
	int i;
	double totalMemoryAccessSpeed = 0;
	for (i = 0; i < totalProcessesToLaunch; i++) {
		double actualTime = PCB[i].memoryAccessSecs;
		double nanoDouble = (double)PCB[i].memoryAccessNano / 1000000000;
		//printf("We changed %d into %f\n", PCB[i].memoryAccessNano, nanoDouble);
		actualTime += nanoDouble;
		//printf("Full time is therefore %f\n", actualTime);
	
		totalMemoryAccessSpeed += actualTime;
	}
	//printf("Full time is equal to %f\n", totalMemoryAccessSpeed);
	totalMemoryAccessSpeed /= (double)totalProcessesToLaunch;
	//printf("Average memory access speed is %f\n", totalMemoryAccessSpeed);
	
	
	return totalMemoryAccessSpeed;
}


int main(int argc, char *argv[]) {
	//this section of code allows us to print the program name in error messages
	char programName[100];
	strcpy(programName, argv[0]);
	if (programName[0] == '.' && programName[1] == '/') {
		memmove(programName, programName + 2, strlen(programName));
	}
	
	//first we process the getopt arguments
	int option;
	while ((option = getopt(argc, argv, "hn:")) != -1) {
		switch (option) {
			case 'h' :	printf("Help page for OS_Klein_project6\n"); //for h, we print data to the screen
						printf("Consists of the following:\n\tTwo .c file titled oss.c and user.c\n\tOne .h file titled messageQueue.h\n\tOne Makefile\n\tOne README file\n\tOne version control log.\n");
						printf("The command 'make' will run the makefile and compile the program\n");
						printf("Command line arguments for oss executable:\n");
						printf("\t-n\t<maxTotalChildren>\tdefaults to 18\n");
						printf("\t-h\t<NoArgument>\n");
						printf("Version control accomplished using github. Log obtained using command 'git log > versionLog.txt\n");
						exit(0);
						break;
			case 'n' :	if (atoi(optarg) <= 18) { //for n, we set the maximum of child processes we will have at a time
							totalProcessesToLaunch = atoi(optarg);
						}
						else {
							errno = 22;
							errorMessage(programName, "Cannot allow more then 18 process at a time. "); //the parent is running, so there's already 1 process running
						}
						break;
			default :	errno = 22; //anything else is an invalid argument
						errorMessage(programName, "You entered an invalid argument. ");
		}
	}
	
	printf("Welcome to Project 6\n");
	
	//we will need a frame table, as well as 18 page tables
	struct frame frameTable[FRAMECOUNT];
	//each process needs an array of 32 ints
	struct page PCB[totalProcessesToLaunch];
	
	int i, j;
	for (i = 0; i < FRAMECOUNT; i++) { //preset everything to starting values
		frameTable[i].dirtyBit = false;
		frameTable[i].referenceByte = 0;
		frameTable[i].processStored = frameTable[i].pageStored = -1; //-1 means empty, since 0 is a valid entry
	}
	
	for (i = 0; i < totalProcessesToLaunch; i++) {
		PCB[i].myPID = 0;
		for (j = 0; j < PAGECOUNT; j++) {
			PCB[i].pageTable[j] = -1; //we set an empty value to -1, since 0 could be a valid entry
		}
		PCB[i].numMemoryAccesses = PCB[i].memoryAccessSecs = PCB[i].memoryAccessNano = 0; //this should set all 3 values to 0 in one line of code
	}
	
	key_t smKey = 1094;
	sm_id = smKey;
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
	
	//now message queue
	key_t mqKey = 2094;
	mq_id = mqKey;
    //msgget creates a message queue and returns identifier 
    int msqid = msgget(mqKey, 0666 | IPC_CREAT);  //create the message queue
	if (msqid < 0) {
		errorMessage(programName, "Error using msgget for message queue ");
	} //we are now ready to send messages whenever we desire
	
	FILE * output;
	output = fopen("log.txt", "w");
	fprintf(output, "Error cases...\n\n");
	
	fprintf(output, "Memory Management Simulation\n\n");
	printf("Running memory management simulation\n");
	printf("Please note that this may take several seconds...\n");
	
	
	bool terminate = false;
	bool makeChild = true;
	int temp;
	int numMessageCalls = 0;
	int processesCalled = 0;
	int processesRunning = 0;
	int numPageFaults = 0;
	int numFullPageHits = 0;
	bool newCall = false;
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
				fprintf(output, "%s: Created process %d at %d:%d\n", programName, pid, *clockSecs, *clockNano);
				//save this process id in the PCT
				if (savePID(pid, PCB) == 1) {
					errorMessage(programName, "Could not save process PID in PCB - no space available ");
				}
				processesCalled++;
				processesRunning++;
				makeChild = false;
			}
		} else {
			if (processesCalled < totalProcessesToLaunch) {
				makeChild = true;
			}
		}
		
		int receive;
		receive = msgrcv(msqid, &message, sizeof(message), getpid(), IPC_NOWAIT); //will wait until is receives a message
		
		if (receive > 0) {
			newCall = true;
			//we received a message - now we process request
			//3 options
				//no page fault
				//page fault and free frame
				//page fault and no free frame
			int ourPage = abs(message.mesg_value) / 1024;	
			if (message.mesg_value < 0) {
				fprintf(output, "%s: Process %d requesting write of address %d, which can be found in page %d, at %d:%d\n", programName, message.return_address, abs(message.mesg_value), ourPage, *clockSecs, *clockNano);	
			} else {
				fprintf(output, "%s: Process %d requesting read of address %d, which can be found in page %d, at %d:%d\n", programName, message.return_address, abs(message.mesg_value), ourPage, *clockSecs, *clockNano);	
			}
			
			int result = findPIDInPCT(message.return_address, PCB);
			if (result < 0) {
				printPCB(PCB, output);
				fclose(output);
				errorMessage(programName, "Error validating return address of message received. Process does not appear to be currently running. ");
			}
			PCB[result].numMemoryAccesses++;
			if (PCB[result].pageTable[ourPage] > -1) { //no page fault - our data is there - handle correctly
				int myFrame = findFrameByPage(message.return_address, ourPage, frameTable);
				if (result < 0) {
					errorMessage(programName, "Error finding page value in frame table. Inconsistent data ");
				}
				
				*clockNano += 500;
				//TEMP:MWH308}
				if (*clockNano >= 1000000000) { //increment the next unit
					*clockSecs += 1;
					*clockNano -= 1000000000;
				}
				PCB[result].memoryAccessNano += 500;
				if (PCB[result].memoryAccessNano >= 1000000000) { //increment the next unit
					PCB[result].memoryAccessSecs += 1;
					PCB[result].memoryAccessNano -= 1000000000;
				}
				fprintf(output, "%s: Address %d found in frame %d, giving data to %d at %d:%d\n", programName, abs(message.mesg_value), myFrame, message.return_address, *clockSecs, *clockNano);
			
				frameTable[myFrame].referenceByte = setMostSignificantBit(frameTable[myFrame].referenceByte); //make sure this works!!!
				if (message.mesg_value < 0) {
					frameTable[myFrame].dirtyBit = true;
				}
			} else { //a page fault has occured. We now check if there are any frames available.
				fprintf(output, "%s: Process %d\'s request has resulted in a page fault\n", programName, message.return_address);
				numPageFaults++;
				int myFrame = getAFrame(frameTable);
				if (myFrame == -1) { //no frames available... find the one with the smallest referenceByte and swap it out
					numFullPageHits++;
					int smallestFrame = getSmallestFrame(frameTable); //this is the frame we want to swap out

					//first we need to clear it from it's page table
					if (clearPage(frameTable[smallestFrame].processStored, frameTable[smallestFrame].pageStored, PCB) < 0) {
						errorMessage(programName, "Failed to find page. Unable to remove it and satisfy page fault ");
					}
					//increment clock, more if the dirty bit was set
					if (frameTable[smallestFrame].dirtyBit) {
						*clockNano += 50000;
						PCB[result].memoryAccessNano += 50000;
					} else {
						*clockNano += 20000;
						PCB[result].memoryAccessNano += 20000;
					}
					if (*clockNano >= 1000000000) { //increment the next unit
						*clockSecs += 1;
						*clockNano -= 1000000000;
					}
					if (PCB[result].memoryAccessNano >= 1000000000) { //increment the next unit
						PCB[result].memoryAccessSecs += 1;
						PCB[result].memoryAccessNano -= 1000000000;
					}					
					fprintf(output, "%s: No frames are free. Clearing frame %d and swapping in process #%d page %d\n", programName, smallestFrame, message.return_address, ourPage); //validate that htis is correct!!!
	
					//now we need to save new frame over old frame correctly				
					frameTable[smallestFrame].dirtyBit = false;
					frameTable[smallestFrame].referenceByte = resetReferenceByte(frameTable[smallestFrame].referenceByte);
					if (message.mesg_value < 0) {
						fprintf(output, "%s: Dirty bit of frame %d set, adding additional time to the clock\n", programName, smallestFrame);
						frameTable[smallestFrame].dirtyBit = true;
					}
					frameTable[smallestFrame].processStored = message.return_address;
					frameTable[smallestFrame].pageStored = ourPage;
					PCB[result].pageTable[ourPage] = smallestFrame; //save a link back
					fprintf(output, "%s: Granting data back to %d at %d:%d\n", programName, message.return_address, *clockSecs, *clockNano);
				} else {
					//store in this frame
					*clockNano += 10000;
					PCB[result].memoryAccessNano += 10000;
					if (*clockNano >= 1000000000) { //increment the next unit
						*clockSecs += 1;
						*clockNano -= 1000000000;
					}
					if (PCB[result].memoryAccessNano >= 1000000000) { //increment the next unit
						PCB[result].memoryAccessSecs += 1;
						PCB[result].memoryAccessNano -= 1000000000;
					}
					fprintf(output, "%s: Frame %d is empty. Address %d written to to that frame at %d:%d\n", programName, myFrame, abs(message.mesg_value), *clockSecs, *clockNano);
			
					frameTable[myFrame].referenceByte = resetReferenceByte(frameTable[myFrame].referenceByte);
					if (message.mesg_value < 0) {
						frameTable[myFrame].dirtyBit = true;
					}
					frameTable[myFrame].processStored = message.return_address;
					frameTable[myFrame].pageStored = ourPage;
					PCB[result].pageTable[ourPage] = myFrame; //save a link back
				}
			}
			
			message.mesg_type = message.return_address;
			strncpy(message.mesg_text, "Parent to child", 100);
			
			message.return_address = getpid();
			int send = msgsnd(msqid, &message, sizeof(message), 0); //send message
			if (send == -1) {
				errorMessage(programName, "Error on msgsnd");
			}

			numMessageCalls++;
			if (numMessageCalls % 32 == 0) { //shift every 32 message calls - NOTE THIS SHOULD PROBABLY BE A SEPARATE FUNCTION!!!
				int i;
				for (i = 0; i < FRAMECOUNT; i++) {
					frameTable[i].referenceByte = shiftRight(frameTable[i].referenceByte);
				}
			}
		}
		
		temp = waitpid(-1, NULL, WNOHANG);
		if (temp > 0) {
			fprintf(output, "%s: Process %d ended at %d:%d\n", programName, temp, *clockSecs, *clockNano);
			//deallocate all values
			clearProcessMemory(temp, PCB, frameTable, programName, output);
			processesRunning--;
			if (processesCalled == totalProcessesToLaunch && processesRunning == 0) {
				terminate = true;
			}
		}
		
		if (numMessageCalls % 100 == 0 && newCall == true) {
			newCall = false;
			fprintf(output, "%s: Current memory layout after %d memory requests at time %d:%d is:\n", programName, numMessageCalls, *clockSecs, *clockNano);
			printFrameTable(frameTable, output);
		}
	}
	
	double memoryAccessPerSecond = getMemoryAccessPerSecond(*clockSecs, *clockNano, numMessageCalls);
	double pageFaultsPerAccess = getPageFaultsPerAccess(numPageFaults, numMessageCalls);
	double averageMemoryAccessSpeed = getAverageMemoryAccessSpeed(PCB);
	
	fprintf(output, "\n\tMemory access per second: %f\n", memoryAccessPerSecond);
	fprintf(output, "\tPage faults per access: %f\n", pageFaultsPerAccess);
	fprintf(output, "\tAverage memory access speed: %f\n", averageMemoryAccessSpeed);
	fprintf(output, "\tTotal page faults: %d\n", numPageFaults);
	fprintf(output, "\tNumber of times frame table was full: %d\n", numFullPageHits);
	fprintf(output, "\nEnd of log\n");
	
	fclose(output);
	
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