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
  bool isInited = false;
  int self;
  unordered_map<string, DataSegment> segments;


public:
  DirectoryImpl() {
 
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
	string name = req_obj->name();
	DataSegment segment(req_obj->num_pages(), name);
	segments.insert(make_pair(name, segment));
	return Status::OK;
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
