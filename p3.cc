//Program 3 of the sequential consistency check

#include <stdlib.h>
#include <stdio.h>
#include "node.hpp"

int a __attribute__ ((aligned (4096)));
int b __attribute__ ((aligned (4096)));
int c __attribute__ ((aligned (4096)));

int main(int argc, char* argv[])
{
    printf("address of a is: %x\n", &a);
    printf("address of b is %x\n", &b);
	psu_dsm_register_datasegment(&a, 4096*2);
	while (b == 0);

	printf("Program 3 success! ; a = %d and b = %d\n",a,b);
	return 0;
}
