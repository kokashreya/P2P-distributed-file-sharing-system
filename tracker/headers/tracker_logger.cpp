#include "logger_header.h"
using namespace std;

// ANSI color codes for console
#define COLOR_TIMESTAMP "\033[1;32m"  // Bold Green
#define COLOR_IP "\033[1;34m"         // Bold Blue
#define COLOR_INFO "\033[1;32m"       // Bold Green for INFO
#define COLOR_ERROR "\033[1;31m"      // Bold Red for ERROR
#define COLOR_FAILED "\033[1;33m"
#define COLOR_SYNC "\033[1;35m"
#define COLOR_RESET "\033[0m"

#define TAG_INFO "INFO"
#define TAG_ERROR "ERROR"
#define TAG_FAILED "FAILED"
#define TAG_SYNC "SYNC"
Logger::Logger() {
}

Logger::~Logger() {
    if (log_file.is_open())
        log_file.close();
}

void Logger::initialize(const string& file) {
    filename = file;
    log_file.open(filename, ios::app);
    if (!log_file.is_open()) {
        cerr << "Failed to open log file: " << filename << endl;
    }
}

void Logger::log(const string& message, const string& ip, int port, const string& tag, bool to_console) {
    lock_guard<mutex> lock(log_mutex);

    string full_message = format_message(message, ip, port, tag, false); // for file
    if (log_file.is_open()) {
        
        log_file << full_message << endl;
    }

    if (to_console) {
        full_message = format_message(message, ip, port, tag, true); // for console
        cout << full_message << endl<<flush;
    }
}

string Logger::format_message(const string& message, const string& ip, int port, const string& tag, bool to_console) {
    string time_stamp = current_time();
    
    // Fixed width for IP address part
    string address_info = "";
    if (!ip.empty() && port != -1) {
        address_info = "[" + ip + ":" + to_string(port) + "]";
    } else {
        address_info = "[0.0.0.0:0]";
    }
    const int ADDRESS_WIDTH = 16; 
    if (address_info.length() < ADDRESS_WIDTH) {
        address_info += string(ADDRESS_WIDTH - address_info.length(), ' ');
    }

    // Fixed width for tag
    const int TAG_WIDTH = 8;
    string tag_display = "[" + tag + "]";
    if (tag_display.length() < TAG_WIDTH) {
        tag_display += string(TAG_WIDTH - tag_display.length(), ' ');
    }

    if (to_console) {
        string colored_tag = tag_display;
        if (tag == TAG_INFO) {
            colored_tag = string(COLOR_INFO) + tag_display + string(COLOR_RESET);
        } else if (tag == TAG_ERROR) {
            colored_tag = string(COLOR_ERROR) + tag_display + string(COLOR_RESET);
        } else if (tag == TAG_FAILED) {
            colored_tag = string(COLOR_FAILED) + tag_display + string(COLOR_RESET);
        } else if(tag == TAG_SYNC){
            colored_tag = string(COLOR_SYNC) + tag_display + string(COLOR_RESET);
        }
        return string(COLOR_TIMESTAMP) + time_stamp + " " + string(COLOR_IP) + address_info + string(COLOR_RESET) + " " + colored_tag + " " + message;
    } else {
        return time_stamp + " " + address_info + " " + tag_display + " " + message;
    }
}

string Logger::current_time() {
    auto now = chrono::system_clock::now();
    time_t now_time = chrono::system_clock::to_time_t(now);
    char buf[100];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now_time));
    return string(buf);
}
