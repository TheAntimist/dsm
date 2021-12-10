#include "psu_dsm_system.h"

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
		cv.wait(lk, [=](){return !requestingCS;});
	}
	// Unlock on return
}

LockReply NodeClient::request_lock(LockRequest request) {
	LockReply reply;
	ClientContext context;
	Status status = stub_->request_lock(&context, request, &reply);
    logger->log(string_format("----RPC call from %s to %s for %s with arguments: [%d, %d, %d] ----",
							  local_host.c_str(), receiver_host.c_str(), "request_lock", request.lockno(), request.seqno(), request.nodeid()));

    // Act upon its status.
    if (!status.ok()) {
      cout << status.error_code() << ": " << status.error_message()
           << endl;
    }
    return reply;
  }

bool NodeClient::reply_lock(LockRequest request) {
	Empty empty;	
	ClientContext context;
    logger->log(string_format("----RPC call from %s to %s for %s with arguments [%d, %d, %d] ----",
							  local_host.c_str(), receiver_host.c_str(), "reply_lock", request.lockno(), request.seqno(), request.nodeid()));
	Status status = stub_->reply_lock(&context, request, &empty);
    return status.ok();
}

void waitForRequest(NodeClient client, LockRequest reqObj) {
	client.request_lock(reqObj);
}

void waitForReply(NodeClient client, LockRequest reqObj) {
	client.reply_lock(reqObj);
}

void Node::init_locks(vector<NodeClient> _nodes) {
    nodes = _nodes;
}

void Node::lock_enter_cs(int lockno) {
    cout << "[debug] Sending Enter Request to directory\n";
//	client.request_lock(lockno);

    cout << "[debug] Sending Request to all nodes\n";

    auto lock = lockMap[lockno];
    lock->enterCS();

    LockRequest request;
    request.set_lockno(lockno);
    request.set_seqno(lock->maxSeqNo);
    request.set_nodeid(self);

    vector<thread> threads;
    for(auto client : nodes){
        threads.emplace_back(thread(waitForRequest, client, request));
    }

    //wait for the replies
    for (auto& th : threads)
        th.join();

    cout << "[debug] Reply received from all. Entering CS. \n";

    //cout << "[debug] Entering CS.\n";
}

void Node::lock_exit_cs(int lockno) {
//    cout << "[debug] Sending Exit Request to directory\n";
//	client.request_unlock(lockno);
//	cout << "[debug] Exiting CS.\n";
    auto lock = lockMap[lockno];
    lock->exitCS();

    cout << "[debug] Exited CS and sending Replies. \n";
    // Build request for GRPC
    LockRequest request;
    request.set_lockno(lockno);
    request.set_seqno(lock->maxSeqNo);
    request.set_nodeid(self);

    //Threads to handle replies. No need to wait.
    for(auto client : nodes){
        thread t(waitForReply, client, request);
        t.join();
    }
}

Status Node::request_lock(ServerContext* context,
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

Status Node::reply_lock(ServerContext* context,
			const LockRequest* reqObj,
			Empty* reply) {
	cout << "[debug] Received Reply from: " << reqObj->nodeid() << endl;
	// Do nothing, since we just wait for responses.
	return Status::OK;
}

Node Node::instance;

Node::Node() : logger(new Logger("node")) {
    memset(&sig, 0, sizeof(sig));

    thread t(&Node::startServer, this);
    server_thread = move(t);
    
    string actualHost = localHostname();
    ifstream infile("node_list.txt");
    string lastHost;
    int idx = -1;
    while (infile >> lastHost) {
        idx += 1;
        if (actualHost == lastHost) {
	        self = idx;
	        continue;
	  }
    }
    infile.close();
    cout << "[debug] Directory Host : " << lastHost << endl;
    DirectoryClient _client(grpc::CreateChannel(lastHost,
                            grpc::InsecureChannelCredentials()),
                            actualHost, 
                            lastHost,
                            logger);
    client = _client;
    // Wait for Directory connection.
    client.hello();
    cout << "[debug] Initialized Node with node id: " << self << endl;
}

Node::~Node() {
	cout << "[info] Application ended. Blocking with server thread.\n";
	server_thread.join();
}

void Node::startServer() {
  cout << "[debug] Starting server...\n";
  std::string server_address("0.0.0.0:8080");

  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  ServerBuilder builder;

  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(this);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  server->Wait();
}

void Node::init() {
    sig.sa_flags = SA_SIGINFO;
    sig.sa_sigaction = &Node::wrap_signal_handler;
    if (sigaction(SIGSEGV, &sig, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    is_init = true;
}

string Node::get_addr_segment(void *addr) {
    if (start_addr > 0 && start_addr <= addr && addr < end_addr) {
        return "default";
    }

    for (auto const& x : malloc_map) {
        string segment = x.first;
        HeapMemory mem = x.second;
        if (mem.start_addr <= addr && addr < mem.end_addr) {
            return segment;
        }
    }

    // Shouldn't ever reach here
    return "default";
}

int Node::get_page_num(void * addr, string name) {
    void * start = start_addr;
    if (name != "default") {
        start = malloc_map[name].start_addr;
    }
    
    int page_num = ((int) ((((char*) addr) - ((char*) start)) / PAGE_SIZE));

    if(page_num < 0){
        cout << "Error!" << endl;
        printf("Given addr: %x", addr);
        printf("Start addr: %x", start);
    }

    return page_num;
}


void * Node::get_page_addr(void *addr, string name) {
    void *start = start_addr;
    if (name != "default") {
        start = malloc_map[name].start_addr;
    }

    int page_num = ((int) ((((char *) addr) - ((char  *) start)) / PAGE_SIZE));

    return (void *)( ((char*) start) + PAGE_SIZE*page_num);
}

void Node::sighandler(int sig, siginfo_t *info, void *ctx){
	  
    cout << "[debug] Sighandler called\n";

    if((((ucontext_t *)ctx)->uc_mcontext.gregs[REG_ERR]) & 0x2){
        cout << "[debug] write fault\n";
        string segment = get_addr_segment(info->si_addr);
        int page_num = get_page_num(info->si_addr, segment);

        cout << "Requesting page number: " << page_num << endl;

        AccessReply reply = client.request_access(true, page_num, segment, self);

        return;
    } else {
        cout << "[debug] read fault\n";

        string segment = get_addr_segment(info->si_addr);
        int page_num = get_page_num(info->si_addr, segment);

        cout << "Requesting page number: " << page_num << endl;

        AccessReply reply = client.request_access(false, page_num, segment, self);
       
         
        mprotect(get_page_addr(info->si_addr, segment), PAGE_SIZE, PROT_WRITE);
        cout << "Changing page protecting to write only" << endl;   
        const char* p_data = reply.page_data().c_str();
        //cout << "Did the p_data conversion" << endl;
        void * base_addr = get_page_addr(info->si_addr, segment);
        printf("Base address of the page is: %x\n", base_addr);
        memcpy(base_addr, p_data, PAGE_SIZE);
        cout << "Memcopying to the p_data" << endl;
        mprotect(get_page_addr(info->si_addr, segment), PAGE_SIZE, PROT_READ);
        cout << "Changed permission to read only" << endl;
        return;
    }
}


Status Node::invalidate_page(ServerContext* context,
                           const PageRequest* req_obj,
                           Empty* reply) {
    string name = req_obj->name();
    if (name != "default" && malloc_map.find(name) == malloc_map.end()) {
        // Return if we do not have the memory name 
        return Status::OK;
    }

    int page_num = req_obj->page_num();
    
    void * page_addr = get_page_base_addr(page_num, name);
    mprotect(page_addr, PAGE_SIZE, PROT_NONE);

    cout << "Invalidating page num: " << req_obj->page_num() << endl;

    return Status::OK;
}


Status Node::grant_request_access(ServerContext* context,
                            const PageRequestAccess* req_obj, 
                            Empty* reply) {

    void * page_addr = get_page_base_addr(req_obj->page_num(), req_obj->name());

    if(req_obj->is_write()) {
        cout << "Granting write access to page num: " << req_obj->page_num()  << endl;
        mprotect(page_addr, PAGE_SIZE, PROT_READ | PROT_WRITE);
        if(req_obj->page_data().size() != 0) {
            cout << "Copying new memory to page num: " << req_obj->page_num() << endl;
            const char* p_data = req_obj->page_data().c_str();
            void * base_addr = get_page_base_addr(req_obj->page_num(), req_obj->name());
            memcpy(base_addr, p_data, PAGE_SIZE);
        }
    } else {
        cout << "Granting read access to page num: " << req_obj->page_num() <<  endl;
        mprotect(page_addr, PAGE_SIZE, PROT_READ);
    }

    cout << "Granting request access locally to page num: " << req_obj->page_num() << endl;

    return Status::OK;

}


Status Node::revoke_write_access(ServerContext* context, 
                            const PageRequest* req_obj, 
                            PageData* reply) {

    cout << "Received to revoke write_access to page_num: " << req_obj->page_num() << endl;

    void * page_addr = get_page_base_addr(req_obj->page_num(), req_obj->name());
    
    cout << "Changing permissions to given page" << endl;

    mprotect(page_addr, PAGE_SIZE, PROT_READ);
    
    char* page = new char[PAGE_SIZE];
    memcpy(page, page_addr, PAGE_SIZE);
    string page_data(page, PAGE_SIZE);
    // Free up allocated space on heap
    delete[] page;

    cout << "Copying page data to reply object" << endl;

    reply->set_page_num(req_obj->page_num());
    reply->set_page_data(page_data); //TODO: check type for bytes/type page data

    cout << "Revoked write access to page num: " << req_obj->page_num() <<  endl;

    return Status::OK;

}


Status Node::fetch_page(ServerContext* context, 
                    const PageRequest* req_obj, 
                    PageData* reply) {

    cout << "Received to fetch page to page_num: " << req_obj->page_num() << endl;

    void * page_addr = get_page_base_addr(req_obj->page_num(), req_obj->name());
    
    cout << "Copying page data" << endl;

    char* page = new char[PAGE_SIZE];
    memcpy(page, page_addr, PAGE_SIZE);
    string page_data(page, PAGE_SIZE);
    delete[] page;

    reply->set_page_num(req_obj->page_num());
    reply->set_page_data(page_data);

    return Status::OK;

}

void Node::register_datasegment(void * psu_ds_start, size_t psu_ds_size) {
    if(!is_default){
        is_default = true;
    }
    
    num_pages = (int) (psu_ds_size / PAGE_SIZE);
    start_addr = psu_ds_start;
    end_addr = (void *)((char*)psu_ds_start + (int)psu_ds_size);

    //rpc call to directory node to register segment

    client.register_segment("default", num_pages);
    
    for(int i = 0; i < num_pages; i++) {
        mprotect(get_page_base_addr(i, "default"), PAGE_SIZE, PROT_READ);
    }

    cout << "Registered memory segment" << endl;

}

void *Node::register_malloc(char * name, size_t size) {
    // TODO: Do we need to lock here?
    string name_s(name);
    if (malloc_map.find(name_s) == malloc_map.end()) {
        void * start;
        // https://man7.org/linux/man-pages/man3/posix_memalign.3.html
        posix_memalign(&start, PAGE_SIZE, size);
        void * end = (void *)((char*)start + size);
        int pages = (int) (size / PAGE_SIZE);
        HeapMemory mem(start, end, pages);

        for(int i = 0; i < pages; i++) {
            mprotect(get_page_base_addr(i, start), PAGE_SIZE, PROT_NONE);
        }

        // FIXME: Because of race conditions with Grant Request this is here.
        malloc_map.insert(make_pair(name_s, mem));
        // Register on the Directory
        client.register_malloc(name_s, self, pages);
        cout << "[info] Registered Malloc on the server.\n";
        return start;
    }
}


void* psu_dsm_malloc(char *name, size_t size){
    if (!Node::instance.is_inited()) {
        Node::instance.init();
    }

    return Node::instance.register_malloc(name, size);
}


void psu_dsm_register_datasegment(void * psu_ds_start, size_t psu_ds_size) {

    cout << "[info] Registering the datasegment!!" << endl;

	//TODO: Make instance and call from here.
    if (!Node::instance.is_inited()) {
        Node::instance.init();
    }
    Node::instance.register_datasegment(psu_ds_start, psu_ds_size);
}
