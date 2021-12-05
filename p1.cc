//Program 1 of the sequential consistency check

#include <stdlib.h>
#include <stdio.h>
#include "node.hpp"

int a __attribute__ ((aligned (4096)));
int b __attribute__ ((aligned (4096)));
int c __attribute__ ((aligned (4096)));

int main(int argc, char* argv[])
{
    printf("Address of a is: %x\n", &a);
    printf("Address of b is: %x\n", &b);
	psu_dsm_register_datasegment(&a, 4096*2);
	a = 1;
	//cout << a << endl;
	
	//b = 2;

	//cout << b << endl;

	cout << "Success for P1" << endl;
	while(true);
	return 0;
}
