#include "node.hpp"
#include "psu_lock.h"
#include <chrono>
#include <thread>
#include <random>

int main() {
  std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<std::mt19937::result_type> dist(1,5);
	psu_init_lock(0);
	while (true) {
	  int r = dist(rng);
	  cout << "[debug] Sleeping for " << r << " seconds\n";
		this_thread::sleep_for(chrono::seconds(r));
		psu_mutex_lock(0);
		r = dist(rng);
		cout << "[debug] In Lock. Sleeping for " << r << " seconds.\n";
		this_thread::sleep_for(chrono::seconds(r));
		psu_mutex_unlock(0);	
	}
	return 0;
}
