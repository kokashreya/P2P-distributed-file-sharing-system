# Peer-to-Peer Distributed File Sharing System (Tracker)

## Project Overview

This project implements the **Tracker module** of a Peer-to-Peer Distributed File Sharing System.
The tracker serves as the central coordination component that manages users, groups, file metadata, and client sessions. It provides fault tolerance through multi-tracker synchronization and ensures data consistency across the distributed system.

---

## Code Structure

* **tracker.cpp** – Entry point, initializes and starts the tracker with specified configuration.
* **tracker_header.h / tracker_sckelton.cpp** – Defines and implements the `Tracker` class with core functionality.
* **managers/** – Contains specialized manager classes for different system components:
  * **manager.h** – Defines all manager classes and data structures.
  * **user_manager.cpp** – Handles user registration, authentication, and session management.
  * **group_manager.cpp** – Manages groups, memberships, and join requests.
  * **file_manager.cpp** – Handles file metadata and sharing information.
  * **command_manager.cpp** – Processes and executes client commands.
  * **client_manager.cpp** – Manages individual client connections and communication.
* **headers/** – Header files for logging, threading, and utility functions.
* **Makefile** – Build configuration with threading and networking libraries.

---

## Detailed Compilation and Execution Instructions

### Prerequisites
- **Operating System:** Linux (Ubuntu 18.04+ recommended)
- **Compiler:** g++ with C++17 support
- **Required Libraries:** 
  - pthread (for threading)
  - Standard C++ libraries for networking

### Compilation Process
```bash
# Navigate to tracker directory
cd /path/to/project/tracker

# Clean previous builds
make clean

# Compile the project
make

# Alternative manual compilation
g++ -std=c++17 -Wall -Iheaders -Imanagers \
    -o tracker tracker.cpp headers/*.cpp managers/*.cpp -lpthread
```

### Execution Instructions

#### 1. Prepare Configuration File
Create `tracker_info.txt` with all tracker addresses:
```
127.0.0.1:9998    1
127.0.0.1:9999    2
```

#### 2. Start Tracker
```bash
# Basic execution
./tracker tracker_info.txt <tracker_id>

# Examples
./tracker tracker_info.txt 1
./tracker tracker_info.txt 2
```

#### 3. Command Line Arguments
- **Argument 1:** `tracker_info.txt` - File containing all tracker addresses and IDs
- **Argument 2:** `<tracker_id>` - Unique identifier for this tracker instance

#### 4. Runtime Validation
The tracker validates:
- Configuration file existence and readability
- Tracker ID validity and uniqueness
- Network interface availability
- Port binding capabilities

---

## Architectural Overview

### System Architecture Diagram
<img src="../Diagarm/tracker_flow.jpeg" alt="Tracker Flow" width="1000"/>

### Multi-Layer Architecture

#### 1. **Application Layer**
- Command processing and validation
- Client session management
- Multi-tracker synchronization

#### 2. **Manager Layer**
- UserManager: User registration and authentication
- GroupManager: Group operations and membership
- FileManager: File metadata and sharing
- CommandManager: Command execution coordination
- ClientManager: Individual client communication

#### 3. **Network Communication Layer**
- TCP socket management
- Client connection handling
- Inter-tracker synchronization protocol

#### 4. **Thread Management Layer**
- Thread pool for concurrent client handling
- Synchronization primitives for thread safety
- Resource allocation and cleanup

#### 5. **Data Management Layer**
- In-memory data structures for fast access
- File-based logging for persistence
- State synchronization across trackers

### Component Interaction Flow
```
Client Request → ClientManager → CommandManager → Specific Manager (User/Group/File)
                                      ↓
Logger ← Tracker Sync ← Data Update ← Manager Response
```

---

## Flow of Execution

### Steps
1. **Start the Tracker**  
   The program begins with `tracker.cpp`, which calls `Tracker::start()`.

2. **Read Configuration**  
   - The tracker reads its own IP, port, and ID from `tracker_info.txt`.
   - Stores all other tracker addresses for synchronization.

3. **Initialize Synchronization**  
   - Calls `init_sync()` to prepare connections to peer trackers.
   - When updates occur, sync messages are sent to other trackers.

4. **Start Listener**  
   - The tracker listens on its IP and port.
   - If a client or tracker connects, a thread is assigned from the pool.
   - A `ClientManager` object is created to handle communication with the client.

5. **Client Communication**  
   - `ClientManager::login_loop()` ensures the client logs in first.
   - `while_loop()` processes commands until logout/disconnection.

6. **Data Update & Sync**  
   - When users or groups are updated, `notify_sync()` is called in `ClientManager`.
   - This triggers `Tracker::start_sync()` to send updates to all other live trackers.
   - Updates are sent asynchronously; trackers unavailable within 5 seconds are skipped.

---

## Key Algorithms Implemented

### 1. **Multi-Tracker Synchronization Algorithm**
```
Data Update Event → Generate Sync Message → Broadcast to All Trackers
                                          ↓
Timeout Handler (5s) ← Network Send ← Message Serialization
```
**Purpose:** Maintains data consistency across multiple tracker instances.
**Complexity:** O(n) where n is the number of tracker instances.

### 2. **Thread Pool Management Algorithm**
```
Client Connection → Check Thread Availability → Assign to Available Thread
                                              ↓
ClientManager Creation ← Thread Assignment ← Thread Pool Management
```
**Purpose:** Efficiently handles multiple concurrent client connections.
**Complexity:** O(1) for thread assignment with blocking wait if pool full.

### 3. **Session Management Algorithm**
```
Client Connect → Login Required → Authentication → Session Creation → Command Processing
                      ↓                              ↓
              Retry/Disconnect              Session Tracking → Sync Update
```
**Purpose:** Ensures secure access and maintains active session state.
**Complexity:** O(1) for session lookup and management operations.

### 4. **Group Permission Management Algorithm**
```
Group Operation → Validate User → Check Permissions → Execute Operation → Sync Update
                       ↓               ↓
               User Exists Check   Owner/Member Check
```
**Purpose:** Enforces group access control and permission hierarchy.
**Complexity:** O(1) for permission checks using hash maps.

### 5. **File Metadata Distribution Algorithm**
```
File Upload → Generate Metadata → Store Locally → Sync to Other Trackers → Update Seeders
                     ↓                ↓
        Serialize to Object    Object to Serial Transfer
```
**Purpose:** Distributes file information across tracker network for redundancy through serialization.
**Complexity:** O(m) where m is the number of file pieces.

---

## Data Structures and Rationale

### 1. **User Management**
```cpp

unordered_map<string, string> users;           // Username to password mapping
unordered_map<string, Address> logged_in;      // Username to address mapping
mutex mtx;                                     // Thread synchronization

```
**Rationale:** Hash maps provide constant-time access for user operations. Separate storage for credentials and sessions allows efficient session management with thread-safe operations.

### 2. **Group Management**
```cpp
struct GroupInfo {
    string owner;                              // Single owner per group
    vector<string> members;                    // Ordered member list
    unordered_set<string> pending;            // Fast pending lookup
};
unordered_map<string, GroupInfo> groups;      // Group name to info mapping
unordered_map<string, unordered_set<string>> owner_groups;  // Owner to groups
```
**Rationale:** Nested data structures optimize different access patterns. Sets for pending requests enable fast duplicate checking.

### 3. **File Management**
```cpp
struct FileInfo {
    string name, group, owner;                 // File identification
    uint64_t size, piece_size;                // Size information
    vector<string> piece_SHA;                 // Piece verification
    map<string, Address> seeder_users;        // Seeder locations
    map<string, string> user_file_map;        // User to file path mapping
    string toString();                        // Serialization method
    static FileInfo fromString(const string& data);  // Deserialization method
};

class FileManager {
private:
    unordered_map<string, unordered_map<string, FileInfo>> group_files;  // Group to files mapping
    unordered_map<string, unordered_set<string>> user_files;             // User to files mapping
    mutex file_mtx;                                                      // Thread synchronization
}
```
**Rationale:** Comprehensive file metadata with serialization support enables efficient piece-based sharing and cross-tracker synchronization. Nested hash maps provide fast group-to-file and user-to-file lookups.

### 4. **Thread Management**
```cpp
// Hardware-aware thread allocation
const int MAX_THREADS = max(2u, min(10u, thread::hardware_concurrency()));
vector<thread> thread_pool;                   // Dynamic thread storage
vector<bool> thread_available;               // O(1) availability check
mutex thread_mutex;                          // Thread pool synchronization
condition_variable thread_cv;                // Efficient waiting
```
**Rationale:** Thread pool prevents overhead of constant thread creation/destruction. Condition variables avoid busy waiting. **Hardware-aware allocation** automatically adapts to system capabilities:
- **Minimum**: 2 threads (ensures basic concurrency)
- **Maximum**: 10 threads (prevents resource exhaustion)  
- **Optimal**: Automatically detects CPU cores using `thread::hardware_concurrency()`

### 5. **Synchronization State**
```cpp
vector<Address> other_trackers;              // Peer tracker addresses
mutex sync_mutex;                           // Sync operation protection
```
**Rationale:** Simple vector storage for tracker addresses with mutex protection for concurrent sync operations.

---

## Network Protocol Design and Message Formats

### 1. **Client-Tracker Communication Protocol**

#### Login Request
```
Format: "login <username> <password>\n"
Example: "login alice mypassword\n"
Response: "Login successful" or "Login failed"
```

#### User Registration
```
Format: "create_user <username> <password>\n"
Example: "create_user bob newpassword\n"
Response: "User created successfully" or "User already exists"
```

#### Group Operations
```
Format: "create_group <group_name>\n"
        "join_group <group_name>\n"
        "leave_group <group_name>\n"
        "list_groups\n"
Example: "create_group project_team\n"
Response: Operation status and relevant data
```

#### File Operations
```
Format: "upload_file <group_id> <file_path>\n"
        "download_file <group_id> <file_name>\n"
        "list_files <group_id>\n"
Example: "upload_file team1 document.pdf\n"
Response: File metadata or operation status
```

### 2. **Inter-Tracker Synchronization Protocol**

#### Sync Message Format
```
Format: "SYNC <operation> <data>\n"
Examples: 
  "SYNC USER_CREATE alice:password123\n"
  "SYNC GROUP_CREATE team1:alice\n"
  "SYNC USER_LOGIN alice:127.0.0.1:5000\n"
```

#### Sync Operations
- `login` - User login session management
- `create_user` - New user registration
- `logout` - User logout session management
- `create_group` - New group creation
- `join_group` - Group membership requests
- `accept_request` - Group join request approvals
- `leave_group` - Group membership removal
- `upload_file_data` - New file metadata addition
- `update_file_info` - File seeder information updates
- `stop_share` - Remove user as file seeder


### 3. **Session Management**
```
Session Start: Automatic on successful login
Session End: "logout\n" command or connection drop
Heartbeat: Implicit through command activity
```

---

## Class and Function Details

### 1. Tracker Class

The **Tracker** class manages the overall tracker functionality and coordinates all operations.

**Private Functions - Initialization & Setup:**

* `bool set_tracker_address()` – Reads tracker configuration from tracker_info.txt and sets up network binding.
* `bool start_as_server()` – Initializes server socket and starts listening for client connections.
* `void assign_task_to_thread(int client_socket)` – Assigns incoming client connections to available worker threads.
* `void init_sync()` – Initializes synchronization with other tracker instances.

**Private Functions - Communication Management:**

* `void handle_client(int client_sock)` – Creates ClientManager instance to handle individual client communication.
* `void send_sync_message(const Address& address, const string& message, int& updated_count)` – Sends synchronization messages to peer trackers.
* `void input_listener()` – Handles administrative input and commands.

**Public Functions:**

* `Tracker(const string& tracker, const int id)` – Constructor initializing tracker with configuration file and unique ID.
* `bool start()` – Orchestrates tracker startup: configuration reading, server start, sync initialization.
* `void start_sync(string message)` – Broadcasts synchronization messages to all peer trackers.
* `bool stop()` – Gracefully shuts down tracker and cleans up resources.

---

### 2. UserManager Class

Handles all user-related operations including registration, authentication, and session management.

**Private Data Structures:**
* `unordered_map<string, string> users` – Maps username to password for authentication.
* `unordered_map<string, Address> logged_in` – Tracks active user sessions with their network addresses.
* `mutex mtx` – Ensures thread-safe access to user data structures.

**Public Functions:**

* `bool registerUser(const string& username, const string& password)` – Creates new user account with validation.
* `bool isUser(const string& username)` – Checks if username exists in system.
* `bool login(const string& username, const string& password, const Address& addr)` – Authenticates user and creates session.
* `bool logout(const string& username)` – Terminates user session and updates status.
* `bool isLoggedIn(const string& username)` – Checks if user has active session.
* `bool getUserAddress(const string& username, Address& addr)` – Retrieves network address of logged-in user.
* `void showLoggedInUsers()` – Debug function to display all active sessions.

---

### 3. GroupManager Class

Manages groups, memberships, join requests, and group permissions.

**Private Data Structures:**
* `unordered_map<string, GroupInfo> groups` – Maps group name to group information structure.
* `unordered_map<string, unordered_set<string>> owner_groups` – Maps user to groups they own.
* `mutex group_mtx` – Ensures thread-safe access to group data structures.

**GroupInfo Structure:**
```cpp
struct GroupInfo {
    string owner;                    // Group owner username
    vector<string> members;          // List of group members
    unordered_set<string> pending;   // Pending join requests
};
```

**Public Functions:**

* `bool createGroup(const string& group_name, const string& owner)` – Creates new group with specified owner.
* `bool requestToJoin(const string& group_name, const string& username)` – Adds user to pending join requests.
* `bool approveRequest(const string& group_name, const string& username, const string& approver)` – Accepts pending join request (owner only).
* `bool leaveGroup(const string& group_name, const string& username)` – Removes user from group membership.
* `string groupList()` – Returns formatted list of all available groups.
* `bool isGroupOwner(const string& group_name, const string& username)` – Checks if user owns the group.
* `bool isMemberOfGroup(const string& group_name, const string& username)` – Checks if user is group member.
* `bool isGroupAvailable(const string& group_name)` – Checks if group exists in system.
* `string showGroup(const string& group_name)` – Returns detailed group information.

---

### 4. FileManager Class

Handles file metadata, sharing information, and seeder management.

**Key Functions:**

* `bool addFile(const FileInfo& file_info)` – Stores file metadata in system.
* `bool getFileInfo(const string& group, const string& filename, FileInfo& info)` – Retrieves file information.
* `bool updateSeeder(const string& group, const string& filename, const string& user, const Address& addr)` – Updates seeder information.
* `string listFiles(const string& group)` – Returns list of files available in group.
* `bool removeFile(const string& group, const string& filename)` – Removes file metadata from system.

---

### 5. CommandManager Class

Coordinates command execution across different manager classes.

**Command Processing Functions:**

* `string login_command(const string& username, const string& password, const Address& addr)` – Processes login requests.
* `string logout_command(const string& username)` – Handles logout requests.
* `string create_user_command(const string& username, const string& password)` – Processes user registration.
* `string create_group_command(const string& group_name, const string& username)` – Handles group creation.
* `string join_group_command(const string& group_name, const string& username)` – Processes group join requests.
* `string leave_group_command(const string& group_name, const string& username)` – Handles group leave requests.
* `string list_group_command()` – Returns available groups list.
* `string list_request_command(const string& group_name, const string& username)` – Lists pending group requests.
* `string accept_request_command(const string& group_name, const string& target_user, const string& approver)` – Processes request approvals.
* `string sync_handler(const string& sync_message)` – Applies synchronization updates from other trackers.

---

### 6. ClientManager Class

Manages individual client connections and communication sessions.

**Core Functions:**

* `void start_communication()` – Initiates client communication session.
* `void login_loop()` – Enforces login requirement before command processing.
* `void while_loop()` – Main command processing loop for authenticated clients.
* `void notify_sync(const string& message)` – Triggers synchronization with other trackers.

---

## Assumptions Made During Implementation

### 1. **Network Assumptions**
- TCP provides reliable, ordered delivery between trackers and clients
- Network partitions are temporary and connectivity will be restored
- Trackers can establish direct connections to each other
- Maximum concurrent client connections limited by system resources
- Network latency between trackers is reasonable (< 5 seconds)

### 2. **System Assumptions**
- Single machine can handle multiple tracker instances on different ports
- File system provides reliable persistent storage for logs
- System memory sufficient for in-memory data structures
- Linux environment with POSIX-compliant threading
- System clock synchronization across tracker instances

### 4. **Data Consistency Assumptions**
- Network partitions do not last indefinitely
- **All trackers must start simultaneously** - Late-starting trackers will not have complete data synchronization and may miss earlier state changes

### 5. **Tracker Synchronization Assumptions**
- Clients properly handle tracker reconnection scenarios
- Maximum session duration is reasonable (not indefinite)
- Clients send well-formed commands and handle responses appropriately
- Group operations are not performed at extremely high frequency
- File uploads include valid metadata and checksums

### 5. **Security Assumptions**
- Network communication occurs over trusted infrastructure
- Password storage in plaintext acceptable for prototype system
- Basic authentication sufficient without encryption
- Tracker instances are trusted and not malicious
- No sophisticated attack scenarios (DoS, injection, etc.)

---

## Build & Run

### Compile
```bash
make clean
make
```

### Run
```bash
./tracker tracker_info.txt <tracker_id>
```

* `tracker_info.txt` → File containing all tracker addresses and IDs
* `<tracker_id>` → Unique identifier for this tracker instance

---

## Tracker Info File Format

Example `tracker_info.txt`:
```
127.0.0.1:9998    1
127.0.0.1:9999    2
```

---

## Supported Commands (from Client)

### User Management
* `create_user <user_id> <password>` – Register new user account
* `login <user_id> <password>` – Authenticate and start session

### Group Management  
* `create_group <group_id>` – Create new group (user becomes owner)
* `join_group <group_id>` – Request membership in existing group
* `leave_group <group_id>` – Leave group you are member of
* `list_groups` – Display all available groups in system
* `list_requests <group_id>` – Show pending join requests (owner only)
* `accept_request <group_id> <user_id>` – Accept join request (owner only)

### File Operations
* `upload_file <group_id> <file_path>` – Upload file to specified group
* `download_file <group_id> <file_name>` – Download file using metadata
* `list_files <group_id>` – Show files available in group

### Session Management
* `logout` – End session and clear login status

---

## System Limitations

### 1. **Technical Limitations**
- **In-memory storage only** - No persistent database, data lost on restart
- **No encryption** - All communication and storage in plaintext
- **Limited scalability** - Single-threaded managers may become bottlenecks
- **No data backup** - Only logging available for recovery
- **Fixed thread pool** - Cannot dynamically adjust to load

### 2. **Synchronization Limitations**
- **Simple conflict resolution** - Last update wins, no sophisticated merging
- **No partition tolerance** - Split-brain scenarios not handled
- **Timeout-based failure detection** - May miss brief network issues
- **No acknowledgment system** - Sync messages sent fire-and-forget

### 3. **Protocol Limitations**
- **No compression** - Sync messages and responses use raw text
- **Limited message validation** - Basic format checking only
- **No message versioning** - Protocol changes break compatibility
- **Single connection per client** - No connection pooling or multiplexing

### 4. **Security Limitations**
- **No access control lists** - Basic owner/member permissions only
- **No rate limiting** - Vulnerable to request flooding
- **No audit logging** - Limited traceability of operations
- **Plain text passwords** - No hashing or encryption

---

## Testing Procedures and Sample Commands

### 1. **Single Tracker Testing**

#### Start Tracker
```bash
# Start tracker with ID 1
./tracker tracker_info.txt 1
Expected: Tracker starts listening on configured port
```

#### User Management Testing
```bash
# Connect client and test user operations
./client 127.0.0.1:5000 tracker_info.txt

# Create new user
> create_user alice password123
Expected: User created successfully.

# Test login
> login alice password123
Expected: Login successful.

# Test duplicate user creation
> create_user alice newpass
Expected: User already exists.
```

#### Group Management Testing
```bash
# Create group as logged-in user
> create_group project_team
Expected: Group created successfully.

# List groups
> list_groups
Expected: Shows project_team in list.

# Test group joining workflow
# Terminal 2 (different client):
> create_user bob password456
> login bob password456
> join_group project_team

# Terminal 1 (alice):
> list_requests project_team
Expected: Shows bob's pending request.

> accept_request project_team bob
Expected: Request accepted successfully.
```

### 2. **Multi-Tracker Synchronization Testing**

#### Start Multiple Trackers
```bash
# Terminal 1
./tracker tracker_info.txt 1

# Terminal 2  
./tracker tracker_info.txt 2

Expected: Both trackers start and establish sync connections
```

#### Cross-Tracker Data Consistency
```bash
# Connect client to tracker 1
./client 127.0.0.1:5000 tracker_info.txt
> login alice password123
> create_group shared_group

# Connect different client to tracker 2
./client 127.0.0.1:6000 tracker_info.txt
> login bob password456
> list_groups
Expected: shared_group visible on tracker 2

> join_group shared_group
Expected: Join request synchronized across trackers
```

#### Tracker Failover Testing
```bash
# With client connected to tracker 1
> login alice password123
> create_group test_group

# Stop tracker 1
# Client should reconnect to tracker 2
> list_groups
Expected: test_group still available due to sync
```

### 4. **Data Integrity Testing**

#### State Verification
```bash
# Create known data set
> create_user test_user test_pass
> create_group test_group
> join_group test_group

# Verify data consistency across trackers
Expected: All trackers have identical state

# Test edge cases
> leave_group nonexistent_group
Expected: Appropriate error message
```

#### Sync Message Validation
```bash
# Monitor tracker logs during operations
# Verify sync messages are generated and processed
Expected: All state changes properly synchronized
```

---

## Key Features

### 1. **Multi-Tracker Synchronization**
- Real-time state synchronization across tracker instances
- Fault tolerance through redundant tracker operation
- Automatic conflict resolution and data consistency

### 2. **Concurrent Client Handling**
- Thread pool management for multiple simultaneous clients
- Thread-safe operations with proper mutex protection
- Scalable connection management

### 3. **Comprehensive User Management**
- User registration and authentication
- Session tracking and management
- Secure login/logout functionality

### 4. **Flexible Group System**
- Hierarchical permission structure (owner/member)
- Join request and approval workflow
- Dynamic membership management

### 5. **Robust File Metadata Management**
- Piece-based file information storage
- Seeder tracking and management
- File sharing coordination

---

## Technical Details

### Synchronization Architecture
```
Tracker Network (All Equal Topology)
├── Tracker 1 (Full Operations)
├── Tracker 2 (Full Operations)
├── Tracker 3 (Full Operations)
└── Sync Protocol (TCP Messages)
```

### Thread Management Architecture
```
Tracker Instance
├── Main Thread (Server Listener)
├── Admin Thread (Input Processing)
├── Worker Thread Pool (Client Handling)
│   ├── ClientManager 1 (User Session)
│   ├── ClientManager 2 (User Session)
│   └── ClientManager N (User Session)
└── Sync Thread (Inter-Tracker Communication)
```

### Data Flow Architecture
```
Client Request → ClientManager → CommandManager → Manager (User/Group/File) → Data Update
                                      ↓                    ↓
Logger ← Sync Broadcast ← Tracker.start_sync() ← notify_sync()
```

---

## Performance Optimizations

### Network Efficiency
- Asynchronous synchronization to prevent blocking
- Timeout-based failure detection (5 seconds)
- Efficient TCP socket reuse and connection management
- Minimal message overhead in sync protocol

### Memory Management
- Hash map data structures for O(1) lookup operations
- Shared pointer usage for automatic memory management
- Efficient string handling and minimal copying
- Thread-local storage where appropriate

### Concurrency Optimizations
- Fine-grained locking for different data structures
- Read-write lock separation where possible
- Lock-free operations for read-heavy workloads
- Condition variables to avoid busy waiting

---

## Current Implementation Status

✅ **Completed Features:**
- Tracker startup and configuration management
- Multi-tracker synchronization system
- User registration and authentication
- Group management with permissions
- File metadata handling
- Thread pool for concurrent client handling
- Comprehensive logging system
- Inter-tracker communication protocol

---
