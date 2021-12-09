#include "psu_mr.h"

void psu_mr_setup(int _threads) {
    // TODO: Check if this works.
    //psu_dsm_register_datasegment(&pending_threads, sizeof(pending_threads));
    Node::instance.mr_setup(_threads);
}
void psu_mr_map(void* (*map_fp)(void*), void *inpdata, void *opdata) {
    cout << "\n\n[info] Starting Map function.\n\n";

    void * result = map_fp(inpdata);
    if (opdata != nullptr) {
        opdata = result;
    }
    
    Node::instance.mr_barrier();
    cout << "\n\n[info] Exiting Map function.\n\n";
}
void psu_mr_reduce(void* (*reduce_fp)(void*), void *inpdata, void *opdata) {
    cout << "\n\n[info] Starting Reduce function.\n\n";

    void * result = reduce_fp(inpdata);
    if (opdata != nullptr) {
        opdata = result;
    }
    Node::instance.mr_barrier();
    cout << "\n\n[info] Exiting Reduce function.\n\n";
}