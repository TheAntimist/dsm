#include "directory.hpp"

#define INVALID_STATE 0
#define READ_STATE 1
#define RWRITE_STATE 2



Status DirectoryImpl::register_segment(ServerContext* context,
						const RegisterRequest* req_obj,
						Empty* reply) override {
  
  //TODO: initalize the table to have read-only for that node
 
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
									 AccessReply* reply) {

  int page_num = req_obj->page_num();

  if(req_obj->is_write()){//requesting read-write access

	cout << "Read-Write access request from: " << req_obj->node_num() << endl;
        
	segments[req_obj->name()].table[page_num][1]->lock();
   
	vector<int> table_data = segments[req_obj->name()].table[page_num][0];

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


	segments[req_obj->name()].table[page_num][2] = RWRITE_STATE; //Setting state to RW state

	//TODO: Call to client RPC to ACK the change from our end.
	        
	segments[req_obj->name()].table = table_data;
	segments[req_obj->name()].table[page_num][1]->unlock();
    

    reply->set_is_success(true);
    reply->set_page_num(page_num);
   
    //TODO: return correct message [success status] -- Check if above is correct??? ^^

  } else {//requesting read access

	cout << "[debug] Read access request from: " << req_obj->node_num()  << endl;
        
	segments[req_obj->name()].table[page_num][1]->lock();

	int requesting_node_num;
    
    vector<int> table_data = segments[req_obj->name()].table[page_num][0];

    string page; //TODO: check if the page data type is correct

	if(segments[req_obj->name()].table[page_num][2] == READ_STATE){ //if page is Read Only
    
        for(int i = 0; i < num_nodes; i++){
            if(table_data[i] == 1){
                requesting_node_num = i;

                //TODO: RPC call to client/fetches page num's page data
                page = fetch_page(requesting_node_num, page_num);

                break;
            }
            
        }

	}
    else{ //if page is Read Write

        for(int i = 0; i < num_nodes; i++){
            if(table_data[i] == 1){
                requesting_node_num = i;
                
                //TODO: RPC call to client/fetches page num's page data
                page = revoke_write_access(requesting_node_num, page_num);

                break;
            }
        }

	}

    segments[req_obj->name()].table[page_num][0][req_obj->node_num()] = 1;	

    segments[req_obj->name()].table[page_num][2] = READ_STATE; //Setting state to RO state
        
	segments[req_obj->name()].table[page_num][1]->unlock();

    reply->set_is_success(true);
    reply->set_page_num(page_num);
    reply->set_page_data(page);
    
    //TODO: return correct message [return the page contents]
    }
    
    return Status::OK;

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
