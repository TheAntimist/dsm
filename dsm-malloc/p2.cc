//Program 2 of the sequential consistency check

#include <stdlib.h>
#include <stdio.h>
#include "psu_dsm_system.h"
#include <chrono>
#include <thread>

int main(int argc, char* argv[])
{
	int * a = (int *)psu_dsm_malloc("a", 4096);
	while ((*a) != 54);
	cout << "Came out of while loop" << endl;
	int * b = (int *)psu_dsm_malloc("b", 4096);
	char * c = (char *)psu_dsm_malloc("c", 4096);
	c[0] = 'f';
	c[1] = c[2] = '0';
	c[3] = 'b';
	c[4] = '@';
	c[5] = 'R';
	c[6] = '\0';
	this_thread::sleep_for(chrono::seconds(10));
	*b = 20;
    cout << "Program 2 success!" << endl;
	return 0;
}
