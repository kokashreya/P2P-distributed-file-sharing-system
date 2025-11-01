#include "managers/manager.h"
#include "headers/utils_header.h"
#include "headers/tracker_header.h"

int main(int argc, char* argv[]) {
    UserManager um;
    GroupManager gm;
    

    if(!tracker_argument_validation(argc, argv)) {
        return 1;
    }

    Tracker tracker(argv[1], stoi(argv[2]));
    if(!tracker.start()) {
        cerr << "Failed to start tracker.\n";
        return 1;
    }

    return 0;
}
