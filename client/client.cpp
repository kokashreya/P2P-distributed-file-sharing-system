#include "headers/client_header.h"
#include "headers/utils_header.h"
#include<iostream>
#include <sys/stat.h>
using namespace std;



int main(int argc, char *argv[]) {
    if(!client_argument_validation(argc, argv)) {
        return 1;
    }

    string address=argv[1];
    size_t colon_pos = address.find(':');
    if (colon_pos == string::npos) {
        cerr << "Invalid server address format. Expected <server_ip>:<server_port>\n";
        return 1;
    }

    string my_ip = address.substr(0, colon_pos);
    int my_port = stoi(address.substr(colon_pos + 1));
    string tracker_file = argv[2];

    Client client(my_ip, my_port, tracker_file);


    if(!client.start()) {
        cerr << "Failed to start client.\n";
        if(!client.stop()){
            cerr << "Failed to stop client.\n"; 
        }
        return 1;
    }

    return 0;

}