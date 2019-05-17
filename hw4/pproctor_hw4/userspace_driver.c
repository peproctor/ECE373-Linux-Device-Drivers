#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <stdint.h>
#include <errno.h>
#include <inttypes.h>

/*Defines */
#define LED_OFFSET 0x00000E00
#define GPRC_OFFSET 0x00004074
#define REC_REG_OFFSET 0x00000100

#define REC_ENBL 0x00000002

#define LED_0 0x0000000E
#define LED_1 (LED_0 << 8)
#define LED_2 (LED_0 << 16)
#define LED_3 (LED_0 << 24)

#define LED_0_OFF 0x0000000F
#define LED_1_OFF (LED_0_OFF << 8)
#define LED_2_OFF (LED_0_OFF << 16)
#define LED_3_OFF (LED_0_OFF << 24)
#define LED_ALL_OFF 0xFFFFFFFF

/*Global Variables */
static int dev_mem_fd;

void main(int argc, char* argv[]) 
{

	//Variable declarations
	void *base_addr; 
	uint32_t *ledctl_addr;
	uint32_t *GPRC_addr;
	uint32_t *CNTL_addr;
	uint32_t init_val;
	uint32_t GPRC_val;	
	uint32_t values[] = {LED_3, LED_2, LED_1, LED_0};
	size_t mem_window_sz = 0x0001F400;
	off_t eth1_region0 = 0xF1200000;


	//open the memory map
	if((dev_mem_fd = open("/dev/mem", O_RDWR)) < 0)
	{
		printf("File descriptor: %d\n", dev_mem_fd);
		printf("Error occurred: %d\n",errno);
		return;

	}
	

	//do the mmap
	base_addr = mmap(NULL, mem_window_sz,
			PROT_READ | PROT_WRITE, MAP_SHARED,
			dev_mem_fd, eth1_region0);

	if(base_addr == MAP_FAILED)
	{
		printf("mmap failure\n");
		close(dev_mem_fd);
		return;
	}

	//Register setup
	ledctl_addr = (uint32_t *) (base_addr + LED_OFFSET);
	GPRC_addr = (uint32_t *) (base_addr + GPRC_OFFSET);
	CNTL_addr = (uint32_t *) (base_addr + REC_REG_OFFSET);

	//Getting ledctl initial value
	init_val = *ledctl_addr;

	//Enable packet receive
	*CNTL_addr = REC_ENBL;

	printf("LEDCTL initial value: %08x \n", init_val);

	//Turning LED0 and LED2 on
	*ledctl_addr =  LED_0 | LED_2 | LED_1_OFF | LED_3_OFF;
	printf("LEDCTL value 1: 0x%08x\n", *ledctl_addr);
	sleep(2);
	*ledctl_addr = LED_0_OFF | LED_1_OFF | LED_2_OFF | LED_3_OFF;
	printf("LEDCTL value 2: 0x%08x\n", *ledctl_addr);
	sleep(2);
	printf("finished first led part\n");

	printf("doing LED writes \n");

	//Looping through 5 times

	for(int i = 0; i < 5; i++) {
		for(int k = 0; k < 4; k++)
		{	
			*ledctl_addr = values[k];
			printf(".");
			fflush(stdout);
			sleep(1);
			*ledctl_addr = LED_ALL_OFF; 
		}
	}

	//Restoring ledctl init val
	printf("\n");
	*ledctl_addr = init_val; 

	//Checking GPRC
	GPRC_val = *GPRC_addr;
	printf("Good packets received count: %d \n", GPRC_val);

	if((munmap(base_addr, mem_window_sz)) < 0)
	{
		printf("munmap FAILED\n");
		printf("Error num: %d\n", errno);
		close(dev_mem_fd);
		return;
	}
	close(dev_mem_fd);
}
