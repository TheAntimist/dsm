#include "psu_lock.h"

void psu_init_lock(int lockno) {
    Node::instance.create_new_lock(lockno);
    if (Node::instance.is_lock_inited()) return;

    ifstream infile("node_list.txt");

    string currentHost = localHostname(), host;
    vector<string> hosts;
    while (infile >> host) {
        if (currentHost == host) continue;
        hosts.push_back(host);
    }
    infile.close();
    // Remove Directory
    hosts.pop_back();
	
    vector<NodeClient> conns;
    for(string h: hosts){
	  NodeClient client(grpc::CreateChannel(h, grpc::InsecureChannelCredentials()), currentHost, h, Node::instance.get_logger());
        // Wait until ready
        client.hello();
        conns.push_back(client);
    }

    cout << "[debug] Initialized lock " << lockno << endl;
    Node::instance.init_locks(conns);

}
void psu_mutex_lock(int lockno) {
    Node::instance.lock_enter_cs(lockno);
}
void psu_mutex_unlock(int lockno) {
    Node::instance.lock_exit_cs(lockno);
}
