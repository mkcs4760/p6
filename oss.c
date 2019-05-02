#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <sys/ipc.h> 
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

int main(int argc, char *argv[]) {
	
	
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
	
	
	
	
	return 0;
}