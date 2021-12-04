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
using directory::Empty;
using directory::RegisterRequest;


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
	  // TODO: Fix this later
	  for(int j = 0; j < num_nodes; j++){
		v1.push_back(0);
	  }

	  shared_ptr<mutex> ptr(new mutex());
	  table.push_back(make_tuple(v1, ptr, -1));
	}
  }
};

class DirectoryImpl final : public DirectoryService::Service {
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

