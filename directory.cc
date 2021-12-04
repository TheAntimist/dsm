#include "directory.hpp"

Status DirectoryImpl::register_segment(ServerContext* context,
						const RegisterRequest* req_obj,
						Empty* reply) override {
   
  string name = req_obj->name();
  {
	lock_guard<mutex> lk(num_mutex);
	num_nodes--;
	if (!is_initiated && segments.find(name) == segments.end()) {
	  DataSegment segment(req_obj->num_pages(), name);
	  segments.insert(make_pair(name, segment));
	  is_initiated = true;
	}
  }

  unique_lock<mutex> lk(num_mutex);
  if (num_nodes != 0) {
	cout << "[debug] Waiting for other nodes to finish registering.\n";
	cv.wait(lk, [=](){return num_nodes == 0;});
  } else {
	cout << "[debug] All registered. Resuming now..\n";
	cv.notify_all();
  }
  return Status::OK;
}



Status DirectoryImpl::request_access(ServerContext* context,
									 const AccessRequest* req_obj,
									 Empty* reply) {

  if(req_obj->is_write()){
	cout << "Read-Write access request from: " << req_obj->node_num() << endl;
        
	int page_num = req_obj->page_num();

	segments[req_obj->name()].table[page_num][1]->lock();
   
	DataSegment segment = segments[req_obj->name()];

	vector<int> table_data = segment.table[page_num][0];

	for(int i = 0; i < num_nodes; i++){
	  if(table_data[i] == 1 and i != req_obj->node_num()){
		string ip;
		string node_id = to_string(i);
		if(i < 10){
		  string node_id = "0" + node_id;
		}
		table_data[i] = 0;
	  }
	}
	table_data[req_obj->node_num()] = 1; 
        
	for(string ip: invalidate){
	  //TODO: to client RPC call to invalidate. Except for req_obj->node_num()
	}


	segments[req_obj->name()].table[page_num][2] = 2; //Setting state to RW state

	//TODO: Call to client RPC to ACK the change from our end.
	        
	segments[req_obj->name()].table = table_data;
	segments[req_obj->name()].table[page_num][1]->unlock();
         

  } else {
	cout << "[debug] Read access request from: " << req_obj->node_num()  << endl;
        
	segments[req_obj->name()].table[page_num][1]->lock();

	int request_page_num;
        
	if(segments[req_obj->name()].table[page_num][2] == 1){ //if page is Read Only

	} else { //if page is Read Write

	}

	  segments[req_obj->name()].table[page_num][2] = 1; //Setting state to RO state
        
	  segments[req_obj->name()].table[page_num][1]->unlock();
	}
  }
}

Status DirectoryImpl::hello(ServerContext* context,
			 const Empty* reqObj,
			 Empty* reply) override {
  cout << "[debug] Got Hello\n";
  return Status::OK;              	
}

void DirectoryImpl::startServer() {
  cout << "[debug] Starting server...\n";
  std::string server_address("0.0.0.0:8080");

  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  ServerBuilder builder;

  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(this);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  server->Wait();
}

DirectoryImpl::DirectoryImpl() {
  cout << "Initializing the directory node" << endl;

  ifstream infile("node_list.txt");
  string currentHost = localHostname();
  vector<string> hosts;
  while (infile >> host) {
	// TODO: RPCClient client(grpc::CreateChannel(host, grpc::InsecureChannelCredentials()));
	// Wait until ready
	//client.hello();
	hosts.push_back(host);
  }
  infile.close();
  hosts.pop_back();
  num_nodes = hosts.size();
  
  cout << "[debug] Directory node intialized with " << hosts.size()
	   << " nodes\n";
}


int main() {
  DirectoryImpl d;
  d.startServer();
  return 0;
}
