#include <stdio.h>
#define MAX_lim

float temp_conversion(int);

int main()
{
	int myInt;
	float celsius;
	
	printf("Enter a temp in fahrenheit: ");
	scanf("%d", &myInt);
	printf("\n");	
	printf("You entered: %d \n",myInt);

	celsius = temp_conversion(myInt);
	printf("Value in celsius: %f \n", celsius);

	return 0;
}

float temp_conversion(int value){

	float celsius;
	celsius = (value - 32) / 1.800;

	return celsius;
}
