#ifndef NODE_H_
#define NODE_H_

#include <thread>
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

#define PORT 8080

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include "rpc.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using rpcservice::RPCService;
using rpcservice::Empty;
using rpcservice::LockRequest;
using rpcservice::LockReply;

using namespace std;

// TODO:
// - Reply Message handling

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

class RPCClient {
  unique_ptr<RPCService::Stub> stub_;
  shared_ptr<Channel> channel;
public:
  RPCClient(const RPCClient& other) : stub_(RPCService::NewStub(other.channel)) {
	channel = other.channel;
  }
  RPCClient(shared_ptr<Channel> _channel)
	: stub_(RPCService::NewStub(_channel)) {
	channel = _channel;
  }

  LockReply request(LockRequest request) {
	LockReply reply;
	ClientContext context;
	Status status = stub_->request(&context, request, &reply);

    // Act upon its status.
    if (!status.ok()) {
      cout << status.error_code() << ": " << status.error_message()
           << endl;
    }
    return reply;
  }

  void reply(LockRequest request) {
	Empty empty;	
	ClientContext context;
	Status status = stub_->reply(&context, request, &empty);
  }

  void hello() {
	Empty req, reply;
	ClientContext context;
	context.set_wait_for_ready(true);
	Status status = stub_->hello(&context, req, &reply);
  }

  RPCClient& operator=(RPCClient other) {
	swap(channel, other.channel);
	stub_ = RPCService::NewStub(other.channel);
	return *this;
  }
};


void waitForRequest(RPCClient client, LockRequest reqObj);

void waitForReply(RPCClient client, LockRequest reqObj);

class DistLockHandler final : public RPCService::Service {
	vector<RPCClient> conns;
	unordered_map<int, shared_ptr<Lock>> lockMap;
	bool isInited = false;
	int self;

public:
	// General Utils
  DistLockHandler() {
    thread t(&DistLockHandler::startServer, this);
    t.detach();
  }
  
  void init(int _self, vector<RPCClient> _conns) {
  	self = _self;
  	conns = _conns;
  	isInited = true;
  }

  // Server
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

  Status request(ServerContext* context,
  				 const LockRequest* reqObj,
                 LockReply* reply) override;

  Status reply(ServerContext* context,
  				 const LockRequest* reqObj,
                 Empty* reply) override;

  Status hello(ServerContext* context,
  				 const Empty* reqObj,
                 Empty* reply) override {
    cout << "[debug] Got Hello\n";
	return Status::OK;              	
  }
  

  bool hasInitiatialized() {
    return isInited;
  }
  void createNewLock(int lockno) {
    // lockno is not present
	// May not be required
	shared_ptr<Lock> s(new Lock());
	lockMap[lockno] = s;
  }
  void enterCriticalSection(int lockno);
  void exitCriticalSection(int lockno);

	// Static Instance
	static DistLockHandler instance;

};

void psu_init_lock(int lockno);
void psu_mutex_lock(int lockno);
void psu_mutex_unlock(int lockno);

#endif
