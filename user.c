#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ipc.h> 
#include <sys/shm.h>
#include <sys/msg.h> 
#include <string.h>
#include <time.h>
#include "messageQueue.h"



int main(int argc, char *argv[]) {
	
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

	
	
	
	
	printf("Child %d is shutting down at time %d:%d\n", getpid(), *clockSecs, *clockNano);
	return 0;
}