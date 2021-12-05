#include "node.hpp"

string localHostname() {
  char buffer[100];
  int ret;
  if ((ret = gethostname(buffer, sizeof(buffer))) == -1) {
    return "";
  }
  return string(buffer) + string(":8080");
}

Node Node::instance;

Node::Node() {
    memset(&sig, 0, sizeof(sig));

    thread t(&Node::startServer, this);
    t.detach();

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
                            grpc::InsecureChannelCredentials()));
    client = _client;
    // Wait for Directory connection.
    client.hello();
    cout << "[debug] Initialized Node with node id: " << self << endl;
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

int Node::get_page_num(void * addr) {
    int page_num = ((int) ((((char*) addr) - ((char*) start_addr)) / PAGE_SIZE));
    return page_num;
}

void* Node::get_page_addr(void *addr, int page_num) {
    return (void*)( ((char *) start_addr) + PAGE_SIZE*page_num);
}

void * Node::get_page_addr(void *addr){

    int page_num = ((int) ((((char *) addr) - ((char  *) start_addr)) / PAGE_SIZE));

    return (void *)( ((char*) start_addr) + PAGE_SIZE*page_num);
}

void Node::sighandler(int sig, siginfo_t *info, void *ctx){
	  
    cout << "[debug] Sighandler called\n";

    if((((ucontext_t *)ctx)->uc_mcontext.gregs[REG_ERR]) & 0x2){
        cout << "[debug] write fault\n";

        int page_num;
    
        if(is_default){
            page_num = get_page_num(info->si_addr);
        }

        cout << "Requesting page number: " << page_num << endl;

        AccessReply reply = client.request_access(true, page_num, "default", self);        

        //TODO: Do Later, get the new page!!! 
        return;
    } else {
        cout << "[debug] read fault\n";

        int page_num;
    
        if(is_default){
            page_num = get_page_num(info->si_addr);
        }
        cout << "Requesting page number: " << page_num << endl;

        AccessReply reply = client.request_access(false, page_num, "default", self);
       
         
        mprotect(get_page_addr(info->si_addr), PAGE_SIZE, PROT_WRITE);
        cout << "Changing page protecting to write only" << endl;   
        const char* p_data = reply.page_data().c_str();
        cout << "Did the p_data conversion" << endl;
        void * base_addr = get_page_addr(info->si_addr);
        printf("Base address of the page is: %x\n", base_addr);
        memcpy(base_addr, p_data, PAGE_SIZE);
        cout << "Memcopying to the p_data" << endl;
        mprotect(get_page_addr(info->si_addr), PAGE_SIZE, PROT_READ);
        cout << "Changed permission to read only" << endl;
        return;
    }
}


Status Node::invalidate_page(ServerContext* context,
                           const PageRequest* req_obj,
                           Empty* reply) {


    
    int page_num = req_obj->page_num();
    
    void * page_addr = get_page_base_addr(req_obj->page_num());
    mprotect(page_addr, PAGE_SIZE, PROT_NONE);

    cout << "Invalidating page num: " << req_obj->page_num() << endl;

    return Status::OK;
}


Status Node::grant_request_access(ServerContext* context,
                            const PageRequestAccess* req_obj, 
                            Empty* reply) {

    void * page_addr = get_page_base_addr(req_obj->page_num());

    if(req_obj->is_write()){
        cout << "Granting write access" << endl;
        mprotect(page_addr, PAGE_SIZE, PROT_READ | PROT_WRITE);
    }
    else{
        cout << "Granting read access" << endl;
        mprotect(page_addr, PAGE_SIZE, PROT_READ);
    }

    cout << "Granting request access locally to page num: " << req_obj->page_num() << endl;

    return Status::OK;

}


Status Node::revoke_write_access(ServerContext* context, 
                            const PageRequest* req_obj, 
                            PageData* reply) {

    cout << "Received to revoke write_access to page_num: " << req_obj->page_num() << endl;

    void * page_addr = get_page_base_addr(req_obj->page_num());
    
    cout << "Changing permissions to given page" << endl;

    mprotect(page_addr, PAGE_SIZE, PROT_READ);
    
    char* page = new char[PAGE_SIZE];
    memcpy(page, page_addr, PAGE_SIZE);
    string page_data(page, PAGE_SIZE);

    cout << "Copying page data to reply object" << endl;

    reply->set_page_num(req_obj->page_num());
    reply->set_page_data(page_data); //TODO: check type for bytes/type page data

    cout << "Revoked write access" << endl;

    return Status::OK;

}


Status Node::fetch_page(ServerContext* context, 
                    const PageRequest* req_obj, 
                    PageData* reply) {

    cout << "Received to fetch page to page_num: " << req_obj->page_num() << endl;

    void * page_addr = get_page_base_addr(req_obj->page_num());
    
    cout << "Copying page data" << endl;

    char* page = new char[PAGE_SIZE];
    memcpy(page, page_addr, PAGE_SIZE);
    string page_data(page, PAGE_SIZE);

    reply->set_page_num(req_obj->page_num());
    reply->set_page_data(page_data); //TODO: type check page data

    return Status::OK;

}

void Node::register_datasegment(void * psu_ds_start, size_t psu_ds_size) {
    if(!is_default){
        is_default = true;
    }
    
    num_pages = (int) (psu_ds_size / PAGE_SIZE);
    start_addr = psu_ds_start;

    //rpc call to directory node to register segment

    client.register_segment("default", num_pages);
    
    for(int i = 0; i < num_pages; i++){
        mprotect(get_page_addr(psu_ds_start, i), PAGE_SIZE, PROT_READ);
    }

    cout << "Registered memory segment" << endl;

}


void* psu_dsm_malloc(char *name, size_t size){


//Check if the given name is allocated (shared or remote), if not, create

//if name is not allocated, 

}


void psu_dsm_register_datasegment(void * psu_ds_start, size_t psu_ds_size) {

    cout << "Registering the datasegment!!" << endl;

	//TODO: Make instance and call from here.
    if (!Node::instance.is_inited()) {
        Node::instance.init();
    }
    Node::instance.register_datasegment(psu_ds_start, psu_ds_size);
}


/* int main() {
  DirectoryClient client(grpc::CreateChannel("e5-cse-135-07.cse.psu.edu:8080", grpc::InsecureChannelCredentials()));
  // Wait until ready
  client.hello();
  cout << client.register_segment("default", 2) << endl;

  return 0;
} */
