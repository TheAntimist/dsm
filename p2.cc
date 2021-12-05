//Program 2 of the sequential consistency check

#include <stdlib.h>
#include <stdio.h>
#include "node.hpp"

int a __attribute__ ((aligned (4096)));
int b __attribute__ ((aligned (4096)));
int c __attribute__ ((aligned (4096)));

int main(int argc, char* argv[])
{
    printf("a address: %x\n", &a);
    printf("b address: %x\n", &b);
	psu_dsm_register_datasegment(&a, 4096*2);
	while (a == 0);
	cout << "Came out of while loop" << endl;
	b = 1;
    cout << "Program 2 success!" << endl;
    while(true);
	return 0;
}
