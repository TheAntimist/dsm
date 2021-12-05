#include "directory.hpp"



bool NodeClient::invalidate_page(int page_num) {
    PageRequest req;
    req->set_page_num(page_num);

    Empty reply;
    ClientContext context;
    Status status = stub_->invalidate_page(&context, req, &reply);
    return status.ok();
}

bool NodeClient::grant_request_access(int page_num, bool is_write) {
    PageRequestAccess req;
    req->set_page_num(page_num);
    req->set_is_write(is_write);

    Empty reply;
    ClientContext context;
    Status status = stub_->grant_request_access(&context, req, &reply);
    return status.ok();
}

PageData NodeClient::fetch_page(int page_num) {
    PageRequest req;
    req->set_page_num(page_num);

    PageData reply;
    ClientContext context;
    Status status = stub_->fetch_page(&context, req, &reply);
    return reply;
}

PageData NodeClient::revoke_write_access(int page_num) {
    PageRequest req;
    req->set_page_num(page_num);

    PageData reply;
    ClientContext context;
    Status status = stub_->revoke_write_access(&context, req, &reply);
    return reply;
}

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
									 AccessReply* reply) {

  int page_num = req_obj->page_num();

  if(req_obj->is_write()){//requesting read-write access

	cout << "Read-Write access request from: " << req_obj->node_num() << endl;
        
	segments[req_obj->name()].table[page_num][1]->lock();
   
	vector<int> table_data = segments[req_obj->name()].table[page_num][0];

	for(int i = 0; i < num_nodes; i++){
	  if(table_data[i] == 1 and i != req_obj->node_num()){
      nodes[i].invalidate_page(page_num);
      table_data[i] = 0;
    }
	}
	table_data[req_obj->node_num()] = 1; //Explicitly give that node access
        
	segments[req_obj->name()].table[page_num][2] = RWRITE_STATE; //Setting state to RW state

	nodes[req_obj->node_num()].grant_request_access(page_num, true);
	        
	segments[req_obj->name()].table = table_data;
	segments[req_obj->name()].table[page_num][1]->unlock();
    
  reply->set_page_num(page_num);
   
  } else {//requesting read access

	cout << "[debug] Read access request from: " << req_obj->node_num()  << endl;
        
	segments[req_obj->name()].table[page_num][1]->lock();

	int requesting_node_num;
    
  vector<int> table_data = segments[req_obj->name()].table[page_num][0];

  string page; //TODO: check if the page data type is correct

	if(segments[req_obj->name()].table[page_num][2] == READ_STATE){ //if page is Read Only
    
        for(int i = 0; i < num_nodes; i++){
            if(table_data[i] == 1){
            
                PageData page_d = nodes[i].fetch_page(page_num);
                page = page_d->page_data();
                break;
            }
            
        }

	} else{ //if page is Read Write

        for(int i = 0; i < num_nodes; i++){
            if(table_data[i] == 1){                
                //TODO: RPC call to client/fetches page num's page data

                page = revoke_write_access(requesting_node_num, page_num);

                break;
            }
        }

	}

  segments[req_obj->name()].table[page_num][0][req_obj->node_num()] = 1;	

  segments[req_obj->name()].table[page_num][2] = READ_STATE; //Setting state to RO state
      
  segments[req_obj->name()].table[page_num][1]->unlock();

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
    hosts.push_back(host);
  }
  infile.close();
  // Remove yourself
  hosts.pop_back();
  for(string host : hosts) {
    NodeClient client(grpc::CreateChannel(host, grpc::InsecureChannelCredentials()));
    // Wait until ready
    client.hello();
    nodes.push_back(client);
  }
  num_nodes = hosts.size();
  
  cout << "[debug] Directory node intialized with " << hosts.size()
	   << " nodes\n";
}


int main() {
  DirectoryImpl d;
  d.startServer();
  return 0;
}
