#include "psu_lock.h"

void psu_init_lock(int lockno) {
    Node::instance.create_new_lock(lockno);
	
	cout << "[debug] Initialized lock " << lockno
		 << endl;
}

void psu_mutex_lock(int lockno) {
    Node::instance.lock_enter_cs(lockno);
}
void psu_mutex_unlock(int lockno) {
    Node::instance.lock_exit_cs(lockno);
}