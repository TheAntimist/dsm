LDFLAGS = -L/usr/local/lib `pkg-config --libs protobuf grpc++`\
           -Wl,--no-as-needed -lgrpc++_reflection -Wl,--as-needed\
           -ldl

CXX = g++
CPPFLAGS += `pkg-config --cflags protobuf grpc`
CXXFLAGS += -std=c++11 -g

GRPC_CPP_PLUGIN = grpc_cpp_plugin
GRPC_CPP_PLUGIN_PATH ?= `which $(GRPC_CPP_PLUGIN)`

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/usr/local/lib/pkgconfig/

all: build

p1: directory.pb.o directory.grpc.pb.o psu_dsm_system.o p1.o
	$(CXX) $^ $(LDFLAGS) -o $@

p2: directory.pb.o directory.grpc.pb.o psu_dsm_system.o p2.o
	$(CXX) $^ $(LDFLAGS) -o $@
p3: directory.pb.o directory.grpc.pb.o psu_dsm_system.o p3.o
	$(CXX) $^ $(LDFLAGS) -o $@

merge: directory.pb.o directory.grpc.pb.o psu_dsm_system.o psu_lock.o app1.o
	$(CXX) $^ $(LDFLAGS) -o $@

kmeans: directory.pb.o directory.grpc.pb.o psu_dsm_system.o psu_lock.o kmeans.o
	$(CXX) $^ $(LDFLAGS) -o $@

wordcount: directory.pb.o directory.grpc.pb.o psu_dsm_system.o psu_lock.o psu_mr.o wordcount.o
	$(CXX) -lboost_regex $^ $(LDFLAGS) -o $@


dir: directory

directory: directory.pb.o directory.grpc.pb.o directory.o
	$(CXX) $^ $(LDFLAGS) -o $@

%.grpc.pb.cc: %.proto
	protoc --grpc_out=. --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) $<

%.pb.cc: %.proto
	protoc --cpp_out=. $<

clean:
	rm -fv *.o *.pb.cc *.pb.h directory p1 p2 p3
