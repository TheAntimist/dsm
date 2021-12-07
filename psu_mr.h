#include "node.hpp"
#include "psu_lock.h"
#include <thread>
#include <vector>

void psu_mr_setup(int threads);
void psu_mr_map(void* (*map_fp)(void*), void *inpdata, void *opdata);
void psu_mr_reduce(void* (*reduce_fp)(void*), void *ipdata, void *opdata);