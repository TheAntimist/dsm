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

#define __USE_GNU
#include <ucontext.h>
#include <iostream>
#include <signal.h>
#include <cstdlib>
#include <malloc.h>
#include <errno.h>
#include <sys/mman.h>
#define PAGE_SIZE 4096

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
using directory::PageRequest;
using directory::PageData;
using directory::PageRequestAccess;
using directory::AccessReply;
using directory::RegisterRequest;
using directory::AccessRequest;


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
	cout << "[debug] Sending Hello\n";
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

class Node final : public NodeService::Service {
	DirectoryClient client;
	bool is_init = false;
	bool is_default = false;
	struct sigaction sig = { 0 };
	int num_pages = 0;
	void* start_addr;
	int self = -1;

public:
	Node() {
		
	}
	
	int get_page_num(void * addr);

	void invalidate_page(int page_num);

	void* get_page_addr(void *addr, int page_num);

	void * get_page_addr(void *addr);
	
	void request_access(void* addr, bool is_write);

	void sighandler(int sig, siginfo_t *info, void *ctx);

	void register_datasegment(void * psu_ds_start, size_t psu_ds_size);

	static Node* instance;
	static void wrap_signal_handler(int sig, siginfo_t *info, void *ctx) {
		instance->sighandler(sig, info, ctx);
	}
	
}
