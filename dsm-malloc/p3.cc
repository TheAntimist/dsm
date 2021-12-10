//Program 3 of the sequential consistency check

#include <stdlib.h>
#include <stdio.h>
#include "psu_dsm_system.h"

int main(int argc, char* argv[])
{
    this_thread::sleep_for(chrono::seconds(20));
	int * b = (int *)psu_dsm_malloc("b", 4096);
	while (*b != 20);
	char * c = (char *)psu_dsm_malloc("c", 4096);
	cout << "c=" << c << endl;

	printf("Program 3 success!");
	return 0;
}
