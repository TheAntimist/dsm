#include <string>
#include <fstream>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <memory>
#include <thread>
#include <mutex>
#include <unistd.h>

#include <grpcpp/grpcpp.h>

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
	cout << "Sending Hello\n";
	Status status = stub_->hello(&context, req, &reply);
  }


  bool request_access(bool is_write, int page_num, 
                      string name, int node_num){
    AccessRequest request;
    request.set_name(name);
    request.set_page_num(page_num);
    request.set_is_write(is_write);
    request.set_node_num(node_num);
    Empty empty;
    ClientContext context;
    Status status = stub_->request_access(&context, request, &empty);
    return status.ok();   
  }

  bool register_segment(string name, int num_pages) {
	RegisterRequest request;
	request.set_name(name);
	request.set_num_pages(num_pages);
	Empty empty;
	ClientContext context;
	Status status = stub_->register_segment(&context, request, &empty);
	
	return status.ok();
  }

};

int main() {
  DirectoryClient client(grpc::CreateChannel("e5-cse-135-07.cse.psu.edu:8080", grpc::InsecureChannelCredentials()));
  // Wait until ready
  client.hello();
  cout << client.register_segment("default", 2) << endl;

  return 0;
}
