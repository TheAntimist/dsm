#ifndef __NODE_H_
#define __NODE_H_

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
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/health_check_service_interface.h>

#include "directory.grpc.pb.h"

//#define __USE_GNU

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
using directory::NodeService;
using directory::LockReply;
using directory::LockRequest;
using directory::MRRequest;
using directory::LkRequest;

string localHostname();

class Lock {
public:
  int maxSeqNo;
  int highestSeqNo;
  bool requestingCS;
  mutex cvMutex;
  condition_variable cv;
  Lock() {
	maxSeqNo = highestSeqNo = 0;
	requestingCS = false;
  }

  // Defer until requestingCS is updated or notified.
  void waitUntilNotExecuting(int k, int nodeid, int self);

  // Guarded Updates
  void enterCS();

  void exitCS();
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
  LockReply request_lock(LockRequest request);
  bool reply_lock(LockRequest request);
};

class DirectoryClient {
  unique_ptr<DirectoryService::Stub> stub_;
  shared_ptr<Channel> channel;
public:
  DirectoryClient() {}
  DirectoryClient(shared_ptr<Channel> _channel) 
	: stub_(DirectoryService::NewStub(_channel)){
	channel = _channel;
  }

  DirectoryClient(const DirectoryClient& other) : stub_(DirectoryService::NewStub(other.channel)) {
	channel = other.channel;
  }
  DirectoryClient& operator=(DirectoryClient other) {
	//cout << "[debug] Channels: " << channel << " "<< other.channel << endl;
	channel = other.channel;
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


  AccessReply request_access(bool is_write, int page_num, 
                      string name, int node_num){
    AccessRequest request;
    request.set_name(name);
    request.set_page_num(page_num);
    request.set_is_write(is_write);
    request.set_node_num(node_num);
	AccessReply reply;
    ClientContext context;
    Status status = stub_->request_access(&context, request, &reply);
    return reply;
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

  void init_lock(int lockno) {
	LkRequest request;
	request.set_lockno(lockno);
    Empty empty;
	ClientContext context;
	Status status = stub_->init_lock(&context, request, &empty);
}

    void request_lock(int lockno) {
	LkRequest request;
	request.set_lockno(lockno);
    Empty empty;
	ClientContext context;
	Status status = stub_->request_lock(&context, request, &empty);
	}

  void request_unlock(int lockno) {
	LkRequest request;
	request.set_lockno(lockno);
    Empty empty;
	ClientContext context;
	Status status = stub_->request_unlock(&context, request, &empty);
}

  void mr_setup(int total) {
	MRRequest request;
	request.set_total(total);
    Empty empty;
	ClientContext context;
Status status = stub_->mr_setup(&context, request, &empty);
}

  void mr_barrier() {
	Empty empty, request;
	ClientContext context;
	Status status = stub_->mr_barrier(&context, request, &empty);
  }

};

void waitForRequest(NodeClient client, LockRequest reqObj);

void waitForReply(NodeClient client, LockRequest reqObj);

class Node final : public NodeService::Service {
	DirectoryClient client;
	bool is_init = false;
	bool is_default = false;
	struct sigaction sig;
	int num_pages = 0;
	void* start_addr;

	int self = -1;
	thread server_thread;

	// Lock related
	unordered_map<int, shared_ptr<Lock>> lockMap;
	vector<NodeClient> nodes;
	bool lock_init = false;

public:
	Node();
	~Node();
	
	bool is_inited() { return is_init; }
	void init();
	void startServer();
	int get_page_num(void * addr);

	Status invalidate_page(ServerContext* context,
                           const PageRequest* req_obj,
                           Empty* reply) override;
	Status grant_request_access(ServerContext* context,
                            const PageRequestAccess* req_obj, 
                            Empty* reply) override;
	Status revoke_write_access(ServerContext* context, 
                            const PageRequest* req_obj, 
                            PageData* reply) override;
	Status fetch_page(ServerContext* context, 
                    const PageRequest* req_obj, 
                    PageData* reply) override;

	void* get_page_base_addr(int page_num) {
		void* page_addr = (void *) (((char *) start_addr) + PAGE_SIZE*page_num);
		return page_addr;
	}

//	void* get_page_addr(void *addr, int page_num);

	void * get_page_addr(void *addr);
	
	void request_access(void* addr, bool is_write);

	void sighandler(int sig, siginfo_t *info, void *ctx);

	void register_datasegment(void * psu_ds_start, size_t psu_ds_size);

	// Ricart Agarwala
	void init_locks(vector<NodeClient> _nodes);
	bool is_lock_inited() {
		return lock_init;
	}
	void create_new_lock(int lockno) {
		// lockno is not present
		// May not be required
		client.init_lock(lockno);
	}
	void lock_enter_cs(int lockno);
	void lock_exit_cs(int lockno);
	Status request_lock(ServerContext* context,
  				 const LockRequest* reqObj,
                 LockReply* reply) override;
	Status reply_lock(ServerContext* context,
  				 const LockRequest* reqObj,
                 Empty* reply) override;
	
	void mr_setup(int total) {
		client.mr_setup(total);
	}

	void mr_barrier() {
		client.mr_barrier();
	}

	static Node instance;
	static void wrap_signal_handler(int sig, siginfo_t *info, void *ctx) {
		instance.sighandler(sig, info, ctx);
	}
};

void psu_dsm_register_datasegment(void * psu_ds_start, size_t psu_ds_size);

#endif
