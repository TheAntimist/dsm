//Program 1 of the sequential consistency check

#include <stdlib.h>
#include <stdio.h>
#include "node.hpp"

int main(int argc, char* argv[])
{
    //psu_dsm_register_datasegment(&a, 4096*2);
	int * a = (int *)psu_dsm_malloc("a", 4096);
	this_thread::sleep_for(chrono::seconds(5));
	*a = 54;
	cout << "a=" << *a << endl;
	cout << "Success for P1" << endl;
	return 0;
}
