
---

# Peer-to-Peer Distributed File Sharing System

---

## Project Overview

This project implements a **comprehensive Peer-to-Peer Distributed File Sharing System** with advanced features including multi-tracker synchronization, piece-based file transfer, and intelligent seeder management. The system consists of two main components: **Client** and **Tracker** modules that work together to provide fault-tolerant, scalable file sharing capabilities.

### Key System Features

- **Multi-Tracker Synchronization**: Automatic state synchronization across multiple tracker instances
- **Piece-Based File Transfer**: Efficient parallel downloads using fixed-size file pieces
- **Intelligent Seeder Management**: Real-time seeder availability checking using login maps
- **Thread Pool Architecture**: Concurrent handling of multiple operations
- **Fault Tolerance**: Automatic tracker failover and session restoration
- **Round-Robin Load Balancing**: Optimized seeder selection for download performance

---

## Combined Architecture Flow

<img src="Diagarm/flow.jpeg" alt="System Flow" width="900"/>

---

# Multi-Tracker Synchronization Mechanism

To maintain **consistency across multiple trackers**, every time a command is successfully executed on one tracker (such as `create_user`, `login`, `create_group`, etc.), the tracker broadcasts this update to all other trackers using a sophisticated synchronization protocol.

## Synchronization Protocol

* Messages are sent with a **`SYNC` prefix** to distinguish synchronization operations from client commands.
* Each synchronization attempt uses a separate thread, which waits **up to 5 seconds** for a connection to the peer tracker.
* The system does **not block or wait for a response** from other trackers, ensuring immediate client responsiveness.
* **10 Core Sync Operations**: login, create_user, logout, create_group, join_group, accept_request, leave_group, upload_file_data, update_file_info, stop_share.

## Sync Operations Details

### User Management Sync
- `login` - User login session management across trackers
- `create_user` - New user registration synchronization  
- `logout` - User logout session management and cleanup

### Group Management Sync
- `create_group` - New group creation and ownership setup
- `join_group` - Group membership requests and updates
- `accept_request` - Group join request approvals
- `leave_group` - Group membership removal and cleanup

### File Management Sync
- `upload_file_data` - New file metadata distribution
- `update_file_info` - File seeder information updates
- `stop_share` - Remove user as file seeder with availability check

## Why This Design is Excellent

1. **Non-blocking & Responsive**
   * The main tracker thread never stalls while waiting for peers.
   * Clients continue to get immediate responses without delay.
   * Asynchronous sync operations maintain system performance.

2. **Fault Tolerance**
   * If a tracker is down or unreachable, the system simply skips it after 5 seconds.
   * This avoids a single point of failure and maintains system availability.
   * Eventual consistency ensures data integrity across the network.

3. **Eventual Consistency**
   * All live trackers receive updates through reliable sync mechanisms.
   * Even if one misses due to downtime, it will catch up once restarted.
   * Serialization-based metadata distribution ensures data accuracy.

4. **Scalability**
   * New trackers can be added without changing the synchronization model.
   * Each tracker independently pushes updates to peers.
   * Linear scaling with additional tracker instances.

In summary, this approach balances **consistency, availability, and responsiveness** by ensuring all trackers stay updated **without compromising client performance**.

---


# Client Module - Enhanced P2P File Sharing

## Project Overview

The **Client module** implements a sophisticated peer-to-peer file sharing system with advanced features including piece-based downloads, round-robin seeder selection, and intelligent upload validation. The client connects to trackers, manages file uploads/downloads, handles peer-to-peer connections, and provides comprehensive commands for user and group operations.

### Key Client Features

- **Enhanced Piece-Based Downloads**: Random piece selection with round-robin seeder assignment
- **Upload Command Flow**: Comprehensive file validation and metadata generation
- **List Files with Seeder Check**: Only shows files with available seeders
- **Stop Share Intelligence**: Removes files only when no other seeders remain
- **Thread Pool Management**: Concurrent handling of multiple peer connections
- **Fault Tolerance**: Automatic tracker reconnection with session restoration

---

## Client Code Structure

* **client.cpp** – Entry point, starts the client with IP:PORT and tracker file.
* **client\_header.h / client\_skelton.cpp** – Defines and implements the `Client` class with all core functionality.
* **thread\_header.h / client\_threads.cpp** – Thread pool management for concurrent operations.
* **utils\_header.h / client\_utils.cpp** – Helper functions for validation, file handling, and string operations.
* **file\_header.h** – File handling and piece management declarations.
* **Makefile** – Compilation rules with pthread, SSL, and crypto libraries.

---

## Client Flow of Execution

### Architecture Diagram
<img src="Diagarm/client.jpeg" alt="Client Flow" style="width:60%;"/>

### Client Execution Steps
1. **Start the Client**  
   The program begins with `client.cpp`, which calls `Client::start()`.

2. **Start Listener**  
   - A listener thread is started on the IP and port provided in the command line.  
   - This port is reserved for incoming peer connections for file piece requests.
   - If another client connects, the listener accepts the connection and assigns it to an available thread.
   - The assigned thread handles file piece serving based on piece index requests.

3. **Connect to Tracker**  
   - The client reads tracker addresses from `tracker_info.txt`.  
   - It tries each entry one by one until a connection is successful.  
   - Once connected, the client sends its own IP and port to the tracker for peer discovery.

4. **Tracker Session Handling**  
   - If a connection to the tracker breaks, the client tries the next tracker in the list.  
   - If the user was logged in previously, the client asks if they want to log in automatically.  
   - Before logging into a new tracker, the client sends a logout request to clear stale entries.  
   - After logout, it logs in again and resumes communication with the new tracker.

5. **Enhanced File Operations**
   - Download: Random piece selection with round-robin seeder assignment
   - Upload: Comprehensive validation and metadata generation with SHA calculation
   - List Files: Only displays files with active seeders (login map validation)
   - Stop Share: Intelligent file removal based on remaining seeder availability

---

## Enhanced Client Commands

### File Operations with Intelligence

#### 1. Download Command Enhanced Flow
```
file_download_command()
├── Parse command (group, file, destination)
├── Request file metadata from tracker
├── Receive seeder list and piece information
├── **Random piece order generation**
├── For each piece:
│   ├── **Select seeder using round-robin (piece_index % seeder_count)**
│   ├── assign_download_task()
│   ├── download_piece() from selected seeder
│   └── write_content() to correct file offset
└── Notify completion when all pieces downloaded
```

#### 2. Upload Command Enhanced Flow
```
file_upload_command()
├── Parse command (group, file_path)
├── Validate local file existence and permissions
├── Send upload request to tracker
├── **Tracker validation check**
│   ├── Check group membership/ownership
│   ├── Check if file already exists in group
│   └── Return "send_all_data" ACK if valid
├── **Generate file metadata locally**
│   ├── Calculate full file SHA
│   ├── Divide file into pieces (512KB each)
│   ├── Calculate individual piece SHA for each piece
│   ├── Create FileInfo structure with metadata
│   └── Serialize metadata for transmission
├── Send complete file metadata to tracker
└── Client ready to serve file pieces to peers
```

#### 3. List Files Command Enhanced Flow
```
list_files <group_id>
├── Parse command (group_id)
├── Send list_files request to tracker
├── **Tracker processes file list request**
│   ├── Validate user is member of group
│   ├── Get all files in group from FileManager
│   └── For each file in group:
│       ├── **Check seeder availability using login map**
│       │   ├── Get current seeder list for file
│       │   ├── Check if seeder exists in login map
│       │   └── Filter out offline/unreachable seeders
│       ├── **Include file only if seeders available**
│       │   ├── If seeders > 0: Add file to response list
│       │   └── If seeders = 0: Skip file (not downloadable)
└── Display only files with available seeders
```

#### 4. Stop Share Command Enhanced Flow
```
stop_share <group_id> <file_name>
├── Parse command (group_id, file_name)
├── Send stop_share request to tracker
├── **Tracker processes stop share request**
│   ├── Validate user is currently seeding the file
│   ├── Remove user from file's seeder list
│   └── **Check remaining seeders availability**
│       ├── For each remaining seeder:
│       │   ├── Check if seeder exists in login map
│       │   └── Count active/online seeders
│       ├── **If no active seeders remain:**
│       │   ├── Remove file completely from group
│       │   ├── Delete file metadata from FileManager
│       │   └── Sync file removal to other trackers
│       └── **If other seeders available:**
│           ├── Keep file in group with updated seeder list
└── Return operation status message
```

---

## Client Technical Implementation

### Round-Robin Seeder Selection
- **Algorithm**: `piece_index % seeder_count`
- **Purpose**: Balances load across all available seeders
- **Example**: 4 pieces, 2 seeders → P0→S0, P1→S1, P2→S0, P3→S1

### File I/O Operations
- **Methods Used**: lseek64(), open64(), read()/write() operations
- **Specifically Avoided**: pwrite/pread (as per implementation choice)
- **Piece Size**: Fixed 512KB pieces for optimal transfer performance
- **SHA Verification**: Full file SHA + individual piece SHA for integrity

### Seeder Availability Validation
- **Method**: Login map status checking instead of socket connection tests
- **Efficiency**: O(1) lookup time using hash maps
- **Real-time**: Updated during each list_files operation
- **Accuracy**: Only shows files with currently logged-in seeders

---





# Tracker Module - Advanced Coordination System

## Project Overview

The **Tracker** serves as the advanced coordination component of the Peer-to-Peer Distributed File Sharing System. It manages users, groups, file metadata, client sessions, and implements sophisticated multi-tracker synchronization with 10 core operations. The tracker provides fault tolerance through distributed state management and ensures data consistency across the network.

### Key Tracker Features

- **Multi-Tracker Synchronization**: 10 sync operations with serialization-based metadata distribution
- **Advanced File Management**: FileManager with seeder tracking and login map validation
- **Comprehensive User Management**: Session tracking with Address mapping and thread-safe operations
- **Group Management**: Owner/member hierarchy with pending request handling
- **Real-time Sync Operations**: Asynchronous updates with timeout-based failure detection
- **Thread Pool Architecture**: Concurrent client handling with dedicated ClientManager instances

---

## Tracker Code Structure

* **tracker.cpp** – Entry point, initializes and starts the tracker with specified configuration.
* **tracker_header.h / tracker_sckelton.cpp** – Defines and implements the `Tracker` class with core functionality.
* **managers/** – Contains specialized manager classes for different system components:
  * **manager.h** – Defines all manager classes and data structures.
  * **user_manager.cpp** – User registration, authentication, and session management.
  * **group_manager.cpp** – Group operations, membership, and permission management.
  * **file_manager.cpp** – File metadata, seeder tracking, and availability management.
  * **command_manager.cpp** – Command execution coordination and sync handling.
  * **client_manager.cpp** – Individual client connection and communication management.
* **headers/** – Header files for logging, threading, and utility functions.
* **Makefile** – Build configuration with threading and networking libraries.

---

## Tracker Flow of Execution

### Architecture Diagram
<img src="Diagarm/tracker_flow.jpeg" alt="Tracker Flow" width="1000"/>

### Tracker Execution Steps

1. **Start the Tracker**  
   The program begins with `tracker.cpp`, which calls `Tracker::start()`.

2. **Read Configuration**  
   - The tracker reads its own IP, port, and ID from `tracker_info.txt`.
   - Stores all other tracker addresses for synchronization.

3. **Initialize Synchronization**  
   - Calls `init_sync()` to prepare connections to peer trackers.
   - Sets up sync operations for 10 core operations.

4. **Start Listener**  
   - The tracker listens on its IP and port.
   - When a client or tracker connects, a thread is assigned from the pool.
   - A `ClientManager` object is created to handle communication.

5. **Client Communication**  
   - `ClientManager::login_loop()` ensures the client logs in first.
   - `while_loop()` processes commands until logout/disconnection.

6. **Advanced Data Update & Sync**  
   - When users, groups, or files are updated, `notify_sync()` is called.
   - This triggers `Tracker::start_sync()` to send updates to all other live trackers.
   - Updates use serialization and are sent asynchronously with 5-second timeout.

---

## Core Tracker Sync Operations

The tracker implements **10 sophisticated sync operations** for complete state management:

### User Management Sync Operations
1. **login** - User login session management with Address tracking
2. **create_user** - New user registration with thread-safe hash map updates
3. **logout** - User logout session cleanup and state synchronization

### Group Management Sync Operations
4. **create_group** - New group creation with owner assignment and metadata sync
5. **join_group** - Group membership requests with pending status management
6. **accept_request** - Group join request approvals with member list updates
7. **leave_group** - Group membership removal with owner/member validation

### File Management Sync Operations
8. **upload_file_data** - New file metadata distribution with FileInfo serialization
9. **update_file_info** - File seeder information updates with Address tracking
10. **stop_share** - Remove user as file seeder with intelligent availability checking


---

## Advanced Data Structures

### 1. Enhanced User Management
```cpp
class UserManager {
private:
    unordered_map<string, string> users;           // Username to password mapping
    unordered_map<string, Address> logged_in;      // Username to address mapping
    mutex mtx;                                     // Thread synchronization
}
```

### 2. Sophisticated Group Management
```cpp
struct GroupInfo {
    string owner;                              // Single owner per group
    vector<string> members;                    // Dynamic member list
    unordered_set<string> pending;            // Fast pending lookup
};
unordered_map<string, GroupInfo> groups;      // Group name to info mapping
unordered_map<string, unordered_set<string>> owner_groups;  // Owner to groups
```

### 3. Advanced File Management
```cpp
class FileManager {
private:
    unordered_map<string, unordered_map<string, FileInfo>> group_files;  // Group to files
    unordered_map<string, unordered_map<string, Address>> user_files;    // User to files
    mutex file_mtx;                                                      // Thread safety
    
public:
    // FileInfo with serialization support
    struct FileInfo {
        string name, group, owner;
        uint64_t file_size, piece_size;
        int total_pieces;
        string file_sha, piece_shas;
        unordered_map<string, Address> seeders;
        
        string toString() const;                    // Serialization
        static FileInfo fromString(const string& data);  // Deserialization
    };
}
```

---

## Tracker Manager Classes

### 1. Enhanced UserManager

**Key Functions:**
* `bool registerUser(const string& username, const string& password)` – Thread-safe user creation
* `bool login(const string& username, const string& password, const Address& addr)` – Session management with Address tracking
* `bool logout(const string& username)` – Session cleanup with sync notification
* `bool isLoggedIn(const string& username)` – Real-time login status checking
* `bool getUserAddress(const string& username, Address& addr)` – Address retrieval for peer connections

### 2. Advanced GroupManager

**Key Functions:**
* `bool createGroup(const string& group_name, const string& owner)` – Group creation with owner assignment
* `bool requestToJoin(const string& group_name, const string& username)` – Pending request management
* `bool approveRequest(const string& group_name, const string& username, const string& approver)` – Owner-only approval process
* `bool leaveGroup(const string& group_name, const string& username)` – Member removal with validation
* `bool isGroupOwner(const string& group_name, const string& username)` – Permission checking
* `bool isMemberOfGroup(const string& group_name, const string& username)` – Membership validation

### 3. Sophisticated FileManager

**Key Functions:**
* `bool addFile(const FileInfo& file_info)` – File metadata storage with serialization
* `bool updateSeeder(const string& group, const string& filename, const string& user, const Address& addr)` – Seeder tracking
* `string listFiles(const string& group, const UserManager& user_mgr)` – Login map validation for seeder availability
* `bool removeFile(const string& group, const string& filename)` – File removal with seeder cleanup
* `bool stopSharing(const string& group, const string& filename, const string& user, const UserManager& user_mgr)` – Intelligent file removal

### 4. Enhanced CommandManager

**Key Functions:**
* `string sync_handler(const string& sync_message)` – Processes all 10 sync operations
* Command processing functions for all client operations
* Integration with all manager classes
* Sync message generation and parsing

### 5. Advanced ClientManager

**Key Functions:**
* `void start_communication()` – Client session initialization
* `void login_loop()` – Enforced authentication before command processing
* `void while_loop()` – Command processing with real-time sync
* `void notify_sync(const string& message)` – Sync trigger for all operations


---

## Class Overview

### **Tracker**

* **start()** → Main entry point for tracker execution.
* **set\_tracker\_address()** → Reads tracker’s own IP/port from `tracker_info.txt`.
* **start\_as\_server()** → Begins listening for clients/trackers and spawns `ClientManager` threads.
* **assign\_task\_to\_thread()** → Allocates an available thread to handle a client connection.
* **init\_sync()** → Loads other trackers’ addresses for future synchronization.
* **start\_sync()** → Broadcasts sync updates across trackers.
* **send\_sync\_message()** → Sends a sync message to a specific tracker.
* **stop()** → Gracefully stops the tracker.

---

### **ClientManager**

* Handles communication with **one client**.
* Uses shared objects: `UserManager`, `GroupManager`, `CommandManager`, `Logger`.
* **start\_communication()** → Starts client session.
* **login\_loop()** → Blocks until client successfully logs in.
* **while\_loop()** → Processes commands continuously.
* **notify\_sync()** → Notifies parent `Tracker` about a successful data update.

---

### **UserManager**

* Manages all user-related operations.
* **Data Structures**:

  * `unordered_map<string, string> users` → Maps username → password.
  * `unordered_map<string, Address> logged_in` → Maps username → logged-in address.
* **Functions**:

  * `registerUser()` → Create a new account.
  * `isUser()` → Check if user exists.
  * `login()` / `logout()` → Manage sessions.
  * `isLoggedIn()` → Check active status.
  * `showLoggedInUsers()` → Debug helper to display current users.

---

### **GroupManager**

* Manages groups, members, and join requests.
* **Data Structures**:

  * `unordered_map<string, GroupInfo> groups`

    * `owner` (string) → Owner username
    * `members` (vector<string>) → Group members
    * `pending` (unordered\_set<string>) → Pending join requests
  * `unordered_map<string, unordered_set<string>> owner_groups` → Owner → set of owned groups
* **Functions**:

  * `createGroup()` → Create a group.
  * `requestToJoin()` → Add pending join request.
  * `approveRequest()` → Accept pending request.
  * `leaveGroup()` → Remove a user from a group.
  * `groupList()` → Return list of groups.
  * `isGroupOwner()` / `isMemberOfGroup()` / `isGroupAvailabel()` → Validation helpers.
  * `showGroup()` → Display group info.

---

### **CommandManager**

* Executes commands from clients.
* Works with `UserManager`, `GroupManager`, and `Logger`.
* **Functions**:

  * `login_command()` / `logout_command()` / `create_user_command()`
  * `create_group_command()` / `join_group_command()` / `leave_group_command()`
  * `list_group_command()` / `list_request_command()` / `accept_request_command()`
  * `sync_handler()` → Applies sync updates from other trackers.

---

### **Logger**

* Shared across all tracker components.
* Logs messages to both **console** and **file**.
* Helps trace system activity.

---

### **Thread Management (tracker\_threads.h)**

Thread pool functions (same as client side):

* `is_any_thread_available()` → Checks availability.
* `get_available_thread()` → Returns index of available thread.
* `release_thread()` → Frees thread after use.
* `join_all_threads()` → Waits for all threads to complete.
* `clear_thread_pool()` → Resets thread pool.

---

### **Utility Functions (utils\_header.h)**

* `file_validation()` → Check if file exists and is valid.
* `read_file_by_line_number()` → Read specific line from file.
* `string_address_validation()` / `ip_address_validation()` → Validate IP and port.
* `get_address()` → Parse IP\:port string into `Address` struct.
* `client_argument_validation()` / `tracker_argument_validation()` → Validate command-line arguments.
* `no_entries_in_file()` → Count entries in a file.
* `trim_whitespace()` / `to_lowercase()` → String formatting helpers.
* `tokenize()` → Split string into tokens.

---

## Supported Commands (from Client)

* `create user <user id> <password>`
* `login <user id> <password>`
* `create group <group id>`
* `join group <group id>`
* `leave group <group id>`
* `list groups`
* `list requests <group id>`
* `accept request <group id> <user id>`
* `logout`

---

## Build & Run

### Compile

```bash
make
```

### Run

```bash
./tracker tracker_info.txt <id>
```

* `tracker_info.txt` → File containing all tracker addresses.
* `<id>` → Tracker ID (unique per tracker).

Example `tracker_info.txt`:

```
127.0.0.1:9998    1
127.0.0.1:9999    2
```

Run tracker with ID `1`:

---

## Combined Build & Run Instructions

### Prerequisites
- **Operating System:** Linux (Ubuntu 18.04+ recommended)
- **Compiler:** g++ with C++17 support
- **Required Libraries:** pthread, SSL, crypto libraries

### Compile Both Modules
```bash
# Compile Tracker
cd tracker/
make clean && make

# Compile Client  
cd ../client/
make clean && make
```

### Configuration File Setup
Create `tracker_info.txt` in both directories:
```
127.0.0.1:9998    1
127.0.0.1:9999    2
```

### Run System Components

#### Start Trackers (Multiple Terminal Windows)
```bash
# Terminal 1 - Start first tracker
cd tracker/
./tracker tracker_info.txt 1

# Terminal 2 - Start second tracker
cd tracker/
./tracker tracker_info.txt 2
```

#### Start Clients (Multiple Terminal Windows)
```bash
# Terminal 3 - Start first client
cd client/
./client 127.0.0.1:5000 tracker_info.txt

# Terminal 4 - Start second client
cd client/
./client 127.0.0.1:6000 tracker_info.txt
```

---

## Comprehensive System Commands

### User Management Commands
* `create_user <user_id> <password>` – Register new user account with tracker
* `login <user_id> <password>` – Authenticate and start session with tracker

### Group Management Commands
* `create_group <group_id>` – Create new group (user becomes owner)
* `join_group <group_id>` – Request membership in existing group
* `leave_group <group_id>` – Leave group you are member of
* `list_groups` – Display all available groups in system
* `list_requests <group_id>` – Show pending join requests (owner only)
* `accept_request <group_id> <user_id>` – Accept join request (owner only)

### Enhanced File Operations Commands
* `upload_file <file_path> <group_id>` – Upload file with comprehensive validation and metadata generation
* `download_file <group_id> <file_name> <destination_path>` – Download file using piece-based transfer with round-robin seeder selection
* `list_files <group_id>` – Show files available in group (only displays files with active seeders)
* `stop_share <group_id> <file_name>` – Stop sharing file with intelligent removal (removes file if no other seeders)
* `show_downloads` – Display active and completed downloads

### Session Management Commands
* `logout` – End session and stop sharing files

---

## Advanced System Features

### 1. **Multi-Tracker Fault Tolerance**
- Automatic tracker failover with session restoration
- 10 core sync operations maintain consistency across tracker network
- 5-second timeout for tracker communication with graceful failure handling
- Eventual consistency through serialization-based metadata distribution

### 2. **Intelligent File Management**
- **Piece-Based Transfer**: 512KB fixed pieces with SHA verification
- **Round-Robin Load Balancing**: `piece_index % seeder_count` for optimal distribution
- **Real-Time Seeder Validation**: Login map checking for active seeder status
- **Smart File Removal**: Files removed only when no active seeders remain

### 3. **Enhanced Concurrency**
- **Hardware-Aware Thread Pool Architecture**: `MAX_THREADS = max(2u, min(10u, thread::hardware_concurrency()))` for both client and tracker
  - **Minimum**: 2 threads (ensures basic concurrency on any system)
  - **Maximum**: 10 threads (prevents resource exhaustion)
  - **Optimal**: Automatically detects CPU cores and adapts to hardware
- Concurrent piece downloads from multiple seeders
- Asynchronous sync operations without blocking client responses
- Thread-safe data structures with mutex protection

### 4. **Network Optimization**
- TCP connections with length-prefixed message protocol
- Binary piece transfer protocol for efficient data transfer
- Socket timeout handling and connection state validation
- Automatic reconnection with preserved session state

---

## System Testing Scenarios

### 1. **Basic System Testing**
```bash
# Test user registration and login
> create_user alice password123
> login alice password123

# Test group creation and management
> create_group project_team
> join_group project_team
> list_groups
```

### 2. **Advanced File Sharing Testing**
```bash
# Test file upload with validation
> upload_file /path/to/document.pdf project_team
> list_files project_team

# Test intelligent download with multiple seeders
> download_file project_team document.pdf ./downloads/
> show_downloads
```

### 3. **Multi-Tracker Sync Testing**
```bash
# Start 2 trackers, create user on tracker 1
# Connect client to tracker 2, verify user exists (sync worked)
# Test all 10 sync operations across tracker instances
```

### 4. **Seeder Intelligence Testing**
```bash
# Upload file from client A
> upload_file file.txt group1

# List files from client B (should show file with 1 seeder)
> list_files group1

# Client A logs out, client B lists files (should show no files)
> logout  # from client A
> list_files group1  # from client B (file should not appear)
```

### 5. **Stop Share Intelligence Testing**
```bash
# Multiple clients upload same file
# One client stops sharing
> stop_share group1 file.txt

# File should remain available if other seeders exist
# File should be removed if no other seeders remain
```

---

## Performance Characteristics

### Scalability Metrics
- **Concurrent Clients**: Hardware-adaptive (2-10 per instance based on CPU cores)
- **Tracker Instances**: Unlimited with linear sync overhead
- **File Size**: No theoretical limit (piece-based transfer)
- **Download Speed**: Scales with number of available seeders

### Network Efficiency
- **Piece Size**: 512KB optimized for network and disk I/O
- **Load Distribution**: Round-robin ensures even seeder utilization
- **Bandwidth Usage**: Parallel downloads maximize throughput
- **Connection Management**: Persistent tracker connections, on-demand peer connections
- **Hardware Utilization**: Dynamic thread allocation based on system capabilities

### Memory Usage
- **Client**: O(n) where n = number of active downloads + piece metadata
- **Tracker**: O(u + g + f) where u = users, g = groups, f = files
- **Sync Operations**: O(t) where t = number of tracker instances

---

## System Limitations

### Technical Limitations
- **No encryption support** - All communication in plaintext
- **Limited NAT traversal** - Assumes direct peer connectivity  
- **Fixed thread pool size** - Maximum 20 concurrent operations
- **In-memory storage** - No persistent database (tracker data lost on restart)
- **No compression** - File transfers use raw binary data

### Protocol Limitations
- **Simple conflict resolution** - Last update wins, no sophisticated merging
- **No partition tolerance** - Split-brain scenarios not handled elegantly
- **Basic authentication** - No sophisticated security mechanisms
- **Single connection per client** - No connection pooling or multiplexing

### Operational Limitations
- **All trackers must start simultaneously** - Late-starting trackers miss earlier sync
- **No resume capability** - Downloads must restart from beginning if interrupted
- **Manual intervention required** - Limited automated error recovery
- **Command-line interface only** - No graphical user interface


### Download Performance Benchmarks

**Implementation Note**: The system includes a 50ms delay per 100 pieces downloaded for optimization and seeder load balancing.

**Real-World Performance Measurements**:
Based on testing with a 3.6GB file (taking ~15 seconds), estimated download times:

| File Size | Estimated Pieces (512KB) | Estimated Download Time |
|-----------|-------------------------|-------------------------|
| 1 MB      | ~2 pieces              | < 1 second              |
| 10 MB     | ~20 pieces             | ~1 second               |
| 100 MB    | ~200 pieces            | ~2-3 seconds            |
| 1 GB      | ~2,000 pieces          | ~12-15 seconds          |
| 4 GB      | ~8,000 pieces          | ~18-22 seconds          |

**Performance Factors**:
- **Piece Processing**: 50ms delay per 100 pieces (0.5ms per piece average)
- **Network Speed**: Depends on available bandwidth and seeder connectivity
- **Concurrent Downloads**: Multiple pieces downloaded simultaneously using thread pool
- **Seeder Count**: More seeders = faster downloads due to load distribution
- **System Resources**: I/O performance and available memory affect speed

**Optimization Strategy**:
- Small files (< 10MB): Minimal overhead, very fast transfers
- Medium files (10MB - 1GB): Efficient piece-based parallel downloads
- Large files (> 1GB): Significant benefit from multiple seeders and concurrent piece downloads
  

---

## Current Implementation Status

✅ **Fully Implemented Features:**
- Multi-tracker synchronization with 10 core operations
- Enhanced piece-based file transfer with round-robin seeder selection
- Intelligent seeder management using login map validation
- Smart file removal based on seeder availability
- Comprehensive upload validation and metadata generation
- Automatic tracker failover with session restoration
- Thread pool architecture for concurrent operations
- Real-time sync operations with timeout handling


---
