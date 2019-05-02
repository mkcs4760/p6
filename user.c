#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ipc.h> 
#include <sys/msg.h> 
#include <string.h>
#include <time.h>
#include "messageQueue.h"



int main(int argc, char *argv[]) {
	
	printf("Child %d is alive!!\n", getpid());
	
	
	
	printf("Child %d is shutting down\n", getpid());
	return 0;
}