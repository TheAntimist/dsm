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
#include <float.h>

using namespace std;

#include "node.hpp"
#include "psu_lock.h"


#define MAX_SIZE 8192
#define NUM_CENTROIDS 4

struct point {
    float x, y;
};
typedef struct point point;
int g __attribute__ ((aligned (4096)));
point results[NUM_CENTROIDS][MAX_SIZE] __attribute__ ((aligned (4096)));
int c __attribute__ ((aligned (4096)));

string filename;
int num_points;
int num_centroids;
int node_id;
int total_nodes;

vector<point> centroids;

void map_kmeans(void * inpdata, void * outpdata) {
    int num_points;
	int num_centroids;
    
	string line;	
	ifstream infile(filename);
		
	infile >> num_points;
	infile >> num_centroids;


    results[0][0].x = results[0][0].y = 0.0f;
    results[1][0].x = results[1][0].y = 0.0f;
    results[2][0].x = results[2][0].y = 0.0f;
    results[3][0].x = results[3][0].y = 0.0f;

	
    vector<point> points;
    
    int segment_size = num_points / total_nodes;

    int start = node_id * segment_size,
        end = node_id == (total_nodes - 1) ? num_points : start + segment_size;
    cout << "[debug] File[" << filename << "] | start = " << start << " | end = " << end << " | segment_size = " << segment_size << endl;
    int points_read = -1;

    float a, b;

    //loop through the file to go to line
    while((points_read + 1) != start && (infile >> a >> b)) {
        points_read++;
    }

    while(points_read++ < (end - 1) && (infile >> a >> b)) {
        point p;
        p.x = a;
        p.y = b;
        points.push_back(p);

        cout <<  "Point x=" << a << " y=" << b << endl;        
    }    

    while(points_read++ < num_points && (infile >> a >> b)) {}

    while(points_read++ <= (num_points + 4) && (infile >> a >> b)) {
        point p;
        p.x = a;
        p.y = b;
		centroids.push_back(p);
        cout <<  "centroid x=" << a << " y=" << b << endl;
    }
	infile.close();

	psu_mutex_lock(0);
	for(auto p : points) {

		float x1 = p.x;
		float y1 = p.y;
		
		float min_distance = FLT_MAX;
		int centroid;


		for(int i = 0; i < 4; i++) {
	    
		    float x2 = centroids[i].x;
		    float y2 = centroids[i].y;

		    float dist = pow(x2 - x1, 2) + pow(y2 - y1, 2);
		    dist = sqrt(dist);

		    if(dist < min_distance){
		        min_distance = dist;
		        centroid = i;
		    }
		}


		int size = (int)(results[centroid][0].x);
		results[centroid][size+2] = p;
		results[centroid][0].x = (float)(size + 1);
		cout << "size=" << size << " x=" <<p.x << " y=" << p.y << endl;

        }

	psu_mutex_unlock(0);

	Node::instance.mr_barrier();
}

float round_2(float arg) {
  return round(arg * 100) / 100;
}

void kmeans_reducer(int centroid_id) {

  float x = 0, y = 0;


    for(int j = 0; j < results[centroid_id][0].x; j++) {
        x = x + results[centroid_id][j + 2].x;
        y = y + results[centroid_id][j + 2].y;
    }
    
    float x_res = round_2(x / results[centroid_id][0].x);
    float y_res = round_2(y / results[centroid_id][0].x);

    cout << "Centroid " << centroid_id + 1 << ": " << x_res << " " << y_res << endl;

    results[centroid_id][1].x = x_res;
    results[centroid_id][1].y = y_res;
}


void reduce_kmeans(void *inpdata, void *opdata) {

    for(int i = 0; i < 4; i++) {
        if(node_id == i){
            kmeans_reducer(i);
        }
    }


	if(node_id + 1 == total_nodes) {
	  for(int i = total_nodes; i < 4; i++) {
		kmeans_reducer(i);
	  }    
	}

	Node::instance.mr_barrier();
}


int main(int argc, char *argv[]){

	if (argc < 4){
		cout << "Wrong number of args.\n";
		cout << "<kmeans> <input_file_name> <node_id> <total_nodes>\n";
		return 1;
	}

		
	filename = string(argv[1]);
    node_id = atoi(argv[2]);
    total_nodes = atoi(argv[3]);
	cout << &results << " " << NUM_CENTROIDS * MAX_SIZE * sizeof(point);

    psu_dsm_register_datasegment(&results, NUM_CENTROIDS * MAX_SIZE * sizeof(point));
 
	psu_init_lock(0);

	Node::instance.mr_setup(total_nodes);
    map_kmeans(NULL, NULL);

	cout << "Finished map, going into reduce" << endl;

	Node::instance.mr_setup(total_nodes);
    reduce_kmeans(NULL, NULL);

	ofstream out("output_kmeans.txt");
	for (int i = 0; i < 4; i++) {
	  out << results[i][1].x << " " << results[i][1].y << endl;
	}
	out.close();
	return 0;
}
