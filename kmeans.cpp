#include "kmeans.hpp"


#define MAX_SIZE 10000
#define NUM_CENTROIDS 4

pair<float, float> kmeans[NUM_CENTROIDS][MAX_SIZE];

pair<float, float> centroids[NUM_CENTROIDS];

string filename;
int num_points;
int num_centroids;
int node_id;
int total_nodes;

void kmeans_init(){


    kmeans[0][0] = make_pair(0.0f, 0.0f);
    kmeans[1][0] = make_pair(0.0f, 0.0f);
    kmeans[2][0] = make_pair(0.0f, 0.0f);
    kmeans[3][0] = make_pair(0.0f, 0.0f);

	int num_points;
	int num_centroids; 
    int points_read;
    int point_counter = 0;
    
	string line;	
	ifstream infile(filename);
		
	infile >> num_points;
	infile >> num_centroids;

	
    vector<pair<float, float>> points; 
    
    int node_point_read = (int) (num_points / total_nodes);
    

    if(node_id + 1 = total_nodes){
        node_point_read = node_point_read + (num_points - total_nodes*node_point_read);           
    }


    point_counter = node_id*node_point_read + 1;
    points_read = 0; //starting at first line of n points in file
   
	float a;
	float b;

    //loop through the file to go to line
    while(points_read != point_counter && points_read != num_points && (infile >> a >> b)){
        points_read++;
    }

    int num_read = 1;

    while(num_read != node_point_read && (infile >> a >> b)){
        points.push_back(make_pair(a, b));   
        num_read++;
        points_read++;
    }    

    while(points_read != num_points){
        points_read++
    }
    
    num_read = 0;
    while(num_read < 4 && (infile >> a >> b)){
        centroids.push_back(make_pair(a,b));
    }
    

    //TODO: register data segment call
    
    

    for(auto point : points){

        float x1 = point.first;
        float y1 = point.second;
        
        float min_distance = FLT_MAX;
        float centroid;


        for(int i = 0; i < 4; i++){
    
            float x2 = centroids[i].first;
            float y2 = centroids[i].second;

            float dist = pow(x2 - x1, 2) + pow(y2 - y1, 2);
            dist = sqrt(dist);

            if(dist < min_distance){
                min_distance = dist;
                centroid = i;
            }
        }

        psu_mutex_lock(0);
        
        int size = (int) kmeans[centroid][0].first;
        kmeans[centroid][size+1] = make_pair(x1, y1);
        kmeans[centroid][0] = make_pair(((float) size + 1), 0.0f);
        
        psu_mutex_unlock(0);

    }
	
	return;


}

void map_kmeans(void *inpdata, void *opdata){

	kmeans_init(filename);
    //barrier
}






void kmeans_reducer(int centroid_id){

        

    float x;
    float y;


    for(int j = 0; j < kmeans[centroid_id][0].first; j++){

    //skipping over size
    if(j != 0){
                
        x = x + kmeans[centroid_id][j].first;
        y = y + kmeans[centroid_id][j].second;
    
        }

    }
    
    float x_res = x / kmeans[centroid_id][0].first;
    float y_res = y / kmeans[centroid_id][0].first;

    cout << "Centroid " << centroid_id + 1 << ": " << x_res << " " y_res << endl;

    centroids[centroid_id] = make_pair(x_res, y_res);

}


void reduce_kmeans(void *inpdata, void *opdata){

    

    for(int i = 0; i < 4; i++){
        if(node_id = i){
            kmeans_reducer(i);
        }
    }


    if(total_nodes < 4){
        if(node_id + 1 = total_nodes){
            for(int i = 4 - total_nodes; i <= 4; i++){
                kmeans_reducer(i);
            }    
        }
    }

    



}


int main(int argc, char *argv[]){

	if (argc != 4){
		cout << "Wrong number of args.\n";
		cout << "<kmeans> <input_file_name>\n";
		return 1;
	}

		
	filename = string(argv[1]);
    node_id = atoi(argv[2]);
    total_nodes = atoi(argv[3]);

    
    map_kmeans();
    reduce_kmeans();


}
