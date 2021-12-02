#include <iostream>
#include <unordered_map>
#include <memory>

using namespace std;

class Lock {
public:
  int maxSeqNo;
  int highestSeqNo;
  bool requestingCS;
  Lock() {
	maxSeqNo = highestSeqNo = 0;
  }
};

void test(shared_ptr<Lock> l) {
  l->maxSeqNo = 20;
}

int main() {
  std::unordered_map<int, shared_ptr<Lock>> lockMap;
  shared_ptr<Lock> s(new Lock());
  lockMap[0] = s;
  auto l = lockMap[0];
  l->maxSeqNo = 10;
  std::cout << l->maxSeqNo << std::endl;
  test(l);
  std::cout << l->maxSeqNo << std::endl;
  //lockMap[0] = l;
  std::cout << lockMap[0]->maxSeqNo << std::endl;
  std::cout << [=](){return !l->requestingCS;}() << std::endl;
  return 0;
}
