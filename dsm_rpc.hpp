#ifndef DSM_RPC_H_
#define DSM_RPC_H_


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

class DirectoryClient {
  unique_ptr<DirectoryService::Stub> stub_;
  shared_ptr<Channel> channel;
public:
  DirectoryClient(shared_ptr<Channel> _channel) 
	: stub_(DirectoryService::NewStub(_channel)){
	channel = _channel;
  }

  DirectoryClient(const DirectoryClient& other) : stub_(DirectoryService::NewStub(other.channel)) {
	channel = other.channel;
  }
  DirectoryClient& operator=(DirectoryClient other) {
	swap(channel, other.channel);
	stub_ = DirectoryService::NewStub(other.channel);
	return *this;
  }

  void hello() {
	Empty req, reply;
	ClientContext context;
	context.set_wait_for_ready(true);
	Status status = stub_->hello(&context, req, &reply);
  }

  bool register_segment(string name, int num_pages) {
	RegisterRequest request;
	request.set_name(name);
	request.set_num_pages(num_pages);
	Empty empty;
	ClientContext context;
	Status status = stub_->reply(&context, request, &empty);
	
	return status.ok();
  }

};

class DataSegment {
public:
  string name;
  int num_pages;
  vector<tuple<vector<int>, mutex, int> table;
  DataSegment(int _num_pages, string _name) {
	num_pages = _num_pages;
	name = _name;
	for(int i = 0; i < num_pages; i++) {
	  vector<int> v1;
	  // TODO: Fix this later
	  for(int j = 0; j < 32; j++){
		v1.push_back(0);
	  }
	  mutex m;
	  table.push_back(make_tuple(v1, m, -1));
	}
  }
}

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
		segments[name] = segment;
		return Status::OK;
	  }
}

