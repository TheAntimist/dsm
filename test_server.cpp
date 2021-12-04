#include <string>
#include <fstream>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <memory>
#include <thread>
#include <mutex>
#include <unistd.h>

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include "directory.grpc.pb.h"

using namespace std;

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using directory::DirectoryService;
using directory::Empty;
using directory::RegisterRequest;


class DataSegment {
public:
  string name;
  int num_pages;
  vector<tuple<vector<int>, shared_ptr<mutex>, int>> table;
  DataSegment() {
  }
  DataSegment(int _num_pages, string _name) {
	num_pages = _num_pages;
	name = _name;
	for(int i = 0; i < num_pages; i++) {
	  vector<int> v1;
	  // TODO: Fix this later
	  for(int j = 0; j < 32; j++){
		v1.push_back(0);
	  }

	  shared_ptr<mutex> ptr(new mutex());
	  table.push_back(make_tuple(v1, ptr, -1));
	}
  }
};

class DirectoryImpl final : public DirectoryService::Service {
  //vector<NodeClient> conns;
  bool is_initiated = false;
  int self;
  int num_nodes;
  mutex num_mutex;
  unordered_map<string, DataSegment> segments;


public:
  DirectoryImpl() {
    cout << "Initializing the directory node" << endl;
    this.num_nodes = 3;
  }
  void startServer() {
	cout << "[debug] Starting server...\n";
	std::string server_address("0.0.0.0:8080");

	grpc::EnableDefaultHealthCheckService(true);
	grpc::reflection::InitProtoReflectionServerBuilderPlugin();
	ServerBuilder builder;
	// Listen on the given address without any authentication mechanism.
	builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
	// Register "service" as the instance through which we'll communicate with
	// clients. In this case it corresponds to an *synchronous* service.
	builder.RegisterService(this);
	// Finally assemble the server.
	std::unique_ptr<Server> server(builder.BuildAndStart());
	std::cout << "Server listening on " << server_address << std::endl;

	// Wait for the server to shutdown. Note that some other thread must be
	// responsible for shutting down the server for this call to ever return.
	server->Wait();
  }
  Status register_segment(ServerContext* context,
						  const RegisterRequest* req_obj,
						  Empty* reply) override {
   
        //TODO: Possible that nodes call register segment multiple times -- Check
    this.num_mutex.lock();
    this.num_nodes--;
    this.num_mutex.unlock();
    if(!this.is_initiated){
    	string name = req_obj->name();
	    DataSegment segment(req_obj->num_pages(), name);
    	segments.insert(make_pair(name, segment));
    }
    while(this.num_nodes != 0){} //Wait till all nodes have been registered
	return Status::OK;
  }


  Status request_access(ServerContext* context,
                        const AccessRequest* req_obj,
                        Empty* reply) override {

    if(req_obj->is_write()){
        cout << "Read-Write access request from: " << req_obj->node_num() << endl;
        
        int page_num = req_obj->page_num();

        segments[req_obj->name()].table[page_num][1].lock();

        vector<string> invalidate;        
   
        DataSegment segment = segments[req_obj->name()];

        vector<int> table_data = segment.table[page_num][0];
   


        for(int i = 0; i < 32; i++){
            if(table_data[i] == 1 and i != req_obj->node_num()){
                string ip;
                string node_id = to_string(i);
                if(i < 10){
                    string node_id = "0" + node_id;
                }
                ip = "e5-cse-135-" + node_id + ".cse.psu.edu:8080";
                invalidate.push_back(ip);
                table_data[i] = 0;
            }
        }
        table_data[req_obj->node_num()] = 1; 
        
        for(string ip: invalidate){
            //TODO: to client RPC call to invalidate
        }


        segments[req_obj->name()].table[page_num][2] = 2; //Setting state to RW state

        //TODO: to client RPC call ACK
        
        segments[req_obj->name()].table = table_data;

        segments[req_obj->name()].table[page_num][1].unlock();
         

    }    
    else{
        cout << "Read access request from: " << req_obj->node_num()  << endl;
        
        segments[req_obj->name()].table[page_num][1].lock();

        int request_page_num;
        
        if(segments[req_obj->name()].table[page_num][2] == 1){ //if page is Read Only

            for(int i = 0; i


        }
        else{ //if page is Read Write



        }
        


        segments[req_obj->name()].table[page_num][2] = 1; //Setting state to RO state
        
        segments[req_obj->name()].table[page_num][1].unlock();
    }
  }

  Status hello(ServerContext* context,
  				 const Empty* reqObj,
                 Empty* reply) override {
    cout << "[debug] Got Hello\n";
	return Status::OK;              	
  }



};

int main() {
  DirectoryImpl d;
  d.startServer();
  return 0;
}
