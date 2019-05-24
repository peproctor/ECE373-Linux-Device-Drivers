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
	long input_buf[1] = {0x0};
	long output_buf[2] = {0xE, 0xF};

	long *output_ptr;
 	output_ptr =&output_buf[0];

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
	
	printf("Value read from PCI device: 0x%lx\n",input_buf[0]);	

	
	if((wri = write(fd, output_ptr , count)) < 0){
		printf("Bytes not written, wri = %d\n",wri);
		perror("Dev write");
		return errno;
			
	}	

	printf("Value written to driver:0x%lx\n",(*output_ptr));


	if((rea = read(fd, input_buf, count)) < 0){
		printf("Bytes not read,rea = %d\n", rea);
		perror("Dev read");
		return errno;
	}	
	
	printf("Value read from PCI device: 0x%lx\n",input_buf[0]);	
	
		

	if((wri = write(fd, ++output_ptr , count)) < 0){
                printf("Bytes not written, wri = %d\n",wri);
                perror("Dev write");
                return errno;

        }

	

	close(fd);	
}

