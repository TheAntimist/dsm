#include "d_node.h"
#include <map>


map<string, D_mem> segs;

void create_d_mem(string name, int num_pages){
    
    if(segs.find(name) != segs.end()){
        return;
    }
    D_mem d_mem = new D_mem(num_pages, name);
    segs.insert(name, d_mem);
}



void grant_read_only(int page_num, D_mem d_mem, int node_id){
    d_mem[page_num][0][node_id] = 1;
}

void invalidate_others(int page_num, D_mem d_mem){

    

    //list of nodes to invalidate
    vector<int> invalidate;

    for(int i = 0; i < 31; i++){
        //has access
        if(d_mem[page_num][0][i] == 1){
            invalidate.push_back(i);
        }
    }

    for(int i = 0; i < invalidate.size(); i++){
        //RPC call to invalidate other nodes invalidate[i]
    }

    return;
}

void d_node_init(){


}

int main(){

    d_node_init();
    
    
}
