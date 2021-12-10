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
#include "logger.h"

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
using directory::MallocReply;

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
  LockReply request_lock(LockRequest request);
  bool reply_lock(LockRequest request);
};

class DirectoryClient {
  unique_ptr<DirectoryService::Stub> stub_;
  shared_ptr<Channel> channel;
  string local_host, receiver_host;
  shared_ptr<Logger> logger;
public:
  DirectoryClient() {}
  DirectoryClient(shared_ptr<Channel> _channel, string _local_host, string _receiver_host, shared_ptr<Logger> _logger) 
	: stub_(DirectoryService::NewStub(_channel)) {
	channel = _channel;
	logger = _logger;
	local_host = _local_host;
	receiver_host = _receiver_host;
  }

  DirectoryClient(const DirectoryClient& other) : stub_(DirectoryService::NewStub(other.channel)) {
	channel = other.channel;
	local_host = other.local_host;
	receiver_host = other.receiver_host;
	logger = other.logger;
  }
  DirectoryClient& operator=(DirectoryClient other) {
	//cout << "[debug] Channels: " << channel << " "<< other.channel << endl;
	channel = other.channel;
	local_host = other.local_host;
	receiver_host = other.receiver_host;
	logger = other.logger;  
	stub_ = DirectoryService::NewStub(other.channel);
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


  AccessReply request_access(bool is_write, int page_num, 
                      string name, int node_num){
    AccessRequest request;
    request.set_name(name);
    request.set_page_num(page_num);
    request.set_is_write(is_write);
    request.set_node_num(node_num);
	AccessReply reply;
    ClientContext context;
	logger->log(string_format("----RPC call from %s to %s for %s with arguments: [%d, %d, %s, %d] ----",
							  local_host.c_str(), receiver_host.c_str(), "request_access", is_write, page_num, name.c_str(), node_num));
    Status status = stub_->request_access(&context, request, &reply);
    return reply;
  }

  bool register_segment(string name, int num_pages) {
	RegisterRequest request;
	request.set_name(name);
	request.set_num_pages(num_pages);
	Empty empty;
	ClientContext context;
	logger->log(string_format("----RPC call from %s to %s for %s with arguments: [%s, %d] ----",
							  local_host.c_str(), receiver_host.c_str(), "register_segment", name.c_str(), num_pages));
	Status status = stub_->register_segment(&context, request, &empty);
	
	return status.ok();
  }

  bool register_malloc(string name, int node_num, int num_pages) {
	RegisterRequest request;
	request.set_name(name);
	request.set_num_pages(num_pages);
	request.set_node_num(node_num);
	MallocReply reply;
	ClientContext context;
	logger->log(string_format("----RPC call from %s to %s for %s with arguments: [%s, %d, %d] ----",
							  local_host.c_str(), receiver_host.c_str(), "register_malloc", name.c_str(), node_num, num_pages));
	Status status = stub_->register_malloc(&context, request, &reply);
	
	return reply.is_first();
  }

  void init_lock(int lockno) {
	LkRequest request;
	request.set_lockno(lockno);
    Empty empty;
	ClientContext context;
	logger->log(string_format("----RPC call from %s to %s for %s with arguments: [%d] ----",
							  local_host.c_str(), receiver_host.c_str(), "init_lock", lockno));
	Status status = stub_->init_lock(&context, request, &empty);
}

    void request_lock(int lockno) {
	LkRequest request;
	request.set_lockno(lockno);
    Empty empty;
	ClientContext context;
	logger->log(string_format("----RPC call from %s to %s for %s with arguments: [%d] ----",
							  local_host.c_str(), receiver_host.c_str(), "request_lock", lockno));
	Status status = stub_->request_lock(&context, request, &empty);
	}

  void request_unlock(int lockno) {
	LkRequest request;
	request.set_lockno(lockno);
    Empty empty;
	ClientContext context;
	logger->log(string_format("----RPC call from %s to %s for %s with arguments: [%d] ----",
							  local_host.c_str(), receiver_host.c_str(), "request_unlock", lockno));
	Status status = stub_->request_unlock(&context, request, &empty);
}

  void mr_setup(int total) {
	MRRequest request;
	request.set_total(total);
    Empty empty;
	ClientContext context;
	logger->log(string_format("----RPC call from %s to %s for %s with arguments: [%d] ----",
							  local_host.c_str(), receiver_host.c_str(), "mr_setup", total));
	Status status = stub_->mr_setup(&context, request, &empty);
}

  void mr_barrier() {
	Empty empty, request;
	ClientContext context;
	logger->log(string_format("----RPC call from %s to %s for %s with arguments: [] ----",
                local_host.c_str(), receiver_host.c_str(), "mr_barrier"));
	Status status = stub_->mr_barrier(&context, request, &empty);
  }

};

// Represents memory for [start, end)
class HeapMemory {
public:
	void* start_addr;
	void* end_addr;
	int num_pages;

	HeapMemory() {}
	HeapMemory(void * _start_addr,
				void * _end_addr,
				int _num_pages) {
		start_addr = _start_addr;
		end_addr = _end_addr;
		num_pages = _num_pages;
	}
	HeapMemory& operator=(HeapMemory other) {
		start_addr = other.start_addr;
		end_addr = other.end_addr;
		num_pages = other.num_pages;
		return *this;
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
	// Data Segment
	void* start_addr;
	void* end_addr;

	int self = -1;
	thread server_thread;

	// Malloc
	unordered_map<string, HeapMemory> malloc_map;

	// Lock related
	unordered_map<int, shared_ptr<Lock>> lockMap;
	vector<NodeClient> nodes;
	bool lock_init = false;
	shared_ptr<Logger> logger;

public:
	Node();
	~Node();
	
	bool is_inited() { return is_init; }
	void init();
	void startServer();

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

	void* get_page_base_addr(int page_num, void * st) {
		return (void *) (((char *) st) + PAGE_SIZE*page_num);
	}

	void* get_page_base_addr(int page_num, string name) {
		void * st = start_addr;
		if (name != "default") {
			st = malloc_map[name].start_addr;
		}
		return get_page_base_addr(page_num, st);
	}
	int get_page_num(void * addr, string segment);

	string get_addr_segment(void *addr);

//	void* get_page_addr(void *addr, int page_num);

	void * get_page_addr(void *addr, string name);
	
	void request_access(void* addr, bool is_write);

	void sighandler(int sig, siginfo_t *info, void *ctx);

	void register_datasegment(void * psu_ds_start, size_t psu_ds_size);
	void *register_malloc(char * name, size_t size);

	// Ricart Agarwala
	void init_locks(vector<NodeClient> _nodes);
	bool is_lock_inited() {
		return lock_init;
	}
	void create_new_lock(int lockno) {
		// lockno is not presen
		// May not be required

//		client.init_lock(lockno);
        shared_ptr<Lock> s(new Lock());
        lockMap[lockno] = s;
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
	shared_ptr<Logger> get_logger() {
	  return logger;
	}

	static Node instance;
	static void wrap_signal_handler(int sig, siginfo_t *info, void *ctx) {
		instance.sighandler(sig, info, ctx);
	}
};

void * psu_dsm_malloc(char *name, size_t size);
void psu_dsm_register_datasegment(void * psu_ds_start, size_t psu_ds_size);

#endif
