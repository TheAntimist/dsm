#include <string>
#include <fstream>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
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
using directory::NodeService;
using directory::PageRequest;
using directory::PageRequestAccess;
using directory::PageData;
using directory::Empty;
using directory::RegisterRequest;

#define INVALID_STATE 0
#define READ_STATE 1
#define RWRITE_STATE 2

class DataSegment {
public:
  string name;
  int num_pages;
  vector<tuple<vector<int>, shared_ptr<mutex>, int>> table;
  DataSegment() {
  }
  DataSegment(int _num_pages, int num_nodes, string _name) {
	num_pages = _num_pages;
	name = _name;
	for(int i = 0; i < num_pages; i++) {
	  vector<int> v1;
	  for(int j = 0; j < num_nodes; j++){
		  v1.push_back(READ_STATE);
	  }

	  shared_ptr<mutex> ptr(new mutex());
	  table.push_back(make_tuple(v1, ptr, -1));
	}
  }
};

class NodeClient {
  unique_ptr<NodeService::Stub> stub_;
  shared_ptr<Channel> channel;
public:
  NodeClient(shared_ptr<Channel> _channel) 
	: stub_(NodeService::NewStub(_channel)) {
  	channel = _channel;
  }

  NodeClient(const NodeClient& other) : stub_(NodeService::NewStub(other.channel)) {
	  channel = other.channel;
  }
  NodeClient& operator=(NodeClient other) {
	  swap(channel, other.channel);
	  stub_ = NodeService::NewStub(other.channel);
	  return *this;
  }

  void hello() {
    Empty req, reply;
    ClientContext context;
    context.set_wait_for_ready(true);
    cout << "[debug] Sending Hello\n";
    Status status = stub_->hello(&context, req, &reply);
  }
  bool invalidate_page(int page_num);
  bool grant_request_access(int page_num, bool is_write);
  PageData fetch_page(int page_num);
  PageData revoke_write_access(int page_num);
}

class DirectoryImpl final : public DirectoryService::Service {
  vector<NodeClient> nodes;
  bool is_initiated = false;
  int self;
  int num_nodes;
  mutex num_mutex;
  unordered_map<string, DataSegment> segments;
  condition_variable cv;


public:
  DirectoryImpl();
  void startServer();
  Status register_segment(ServerContext* context,
						  const RegisterRequest* req_obj,
						  Empty* reply) override;


  Status request_access(ServerContext* context,
                        const AccessRequest* req_obj,
                        Empty* reply) override;

  Status hello(ServerContext* context,
			   const Empty* reqObj,
			   Empty* reply) override;


};

