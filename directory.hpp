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
#include <tuple>

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include "directory.grpc.pb.h"
#include "logger.h"

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
using directory::AccessRequest;
using directory::AccessReply;
using directory::LkRequest;
using directory::MRRequest;
using directory::MallocReply;

#define INVALID_STATE 0
#define READ_STATE 1
#define RWRITE_STATE 2

string localHostname() {
  char buffer[100];
  int ret;
  if ((ret = gethostname(buffer, sizeof(buffer))) == -1) {
    return "";
  }
  return string(buffer) + string(":8080");
}

class DataRow {
public:
  vector<int> access;
  shared_ptr<mutex> mut_lock;
  int state;
};

class DataSegment {
public:
  string name;
  int num_pages;
  vector<DataRow> table;
  DataSegment() {
  }

  DataSegment(int _num_pages, int num_nodes, string _name) {
    num_pages = _num_pages;
    name = _name;
    for(int i = 0; i < num_pages; i++) {
      DataRow row;
      vector<int> v1;
      for(int j = 0; j < num_nodes; j++) {
        v1.push_back(READ_STATE);
      }
      shared_ptr<mutex> ptr(new mutex());

      row.access = v1;
      row.mut_lock = ptr;
      row.state = READ_STATE;

      table.push_back(row);
    }
  }
  void lock(int page_num) {
    table[page_num].mut_lock->lock();
  }

  void unlock(int page_num) {
    table[page_num].mut_lock->unlock();
  }

  vector<int> get_access(int page_num) {
    return table[page_num].access;
  }

  void set_access(int page_num, vector<int> _access) {
    table[page_num].access = _access;
  }

  void set_state(int page_num, int state) {
    table[page_num].state = state;
  }

  int get_state(int page_num) {
    return table[page_num].state;
  }
};

class NodeClient {
  unique_ptr<NodeService::Stub> stub_;
  shared_ptr<Channel> channel;
  shared_ptr<Logger> logger;
  string local_host, receiver_host;
public:
  NodeClient(shared_ptr<Channel> _channel, string _local_host, string _receiver_host, shared_ptr<Logger> _logger) 
	: stub_(NodeService::NewStub(_channel)) {
  	channel = _channel;
    local_host = _local_host;
    receiver_host = _receiver_host;
    logger = _logger;
  }

  NodeClient(const NodeClient& other) : stub_(NodeService::NewStub(other.channel)) {
	  channel = other.channel;
    local_host = other.local_host;
	  receiver_host = other.receiver_host;
	  logger = other.logger;

  }
  NodeClient& operator=(NodeClient other) {
	  swap(channel, other.channel);
	  stub_ = NodeService::NewStub(other.channel);
    local_host = other.local_host;
	  receiver_host = other.receiver_host;
	  logger = other.logger;
	  return *this;
  }

  void hello() {
    Empty req, reply;
    ClientContext context;
    context.set_wait_for_ready(true);
    cout << "[debug] Sending Hello\n";
    logger->log(string_format("----RPC call from %s to %s for %s with arguments: [] ----",
                local_host.c_str(), receiver_host.c_str(), "hello"));
    Status status = stub_->hello(&context, req, &reply);
  }
  bool invalidate_page(int page_num, string name);
  bool grant_request_access(int page_num, bool is_write, string page_data, string name);
  PageData fetch_page(int page_num, string name);
  PageData revoke_write_access(int page_num, string name);
};

class DirectoryImpl final : public DirectoryService::Service {
  vector<NodeClient> nodes;
  bool is_initiated = false;
  int self;
  int num_nodes, pending_nodes;
  // Num Mutex for the pending_nodes. map_mutex for the map
  mutex num_mutex, map_mutex;
  unordered_map<string, DataSegment> segments;
  condition_variable cv;

  unordered_map<int, mutex> mutex_map;
  mutex barrier_mut;
  int barrier_total = 0;

  shared_ptr<Logger> logger;

public:
  DirectoryImpl();
  void startServer();
  Status register_segment(ServerContext* context,
						  const RegisterRequest* req_obj,
						  Empty* reply) override;

  Status register_malloc(ServerContext* context,
						  const RegisterRequest* req_obj,
						  MallocReply* reply) override;

  Status request_access(ServerContext* context,
                        const AccessRequest* req_obj,
                        AccessReply* reply) override;

  Status hello(ServerContext* context,
			   const Empty* reqObj,
			   Empty* reply) override;

  Status init_lock(ServerContext* context,
			   const LkRequest* reqObj,
			   Empty* reply) override;

  Status request_lock(ServerContext* context,
			   const LkRequest* reqObj,
			   Empty* reply) override;

  Status request_unlock(ServerContext* context,
			   const LkRequest* reqObj,
			   Empty* reply) override;

  Status mr_setup(ServerContext* context,
			   const MRRequest* reqObj,
			   Empty* reply) override;
  Status mr_barrier(ServerContext* context,
			   const Empty* reqObj,
			   Empty* reply) override;

};
