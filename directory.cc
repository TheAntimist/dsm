#include "directory.hpp"

bool NodeClient::invalidate_page(int page_num, string name) {
    PageRequest req;
    req.set_page_num(page_num);
    req.set_name(name);

    Empty reply;
    ClientContext context;
    Status status = stub_->invalidate_page(&context, req, &reply);
    return status.ok();
}

bool NodeClient::grant_request_access(int page_num, bool is_write, string page_data, string name) {
    PageRequestAccess req;
    req.set_page_num(page_num);
    req.set_is_write(is_write);
    req.set_page_data(page_data);
    req.set_name(name);

    Empty reply;
    ClientContext context;
    Status status = stub_->grant_request_access(&context, req, &reply);
    return status.ok();
}

PageData NodeClient::fetch_page(int page_num, string name) {
    PageRequest req;
    req.set_page_num(page_num);
    req.set_name(name);

    PageData reply;
    ClientContext context;
    Status status = stub_->fetch_page(&context, req, &reply);
    return reply;
}

PageData NodeClient::revoke_write_access(int page_num, string name) {
    PageRequest req;
    req.set_page_num(page_num);
    req.set_name(name);

    PageData reply;
    ClientContext context;
    Status status = stub_->revoke_write_access(&context, req, &reply);
    return reply;
}

Status DirectoryImpl::init_lock(ServerContext* context,
			   const LkRequest* reqObj,
			   Empty* reply) {
    // This will construct in place.
    mutex_map[reqObj->lockno()];
    return Status::OK;
}


Status DirectoryImpl::request_lock(ServerContext* context,
        const LkRequest* reqObj,
        Empty* reply) {
    mutex_map[reqObj->lockno()].lock();
    return Status::OK;
}

Status DirectoryImpl::request_unlock(ServerContext* context,
        const LkRequest* reqObj,
        Empty* reply) {
    mutex_map[reqObj->lockno()].unlock();
    return Status::OK;
}

Status DirectoryImpl::mr_setup(ServerContext* context,
        const MRRequest* reqObj,
        Empty* reply) {
    lock_guard<mutex> lk(barrier_mut);
    if (barrier_total == 0) {
      barrier_total = reqObj->total();
    }
    return Status::OK;
}

Status DirectoryImpl::mr_barrier(ServerContext* context,
        const Empty* reqObj,
        Empty* reply) {
    barrier_mut.lock();
    barrier_total--;
    barrier_mut.unlock();

    while(barrier_total > 0);
    return Status::OK;
}

Status DirectoryImpl::register_segment(ServerContext* context,
						const RegisterRequest* req_obj,
						Empty* reply) {
  string name = req_obj->name();
  {
    lock_guard<mutex> lk(num_mutex), m_lk(map_mutex);
    pending_nodes--;
    if (!is_initiated && segments.find(name) == segments.end()) {
      DataSegment segment(req_obj->num_pages(), num_nodes, name);
      segments.insert(make_pair(name, segment));
      is_initiated = true;
    }
  }

  unique_lock<mutex> lk(num_mutex);
  if (pending_nodes != 0) {
	  cout << "[debug] Waiting for other nodes to finish registering.\n";
	  cv.wait(lk, [=](){return pending_nodes == 0;});
  } else {
	  cout << "[debug] All registered. Resuming now..\n";
	  cv.notify_all();
  }
  return Status::OK;
}

Status DirectoryImpl::register_malloc(ServerContext* context,
						const RegisterRequest* req_obj,
						MallocReply* reply) {
  string name = req_obj->name();
  lock_guard<mutex> m_lk(map_mutex);
  if (segments.find(name) == segments.end()) {
    int num_pages = req_obj->num_pages(), node_num = req_obj->node_num();
    DataSegment segment(num_pages, num_nodes, name);

    for(int i = 0; i < num_pages; i++) {
      vector<int> page = segment.get_access(i);
      for(int j = 0; j < num_nodes; j++) {
        if (j == node_num) {
          page[j] = 1;
        } else {
          page[j] = 0;
        }
      }
      segment.set_access(i, page);
      nodes[node_num].grant_request_access(i, false, string(), name);
    }
    segments.insert(make_pair(name, segment));
    // Grant Access
    reply->set_is_first(true);
  } else {
    reply->set_is_first(false);
    // Page remains invalidated
  }
  return Status::OK;
}

Status DirectoryImpl::request_access(ServerContext* context,
									 const AccessRequest* req_obj,
									 AccessReply* reply) {

  int page_num = req_obj->page_num();
  int node_num = req_obj->node_num();
  string name = req_obj->name();

  if(req_obj->is_write()){//requesting read-write access

            
      segments[name].lock(page_num);

      cout << "Read-Write access request from: " << node_num << " : with page_num:  " << page_num << endl;
      
      vector<int> table_data = segments[name].get_access(page_num);

      cout << "Going through table_data" << endl;
      string page;
      for(int i = 0; i < num_nodes; i++) {
        if(i != node_num){
          cout << "[debug] Invalidating node: " << i << endl;
          if(page.size() == 0 && table_data[i] == 1){
              cout << "Fetching page number for RW access: " << page_num << " from: " << i << endl;
              PageData page_d = nodes[i].fetch_page(page_num, name);
              page = page_d.page_data();
          }
          nodes[i].invalidate_page(page_num, name);
          table_data[i] = 0;
        }
      }
      table_data[node_num] = 1; //Explicitly give that node access

      segments[name].set_state(page_num, RWRITE_STATE); //Setting state to RW state

      cout << "Set the state" << endl;

      nodes[node_num].grant_request_access(page_num, true, page, name);

      cout << "Granted request acccess for node: " << node_num << endl;

      
              
      segments[name].set_access(page_num, table_data);
      segments[name].unlock(page_num);
        
      reply->set_page_num(page_num);
      reply->set_page_data(page);
   
  } else {//requesting read access

            
      segments[name].lock(page_num);

      cout << "[debug] Read access request from: " << node_num << " : with page_num:  " << page_num  << endl;

      vector<int> table_data = segments[name].get_access(page_num);

      string page; //TODO: check if the page data type is correct

      if(segments[name].get_state(page_num) == READ_STATE) { //if page is Read Only
            cout << "Page found to be in READ_STATE" << endl;
            for(int i = 0; i < num_nodes; i++){
                if(table_data[i] == 1){
                    cout << "Node: " << node_num << "is requesting page " << page_num << " from " << i << endl; 
                    PageData page_d = nodes[i].fetch_page(page_num, name);
                    page = page_d.page_data();
                    break;
                }
                
            }

      } else{ //if page is Read Write
            cout << "Page found to be in READ_WRITE_STATE" << endl;
            for(int i = 0; i < num_nodes; i++){
                if(table_data[i] == 1){
                    cout << "Node: " << node_num << "is requesting page" << page_num << " from " << i << endl;
                    cout << "Asking to revoke access from node: " << i << endl;
                    PageData page_d = nodes[i].revoke_write_access(page_num, name);
                    cout << "Received page data from node: " << i << endl;
                    page = page_d.page_data();
                    break;
                }
            }

      }

      segments[name].get_access(page_num)[node_num] = 1;	

      segments[name].set_state(page_num, READ_STATE); //Setting state to RO state
          
      segments[name].unlock(page_num);

      reply->set_page_num(page_num);
      reply->set_page_data(page);
        
  }
    
    cout << "Returning back to signal handler to: " <<  node_num <<endl;

    return Status::OK;

}

Status DirectoryImpl::hello(ServerContext* context,
			 const Empty* reqObj,
			 Empty* reply) {
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
  string currentHost = localHostname(), host;
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
  pending_nodes = num_nodes = hosts.size();
  
  cout << "[debug] Directory node intialized with " << hosts.size()
	   << " nodes\n";
}


int main() {
  DirectoryImpl d;
  d.startServer();
  return 0;
}
