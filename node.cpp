#include "node.hpp"

Node::Node() {
    instance = this;
    is_init = true;
    sig.sa_flags = SA_SIGINFO;
    sig.sa_sigaction = &Node::wrap_signal_handler;
    if (sigaction(SIGSEGV, &sig, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    string actualHost = localHostname();
    ifstream infile("node_list.txt");
    string lastHost;
    int idx = -1;
    while (infile >> lastHost) {
        idx += 1;
        if (actualHost == host) {
	        self = idx;
	        continue;
	  }
    }
    infile.close()
    DirectoryClient _client(grpc::CreateChannel(lastHost,
                            grpc::InsecureChannelCredentials()));
    client = _client;
    // Wait for Directory connection.
    client.hello();
    cout << "[debug] Initialized Node with node id: " << curNode << endl;
}

void Node::invalidate_page(int page_num) {
	  
   void * addr = (void *) (((char *) start_addr) + PAGE_SIZE*page_num);

   mprotect(addr, PAGE_SIZE, PROT_NONE);
}

int Node::get_page_num(void * addr) {
    int page_num = ((int) ((((char*) addr) - ((char*) start_addr)) / PAGE_SIZE));
    return page_num;
}

void* Node::get_page_addr(void *addr, int page_num) {
    return (void*)( ((char *) addr) + PAGE_SIZE*page_num);
}

void * Node::get_page_addr(void *addr){

    int page_num = ((int) ((((int *) addr) - ((int*) start_addr)) / PAGE_SIZE)) - 1;

    return (void *)( ((char*) addr) + PAGE_SIZE*page_num);
}

void Node::request_access(void* addr, bool is_write){

    int page_num;
    
    //TODO: Type conversion as divide might be float, need to round up
    if(is_default){
        page_num = get_page_num(addr);
    }

    cout << "Requesting page number: " << page_num << endl;

    //TODO: use rpc call to talk to directory
    client.request_access(is_write, page_num, "default", self);

    //likely copy new page or request new page,etc,
    return;
}

void Node::sighandler(int sig, siginfo_t *info, void *ctx){
	  
    cout << "[debug] Sighandler called\n";

    if((((ucontext_t *)ctx)->uc_mcontext.gregs[REG_ERR]) & 0x2){
        cout << "[debug] write fault\n";

        int page_num;
    
        //TODO: Type conversion as divide might be float, need to round up
        if(is_default){
            page_num = get_page_num(info->si_addr);
        }

        cout << "Requesting page number: " << page_num << endl;

        //TODO: use rpc call to talk to directory
        client.request_access(true, page_num, "default", self);        
    
        return;
        //send ack back to directory   
    } else {
        cout << "[debug] read fault\n";

        int page_num;
    
        //TODO: Type conversion as divide might be float, need to round up
        if(is_default){
            page_num = get_page_num(info->si_addr);
        }
        cout << "Requesting page number: " << page_num << endl;

        AccessReply reply = client.request_access(false, page_num, "default", self);
        
        mprotect(get_page_addr(info->si_addr), PAGE_SIZE, PROT_WRITE);
        memcpy(get_page_addr(info->si_addr), &reply.page_data, PAGE_SIZE);
        mprotect(get_page_addr(info->si_addr), PAGE_SIZE, PROT_READ);
        return;
    }
}


Status Node::invalidate_page(ServerContext* context,
                           const PageRequest* req_obj,
                           Empty* reply) {
    int page_num = req_obj->page_num();
    
    void * page_addr = get_page_base_addr(req_obj->page_num());
    mprotect(page_addr, PAGE_SIZE, PROT_NONE);

    return Status::OK;
}


Status Node::grant_request_access(ServerContext* context,
                            const PageRequestAccess* req_obj, 
                            Empty* reply) override {

    void * page_addr = get_page_base_addr(req_obj->page_num());

    if(req_obj->is_write()){
        mprotect(page_addr, PAGE_SIZE, PROT_READ | PROT_WRITE);
    }
    else{
        mprotect(page_addr, PAGE_SIZE, PROT_READ);
    }

    return Status::OK;

}


Status Node::revoke_write_access(ServerContext* context, 
                            const PageRequest* req_obj, 
                            PageData* reply) override {


    void * page_addr = get_page_base_addr(req_obj->page_num());
    mprotect(page_addr, PAGE_SIZE, PROT_READ);
    string page; 
    memcpy(&page, page_addr, PAGE_SIZE);

    reply->set_page_num(req_obj->page_num());
    reply->set_page_data(page); //TODO: check type for bytes/type page data

    return Status::OK;

}


Status Node::fetch_page(ServerContext* context, 
                    const PageRequest* req_obj, 
                    PageData* reply) override{

    void * page_addr = get_page_base_addr(req_obj->page_num());
    string page;
    memcpy(&page, page_addr, PAGE_SIZE);

    reply->set_page_num(req_obj->page_num());
    reply->set_page_data(page); //TODO: type check page data

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

}


psu_dsm_ptr_t psu_dsm_malloc(char *name, size_t size){


//Check if the given name is allocated (shared or remote), if not, create

//if name is not allocated, 

}


void psu_dsm_register_datasegment(void * psu_ds_start, size_t psu_ds_size) {
	//TODO: Make instance and call from here.

}


int main() {
  DirectoryClient client(grpc::CreateChannel("e5-cse-135-07.cse.psu.edu:8080", grpc::InsecureChannelCredentials()));
  // Wait until ready
  client.hello();
  cout << client.register_segment("default", 2) << endl;

  return 0;
}
