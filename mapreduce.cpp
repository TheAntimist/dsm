#include <string>
#include <cstring>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <boost/regex.hpp>
#include <algorithm>

#include "node.hpp"
#include "psu_lock.h"
#include "psu_mr.h"

using namespace std;

#define SIZE_OF_DB 8192
#define NUM_THREADS 4


struct keyvalue{
	char word[8];
	int value;
};
typedef struct keyvalue keyvalue;
keyvalue kv[SIZE_OF_DB] __attribute__ ((aligned (4096)));
int c __attribute__ ((aligned (4096)));

unordered_map<string, int> localKv;
int total_nums, node_num;
string filename;
boost::regex word_regex("\\w+");

void *map_function(void *);
void * reduce_function(void *inp);

int get_file_lines(string filename) {
  int lines = 0;
  ifstream file(filename);
  for(string line; getline(file, line); lines++) {}
  // One extra
  lines--;
  file.close();
  return lines;
}

void setup()
{
	for(int i = 0; i < SIZE_OF_DB; ++i)
	{
		kv[i].value = 1;
	}
}


int main(int argc, char *argv[]) {
  if (argc < 4) {
    cout << "Wrong number of args.\n";
    cout << "<map_reduce> <node_num> <total_nums> <input_file>\n";
    return 1;
  }
  node_num = atoi(argv[1]);
  total_nums = atoi(argv[2]);
  filename = string(argv[3]);

  psu_dsm_register_datasegment(&kv, SIZE_OF_DB*sizeof(keyvalue) + 2*sizeof(int));
  kv[0].value = 1;

  psu_init_lock(0);

  if (node_num == 0) {
    setup();
    kv[2].value = kv[1].value = kv[0].value = 0;
  } else {
    while (kv[0].value != 0);
  }
  
  cout << "[info] Node=" << node_num << " | Total Nodes=" << total_nums << " | Filename=" << filename << endl;
  
  psu_mutex_lock(0);
  kv[2].value++;
  psu_mutex_unlock(0);

  psu_mr_setup(total_nums);
  psu_mr_map(map_function, NULL, NULL, &(kv[1].value));
  psu_mr_reduce(reduce_function, NULL, NULL, &(kv[1].value));

  psu_mutex_lock(0);
  kv[2].value--;
  cout << "[debug] Total Nodes remaining=" << kv[2].value << endl;
  psu_mutex_unlock(0);

  if (node_num == 0) {
    while(kv[2].value != 0);

    int size = kv[0].value;
    cout << "[info] Writing " << size <<" words to file\n" ;
    ofstream file("output.txt");
    for(int i = 0; i < size; i++) {
      file << kv[i+3].word << " : " << kv[i+3].value << endl;
    }
    file.close();
  }
  return 0;
}

void * map_function(void * k) {
  int lines = get_file_lines(filename);
  int per_node_segment = lines / total_nums;
  int start_line = node_num * per_node_segment, end_line = node_num == (total_nums - 1) ? lines + 1 : start_line + per_node_segment;
  cout << "[debug] File[" << filename << "] | start = " << start_line << " | end = " << end_line << " | segment_size = " << per_node_segment << endl;

  int i = -1;
  string line;
  ifstream file(filename);
  while((i + 1) != start_line && getline(file, line)) {
    i++;
  }

  while(i++ < (end_line - 1) && getline(file, line)) {
 
    boost::sregex_iterator it(line.begin(), line.end(), word_regex);
    boost::sregex_iterator end;
    for (; it != end; ++it) {
        string word = it->str();
        localKv[word] += 1;
    }
  }
  file.close();
}

void * reduce_function(void *inp) {
  psu_mutex_lock(0);
  for (auto const& x : localKv) {
      string word_s = x.first;
      const char * word = word_s.c_str();
      int value = x.second;
      int size = kv[0].value, val = 0;
      bool present = false;
      for (int i = 0; i < size; i++) {
        if (strcmp(kv[i+3].word, word) == 0) {
          present = true;
          kv[i+3].value += 1;
          val = kv[i+3].value;
          break;
        }
      }

      if (!present) {
        kv[0].value++;
        strcpy(kv[size+1].word, word);
        kv[size+1].value = 1;
        val = kv[size+1].value;
      }
      cout << "[debug] word=" << word << " | value=" << val << endl;
  }
  psu_mutex_unlock(0);
}
