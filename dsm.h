#ifndef __DSM_H__
#define __DSM_H__


#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>


typedef void* psu_dsm_ptr_t;


psu_dsm_ptr_t psu_dsm_malloc(char *name, size_t size);

void psu_dsm_register_datasegment(void * psu_ds_start, size_t psu_ds_size);

void sighandler(int sig, siginfo_t *info, void *ucontext);

void psu_dsm_setup();

void request_access(void* addr, bool is_write);

void* get_page_addr(void *addr);

void* get_page_addr(void *addr, int page_num);

void invalidate_page(int page_num);

#endif

