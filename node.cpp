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

class Node final : public NodeService::Service {
	DirectoryClient client;
	bool is_init = false;
	bool is_default = false;
	struct sigaction sig;
	int num_pages = 0;
	void* start_addr;
public:


    Status invalidate_page(ServerContext* context,
                           const PageRequest* req_obj,
                           Empty* reply) override {

        int page_num = req_obj->page_num();
        
        void * page_addr = get_page_base_addr(req_obj->page_num());
        mprotect(page_addr, PAGE_SIZE, PROT_NONE);

        return Status::OK;

    }


    Status grant_request_access(ServerContext* context,
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


    Status revoke_write_access(ServerContext* context, 
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


    Status fetch_page(ServerContext* context, 
                      const PageRequest* req_obj, 
                      PageData* reply) override{

        void * page_addr = get_page_base_addr(req_obj->page_num());
        string page;
        memcpy(&page, page_addr, PAGE_SIZE);
        
        reply->set_page_num(req_obj->page_num());
        reply->set_page_data(page); //TODO: type check page data

        return Status::OK;

    }

    
    void* get_page_base_addr(int page_num){
        void* page_addr = (void *) (((char *) start_addr) + PAGE_SIZE*page_num);
        return page_addr;
    }


	void* get_page_addr(void *addr, int page_num){
	    return (void*)( ((char *) addr) + PAGE_SIZE*page_num);
    }
	  
	void * get_page_addr(void *addr){
	  
	    int page_num = ((int) ((((int *) addr) - ((int*) start_addr)) / PAGE_SIZE)) - 1;
	      
	    return (void *)( ((char*) addr) + PAGE_SIZE*page_num);
	}  
	  
	  
	  void request_access(void* addr, bool is_write){
	  
	      int page_num;
	      
	  
	      //TODO: Type conversion as divide might be float, need to round up 
	      if(is_default){
	          page_num = (int) ((((int *) addr) - ((int*) start_addr)) / PAGE_SIZE) - 1;
	      }
	  
	      //use rpc call to talk to directory
	  
	      //likely copy new page or request new page,etc,
	      return;
	  }
	  
	  void sighandler(int sig, siginfo_t *info, void *ctx){
	  
	      printf("Sighandler called\n");
	  
	      if((((ucontext_t *)ctx)->uc_mcontext.gregs[REG_ERR]) & 0x2){
	          printf("write fault\n");
	  
	          request_access(info->si_addr, true);
	      
	          mprotect(get_page_addr(info->si_addr), PAGE_SIZE, PROT_READ | PROT_WRITE);
	  
	       
	          //send ack back to directory   
	      }
	      else{
	          printf("read fault\n");
	  
	          request_access(info->si_addr, false);
	  
	          mprotect(get_page_addr(info->si_addr), PAGE_SIZE, PROT_READ);
	          //send ack back to directory
	      }
	  
	      
	  
	  
	  }
	  
	  
	  void psu_dsm_setup(){
	      is_init = true;
	      sig = { 0 };
	      sig.sa_flags = SA_SIGINFO;
	      sig.sa_sigaction = &sighandler;
	      if (sigaction(SIGSEGV, &sig, NULL) == -1){
	          perror("sigaction");
	          exit(EXIT_FAILURE);
	      }
	  
	  } 

	static Node* instance;
	static void wrap_signal_handler(int sig, siginfo_t *info, void *ctx) {
		instance->sighandler(sig, info, ctx);
	}
	
}

psu_dsm_ptr_t psu_dsm_malloc(char *name, size_t size){


//Check if the given name is allocated (shared or remote), if not, create

//if name is not allocated, 

}



void psu_dsm_register_datasegment(void * psu_ds_start, size_t psu_ds_size){

    if(!is_init){
        psu_dsm_setup();
    }
    if(!is_default){
        is_default = true;
    }
    
    num_pages = (int) (psu_ds_size / PAGE_SIZE);
    start_addr = psu_ds_start;

    //rpc call to directory node to register segment
	// TODO:
	// register_segment("default", num_pages)
    
    for(int i = 0; i < num_pages; i++){
        mprotect(get_page_addr(psu_ds_start, i), PAGE_SIZE, PROT_READ);
    }
    

}


int main() {
  DirectoryClient client(grpc::CreateChannel("e5-cse-135-07.cse.psu.edu:8080", grpc::InsecureChannelCredentials()));
  // Wait until ready
  client.hello();
  cout << client.register_segment("default", 2) << endl;

  return 0;
}
