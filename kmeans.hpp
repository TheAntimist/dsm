#include <string>
#include <fstream>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <memory>
#include <thread>
#include <utility>
#include <mutex>
#include <condition_variable>
#include <unistd.h>
#include <math.h>


using namespace std;


void map_kmeans(void *inpdata, void *opdata);

void reduce_kmeans(void *inpdata, void *opdata);

void kmeans_init(string input_file);


