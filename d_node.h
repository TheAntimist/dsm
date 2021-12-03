#ifndef __D_NODE_H__
#define __D_NODE_H__

#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>
#include <string>
#include <vector>
#include <pthread.h>
#include <tuple>

using namespace std; 

class D_mem {

public:
    string name;
    int numPages;
    vector<tuple<vector<int>, pthread_mutex_t *> drt;    

    D_mem(int num_pages, string name){

        this.numPages = num_pages;
        this.name = name;
        vector<int> v1;
        for(int i = 0; i < this.numPages, i++){
            for(int j = 0; j < 32; j++){
                v1.push_back(0);
            }
            v1[31] = -1; //NOT A STATE
            pthread_mutex_t *mtx = malloc(sizeof(pthread_mutex_t));
            drt.push_back(make_tuple(v1, mtx);
        }    

   }

};

#endif
