#include "psu_mr.h"

void psu_mr_setup(int _threads) {
    // TODO: Check if this works.
    //psu_dsm_register_datasegment(&pending_threads, sizeof(pending_threads));
    psu_init_lock(1);
}
void psu_mr_map(void* (*map_fp)(void*), void *inpdata, void *opdata, int* running_threads) {
    cout << "\n\n[info] Starting Map function.\n\n";

    psu_mutex_lock(1);
    (*running_threads)++;
    psu_mutex_unlock(1);

    void * result = map_fp(inpdata);
    if (opdata != nullptr) {
        opdata = result;
    }
    psu_mutex_lock(1);
    (*running_threads)--;
    psu_mutex_unlock(1);
    
    // Wait until all complete.
    while(*running_threads > 0);
    cout << "\n\n[info] Exiting Map function.\n\n";
}
void psu_mr_reduce(void* (*reduce_fp)(void*), void *inpdata, void *opdata, int* running_threads) {
    cout << "\n\n[info] Starting Reduce function.\n\n";
    psu_mutex_lock(1);
    (*running_threads)++;
    psu_mutex_unlock(1);

    void * result = reduce_fp(inpdata);
    if (opdata != nullptr) {
        opdata = result;
    }
    psu_mutex_lock(1);
    (*running_threads)--;
    psu_mutex_unlock(1);
    
    // Wait until all complete.
    while(*running_threads > 0);
    cout << "\n\n[info] Exiting Reduce function.\n\n";
}