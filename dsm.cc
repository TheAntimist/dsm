#include "dsm.h"
#define __USE_GNU
#include <ucontext.h>
#include <iostream>
#include <signal.h>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include <malloc.h>
#include <errno.h>
#include <sys/mman.h>


#define PAGE_SIZE 4096

using namespace std;

bool is_init = false;
bool is_default = false;
struct sigaction sig;
int num_pages = 0;
void* start_addr;


void invalidate_page(int page_num){

    void * addr = (void *) (((char *) start_addr) + PAGE_SIZE*page_num);

    mprotect(addr, PAGE_SIZE, PROT_NONE);

    return;
}

void* get_page_addr(void *addr, int page_num){
    return (void*)( ((char *) addr) + PAGE_SIZE*page_num);
}

void * get_page_addr(void *addr){

    int page_num = ((int) ((((int *) addr) - ((int*) start_addr)) / PAGE_SIZE)) - 1;
    
    return (void *)( ((char*) addr) + PAGE_SIZE*page_num);
}  


void request_access(void* addr, bool is_write){

    int page_num;
    


    if(is_default){
        page_num = (int) ((((int *) addr) - ((int*) start_addr)) / PAGE_SIZE) - 1;
    }

    //use rpc call to talk to directory

    //likely copy new page or request new page,etc,
    return;
}

void sighandler(int sig, siginfo_t *info, void *ctx){

    printf("Sighandler called\n");

    if((((ucontext_t *)ctx)->uc_mcontext.gregs[REG_ERR]) & 0x2){
        printf("write fault\n");

        request_access(info->si_addr, true);
    
        mprotect(get_page_addr(info->si_addr), PAGE_SIZE, PROT_READ | PROT_WRITE);

     
        //send ack back to directory   
    }
    else{
        printf("read fault\n");

        request_access(info->si_addr, false);

        mprotect(get_page_addr(info->si_addr), PAGE_SIZE, PROT_READ);
        //send ack back to directory
    }

    


}


void psu_dsm_setup(){
    is_init = true;
    sig = { 0 };
    sig.sa_flags = SA_SIGINFO;
    sig.sa_sigaction = &sighandler;
    if (sigaction(SIGSEGV, &sig, NULL) == -1){
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

}


psu_dsm_ptr_t psu_dsm_malloc(char *name, size_t size){


//Check if the given name is allocated (shared or remote), if not, create

//if name is not allocated, 

}



void psu_dsm_register_datasegment(void * psu_ds_start, size_t psu_ds_size){

    if(!is_init){
        psu_dsm_setup();
    }
    if(!is_default){
        is_default = true;
    }
    
    num_pages = (int) (psu_ds_size / PAGE_SIZE);
    start_addr = psu_ds_start;

    //rpc call to directory node to register segment
	// TODO:
	// register_segment("default", num_pages)
    
    for(int i = 0; i < num_pages; i++){
        mprotect(get_page_addr(psu_ds_start, i), PAGE_SIZE, PROT_NONE);
    }
    

}
