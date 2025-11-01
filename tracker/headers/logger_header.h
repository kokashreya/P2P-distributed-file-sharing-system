#ifndef LOGGER_HEADER_H
#define LOGGER_HEADER_H

#include <bits/stdc++.h>
#include <fstream>
#include <mutex>
#include <chrono>
#include <ctime>
using namespace std;

class Logger {
    ofstream log_file;
    mutex log_mutex;
    string filename;

public:
    Logger();
    ~Logger();

    void initialize(const string& filename); 
    void log(const string& message, const string& ip = "", int port = -1,const string& tag="INFO", bool to_console = false);

private:
    string format_message(const string& message, const string& ip, int port,const string& tag, bool to_console);
    string current_time();
};

#endif 
