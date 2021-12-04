#include "ricart.hpp"

// TODO:


void waitForRequest(RPCClient client, LockRequest reqObj) {
	client.request(reqObj);
}

void waitForReply(RPCClient client, LockRequest reqObj) {
	client.reply(reqObj);
}

string localHostname() {
  char buffer[100];
  int ret;
  if ((ret = gethostname(buffer, sizeof(buffer))) == -1) {
    return "";
  }
  return string(buffer) + string(":8080");
}

void Lock::enterCS() {
	lock_guard<mutex> lk(cvMutex);
	requestingCS = true;
	maxSeqNo = highestSeqNo + 1;
}

void Lock::exitCS() {
	{
		lock_guard<mutex> lk(cvMutex);
		requestingCS = false;
	}
	cv.notify_all();
}


void Lock::waitUntilNotExecuting(int k, int nodeid, int self) {
	// Acquire Lock
	unique_lock<mutex> lk(cvMutex);
	if (requestingCS &&
		((k > maxSeqNo) ||
		(k == maxSeqNo && nodeid > self))) {
	  cout << "[debug] Waiting on the Condition Variable.\n";
		// TODO: Test this
		cv.wait(lk, [=](){return !requestingCS;});
	}
	// Unlock on return
}

Status DistLockHandler::request(ServerContext* context,
			const LockRequest* reqObj, LockReply* reply) {
	auto lock = lockMap[reqObj->lockno()];
	int ourSeqNo = lock->maxSeqNo,
	k = reqObj->seqno();

	cout << "[debug] Received Request from: " << reqObj->nodeid() 
		 << endl;
	if (k > lock->highestSeqNo) {
		lock->highestSeqNo = k;
	}

	// Defer until we are done with the CS
	lock->waitUntilNotExecuting(k, reqObj->nodeid(), self);
	cout << "[debug] Providing access to the CS for node: "
		<< reqObj->nodeid() << endl;
	// Give up access to the Critical Section
	reply->set_lockno(reqObj->lockno());
	reply->set_seqno(ourSeqNo);
	reply->set_nodeid(self);

	return Status::OK;
}


Status DistLockHandler::reply(ServerContext* context,
			const LockRequest* reqObj,
			Empty* reply) {
	cout << "[debug] Received Reply from: " << reqObj->nodeid() << endl;
	// Do nothing?
	return Status::OK;			
}

void DistLockHandler::enterCriticalSection(int lockno) {
	cout << "[debug] Sending Request to all nodes\n";
	// Send a request to all conns.
	auto lock = lockMap[lockno];
	lock->enterCS();

	// Build request for GRPC
	LockRequest request;
	request.set_lockno(lockno);
	request.set_seqno(lock->maxSeqNo);
	request.set_nodeid(self);

	std::vector<std::thread> threads;
	for(auto client : conns) {
		threads.emplace_back(thread(waitForRequest, client, request));
	}

	//wait for the replies
	for (auto& th : threads)
		th.join();

	cout << "[debug] Replied received from all. Entering CS.\n";
 }

void DistLockHandler::exitCriticalSection(int lockno) {
    auto lock = lockMap[lockno];
    // Update Lock and Notify all waiting requests
	lock->exitCS();

	cout << "[debug] Exited CS and sending Replies.\n";
	// Build request for GRPC
	LockRequest request;
	request.set_lockno(lockno);
	request.set_seqno(lock->maxSeqNo);
	request.set_nodeid(self);

	// Threads to handle replies. No need to wait.
	for(auto client : conns) {
		thread t(waitForReply, client, request);
		t.detach();
	}
}

void psu_init_lock(int lockno) {
 
	DistLockHandler::instance.createNewLock(lockno);

	if (DistLockHandler::instance.hasInitiatialized()) return;

	ifstream infile("node_list.txt");

	string currentHost = localHostname();

	string host;
	int nodeNo, curNode;

	vector<RPCClient> conns;
	while (infile >> host >> nodeNo) {
	  cout << "[debug] Host - " << host <<  " | node id - " << nodeNo << endl;
	  if (currentHost == host) {
	    curNode = nodeNo;
	    continue;
	  }
	  RPCClient client(grpc::CreateChannel(host, grpc::InsecureChannelCredentials()));
	  // Wait until ready
	  client.hello();
	  
	  // Note: currentHost contains port as well
	  conns.push_back(client);
	}
	infile.close();
	cout << "[debug] Initialized lock " << lockno
		 << " with node id: " << curNode << endl;
	DistLockHandler::instance.init(curNode, conns);

}


void psu_mutex_lock(int lockno) {
  	DistLockHandler::instance.enterCriticalSection(lockno);
}

void psu_mutex_unlock(int lockno) {
	DistLockHandler::instance.exitCriticalSection(lockno);
}

DistLockHandler DistLockHandler::instance;
