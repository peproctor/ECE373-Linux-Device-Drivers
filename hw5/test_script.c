#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/select.h>
#include <errno.h>


#define PATH_READ "/dev/ece_led" 
#define PATH_WRITE "/sys/module/driver_setup/parameters"


int main(int argc, char **argv){

	int fd,rea,wri;
	int count = 2;
	long input_buf[1] = {0};
	long output_buf[1] = {4};

	long *output_ptr;
 	output_ptr = &output_buf[0];

	if((fd = open(PATH_READ,O_RDWR /* O_RDWR*/)) < 0){
		printf("File not found.\n");
		perror("Dev open:");
		return errno;
	}
	

	if((rea = read(fd, input_buf, count)) < 0){
		printf("Bytes not read,rea = %d\n", rea);
		perror("Dev read");
		return errno;
	}	
	
	printf("Blink rate read from PCI device: %ld\n",input_buf[0]);	
	
		
	if((wri = write(fd, output_ptr , count)) < 0){
		printf("Bytes not written, wri = %d\n",wri);
		perror("Dev write");
		return errno;
			
	}	

	printf("Blink rate written to driver:%ld\n",(*output_ptr));

	close(fd);	
}

