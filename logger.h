#ifndef _LOGGER_H_
#define _LOGGER_H_
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <string>
#include <fstream>
#include <memory>
#include <iostream>

template<typename ... Args>
std::string string_format( const std::string& format, Args ... args )
{
    int size_s = std::snprintf( nullptr, 0, format.c_str(), args ... ) + 1; // Extra space for '\0'
    if( size_s <= 0 ){ throw std::runtime_error( "Error during formatting." ); }
    auto size = static_cast<size_t>( size_s );
    std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf( buf.get(), size, format.c_str(), args... );
    return std::string( buf.get(), buf.get() + size - 1 ); // We don't want the '\0' inside
}

class Logger {
  std::shared_ptr<std::mutex> m;
  //std::thread write_thread;
  std::shared_ptr<std::ofstream> file;
public:

    Logger(std::string component) : file(new std::ofstream("rpc-log-file-" + component + ".txt")),
                                    m(new std::mutex()) { }

    ~Logger() {
        file->close();
    }

    // Add an element to the queue.
    void log(std::string t) {
        std::lock_guard<std::mutex> lock(*m);
        *file << t << std::endl;
    }

    //TODO: Check this later
    Logger& operator=(Logger other) {
	  file = other.file;
      m = other.m;
	  return *this;
  }
};

#endif
