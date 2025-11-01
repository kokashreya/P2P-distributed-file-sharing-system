# Peer-to-Peer Distributed File Sharing System (Client)

## Project Overview

This project implements the **Client module** of a Peer-to-Peer Distributed File Sharing System.
The client connects to trackers, manages file uploads/downloads, handles peer-to-peer connections, and provides commands for user and group operations.
It uses a thread pool to handle multiple peer connections concurrently and implements efficient piece-based file sharing.

---

## Code Structure

* **client.cpp** – Entry point, starts the client with IP:PORT and tracker file.
* **client\_header.h / client\_skelton.cpp** – Defines and implements the `Client` class with all core functionality.
* **thread\_header.h / client\_threads.cpp** – Thread pool management for concurrent operations.
* **utils\_header.h / client\_utils.cpp** – Helper functions for validation, file handling, and string operations.
* **file\_header.h** – File handling and piece management declarations.
* **Makefile** – Compilation rules with pthread, SSL, and crypto libraries.

---

## Flow of Execution

### Architecture Diagram
<img src="../Diagarm/client.jpeg" alt="Client Flow" style="width:60%;"/>

### Steps
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

5. **Command Handling**  
   - The client accepts user commands for account management, groups, and file operations.
   - Variables are saved in memory to support reconnection and session restoration.

6. **File Download Process**
   - When downloading, pieces are selected randomly for better distribution.
   - Seeders are chosen using round-robin: piece index % number_of_seeders.
   - Multiple threads download different pieces concurrently from different seeders.

---

## Class and Function Details

### 1. Client Class

The **Client** class manages tracker connections, listener setup, command handling, and peer-to-peer file sharing.

**Private Functions - Connection Management**

* `bool read_tracker()` – Reads tracker IP and port details from tracker_info.txt file.
* `bool set_tracker_address()` – Configures tracker socket address structure for connection.
* `bool connect_to_tracker()` – Establishes TCP connection with tracker and sends client details.
* `void reset_tracker()` – Switches to next available tracker when current connection fails.

**Private Functions - Server Operations**

* `void server_listener(int server_fd, struct sockaddr_in address, socklen_t addrlen)` – Accepts incoming peer connections and delegates to threads.
* `bool start_as_listener()` – Binds to specified IP:PORT and starts listening for peer requests.
* `void handle_client(int client_sock, mutex &file_mutex)` – **Serves file pieces to requesting peers based on piece index.**
* `void assign_task_to_thread(int client_socket, mutex &file_mutex)` – Assigns peer connection to available thread from pool.

**Private Functions - Command Processing**

* `void start_client_command_loop()` – Main interactive loop reading and processing user commands.
* `string handle_command(string command)` – Routes commands to appropriate handlers (upload/download/tracker commands).
* `string file_upload_command(string command)` – Processes file upload requests and updates tracker with file metadata.
* `string file_download_command(string command, shared_ptr<DownloadTask> task_ptr, shared_ptr<mutex> results_mutex)` – **Manages file downloads with random piece selection and round-robin seeder assignment.**

**Private Functions - Download Management**

* `void assign_download_task(...)` – **Creates download threads for individual pieces with round-robin seeder selection.**
* `bool download_piece(...)` – **Downloads specific file piece from assigned seeder using piece index.**
* `string receive_full_file_data(int sock)` – Receives variable-length data from tracker with length prefix protocol.
* `bool receive_piece(int sock, string& piece_content, uint64_t piece_size)` – Receives binary file piece data from peer.
* `bool write_content(...)` – Writes downloaded piece to correct file offset using piece index.
* `int connect_with_timeout(const string& ip, int port, int timeout_sec)` – Creates socket connection with timeout for peer communication.

**Public Functions**

* `Client(const string& ip, int port, const string& tracker)` – Constructor initializing client with listening address and tracker file.
* `bool start()` – Orchestrates client startup: tracker connection, listener start, command loop.
* `bool stop()` – Gracefully shuts down client, closes connections, and joins threads.

---

### 2. Thread Manager (thread\_header.h / client\_threads.cpp)

Manages thread pool for concurrent peer connections and download operations with **hardware-aware thread allocation**.

* `MAX_THREADS` – **Dynamically determined based on hardware: `max(2u, min(10u, thread::hardware_concurrency()))`**
  - **Minimum**: 2 threads (ensures basic concurrency)
  - **Maximum**: 10 threads (prevents resource exhaustion)
  - **Optimal**: Automatically detects CPU cores and adapts
* `thread_pool` – Vector storing worker thread objects.
* `thread_available` – Boolean array tracking thread availability status.
* `thread_mutex` – Mutex protecting thread pool access.
* `thread_cv` – Condition variable for thread synchronization.

**Hardware Thread Support:**
- **Single/Dual-core systems**: Uses 2 threads
- **Quad-core systems**: Uses 4 threads
- **8-core systems**: Uses 8 threads
- **10+ core systems**: Uses maximum 10 threads (capped for stability)

**Functions**

* `bool is_any_thread_available()` – Checks if any worker thread is currently free for assignment.
* `int get_available_thread()` – Returns index of first available thread and marks it busy.
* `void release_thread(int idx)` – Marks specified thread as available and notifies waiting operations.
* `void join_all_threads()` – Waits for all active threads to complete before shutdown.
* `void clear_thread_pool()` – Resets thread pool state and availability tracking.

---

### 3. Download Task Management

**DownloadTask Structure**
```cpp
struct DownloadTask {
    string result;              // Download status/result message
    mutex m;                   // Thread synchronization
    bool done;                 // Completion flag
    int total_pieces;          // Total pieces in file
    int completed_pieces;      // Successfully downloaded pieces
}
```

**Download Flow with Enhanced Piece Selection**
1. **Random Piece Selection** – Pieces are shuffled randomly to avoid hotspots.
2. **Round-Robin Seeder Assignment** – `piece_index % seeder_count` ensures load balancing.
3. **Concurrent Downloads** – Multiple threads download different pieces simultaneously.
4. **Progress Tracking** – Real-time updates on download completion status.

---

### 4. Server Flow (Peer-to-Peer)

```
start_as_listener()
├── Bind to specified IP:PORT
├── Listen for peer connections
└── server_listener()
    ├── Accept peer connection
    ├── assign_task_to_thread()
    └── handle_client()
        ├── Receive piece index request
        ├── Locate file piece using index
        ├── Send piece data to requesting peer
        └── Close connection
```

**Enhanced handle_client() Function:**
- Receives piece index from requesting peer
- Validates piece availability in local files
- Sends requested piece data using piece index for offset calculation
- Implements proper error handling and connection cleanup

---

### 5. Download Command Enhanced Flow

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

**Round-Robin Seeder Selection Example:**
- 2 seeders available: Seeder_A, Seeder_B
- Piece 0 → Seeder_A (0 % 2 = 0)
- Piece 1 → Seeder_B (1 % 2 = 1)  
- Piece 2 → Seeder_A (2 % 2 = 0)
- Piece 3 → Seeder_B (3 % 2 = 1)

---

### 6. Upload Command Enhanced Flow

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
├── **Tracker stores file information**
│   ├── Add file to group file list
│   ├── Add uploader as initial seeder
│   ├── Sync metadata to other trackers
│   └── Return upload success confirmation
└── Client ready to serve file pieces to peers
```

**Upload Validation Process:**
- **Step 1:** Client validates file exists locally and is readable
- **Step 2:** Tracker validates user permissions and file uniqueness
- **Step 3:** Client generates comprehensive metadata (full SHA + piece SHAs)
- **Step 4:** Tracker stores metadata and enables file sharing

**File Metadata Generation:**
- Full file SHA calculation for integrity verification
- Fixed piece size (512KB) for optimal transfer performance
- Individual piece SHA for each piece to ensure data integrity
- Seeder information (uploader IP:PORT) for peer-to-peer access

---

### 7. List Files Command Enhanced Flow

```
list_files <group_id>
├── Parse command (group_id)
├── Send list_files request to tracker
├── **Tracker processes file list request**
│   ├── Validate user is member of group
│   ├── Get all files in group from FileManager
│   └── For each file in group:
│       ├── **Check seeder availability**
│       │   ├── Get current seeder list for file
│       │   ├── Ping each seeder to verify connectivity
│       │   └── Filter out offline/unreachable seeders
│       ├── **Include file only if seeders available**
│       │   ├── If seeders > 0: Add file to response list
│       │   └── If seeders = 0: Skip file (not downloadable)
│       └── Include seeder count in file metadata
├── **Client receives filtered file list**
│   ├── Display only files with available seeders
│   ├── Show seeder count for each file
│   └── Hide files with zero active seeders
└── Return formatted file list to user
```

**Seeder Availability Check Process:**
- **Step 1:** Tracker validates user has permission to view group files
- **Step 2:** For each file, tracker checks current seeder list from FileManager
- **Step 3:** Tracker attempts connectivity check to each seeder (ping/socket test)
- **Step 4:** Only files with at least one reachable seeder are included in response
- **Step 5:** Client displays filtered list showing only downloadable files

**File List Response Format:**
```
Available Files in Group <group_id>:
- filename1.txt (2 seeders available)
- filename2.pdf (1 seeder available)
- filename3.doc (3 seeders available)

Note: Files with no active seeders are not shown
```

**Seeder Connectivity Validation:**
- Check seeder status using login map entries
- If seeder entry exists in login map, consider seeder as active/online
- If seeder not found in login map, consider seeder as offline/unavailable
- Remove offline seeders from active seeder list based on login map status
- Update seeder status in real-time during list operation by checking current login map

**Login Map Status Check Process:**
- Tracker maintains login map with active user sessions
- For each seeder in file's seeder list, check if seeder exists in login map
- Only seeders with active login sessions are considered available
- This ensures only currently logged-in users can serve file pieces

---

### 8. Stop Share Command Enhanced Flow

```
stop_share <group_id> <file_name>
├── Parse command (group_id, file_name)
├── Send stop_share request to tracker
├── **Tracker processes stop share request**
│   ├── Validate user is member of group
│   ├── Check if user is currently seeding the file
│   ├── Remove user from file's seeder list
│   └── **Check remaining seeders availability**
│       ├── Get updated seeder list for the file
│       ├── For each remaining seeder:
│       │   ├── Check if seeder exists in login map
│       │   └── Count active/online seeders
│       ├── **If no active seeders remain:**
│       │   ├── Remove file completely from group
│       │   ├── Delete file metadata from FileManager
│       │   └── Sync file removal to other trackers
│       └── **If other seeders available:**
│           ├── Keep file in group with updated seeder list
│           └── File remains downloadable from other seeders
├── **Client receives confirmation**
│   ├── Stop serving file pieces for this file
│   ├── Update local file sharing status
│   └── Display stop share result to user
└── Return operation status message
```

**Stop Share Validation Process:**
- **Step 1:** Tracker validates user has permission to stop sharing (user must be current seeder)
- **Step 2:** Remove requesting user from file's seeder list in FileManager
- **Step 3:** Check remaining seeders using login map status
- **Step 4:** If no active seeders remain, completely remove file from group
- **Step 5:** If other seeders available, keep file accessible with updated seeder list


**Stop Share Response Format:**
```
Case 1 - Other seeders available:
"Stop sharing successful. File remains available from 2 other seeders."

Case 2 - No other seeders:
"Stop sharing successful. File removed from group (no other seeders available)."

Case 3 - User not a seeder:
"Error: You are not currently seeding this file."
```

**File Removal Logic:**
- Only remove file if requesting user was the last active seeder
- Check login map status of all other seeders before removal
- Ensure file availability is maintained if other seeders exist
- Sync file removal across all tracker instances for consistency

---

### 9. Utility Functions (utils\_header.h / client\_utils.cpp)

Helper functions for validation, string operations, and file handling.

* `bool file_validation(const string& filepath)` – Verifies file existence and read permissions.
* `string read_file_by_line_number(const string& filepath, int line_number)` – Extracts specific line from text file.
* `bool string_address_validation(const string& line)` – Validates IP:PORT format in tracker entries.
* `bool ip_address_validation(const string& ip_address, const int port)` – Checks IP address and port range validity.
* `bool client_argument_validation(int argc, char *argv[])` – Validates command-line arguments for client startup.
* `void trim_whitespace(string &str)` – Removes leading and trailing whitespace characters.
* `void to_lowercase(string &str)` – Converts string to lowercase for case-insensitive comparisons.
* `void tokenize(const string& str, vector<string>& tokens)` – Splits string into tokens using space delimiter.

---

## Build & Run

### Compile
```bash
make clean
make
```

### Run
```bash
./client <IP>:<PORT> tracker_info.txt
```

* `tracker_info.txt` → File containing tracker addresses with IDs
* `<IP>:<PORT>` → Client listening address for peer connections

---

## Tracker Info File Format

Example `tracker_info.txt`:
```
127.0.0.1:9998    1
127.0.0.1:9999    2
```

---

## Supported Commands

### User Management
* `create user <user_id> <password>` – Register new user account with tracker
* `login <user_id> <password>` – Authenticate and start session with tracker

### Group Management  
* `create group <group_id>` – Create new group (user becomes owner)
* `join group <group_id>` – Request membership in existing group
* `leave group <group_id>` – Leave group you are member of
* `list groups` – Display all available groups in system
* `list requests <group_id>` – Show pending join requests (owner only)
* `accept request <group_id> <user_id>` – Accept join request (owner only)

### File Operations
* `upload_file <file_path> <group_id>` – Upload file to specified group
* `download_file <group_id> <file_name> <destination_path>` – Download file using piece-based transfer
* `list files <group_id>` – Show files available in group
* `show downloads` – Display active and completed downloads

### Session Management
* `logout` – End session and stop sharing files

---

## Key Features

### 1. **Concurrent Operations**
- Thread pool handles multiple peer connections simultaneously
- Parallel piece downloads from different seeders
- Non-blocking command processing during downloads

### 2. **Efficient Download Strategy** 
- Random piece selection prevents seeder hotspots
- Round-robin seeder assignment balances load
- Resume capability for interrupted downloads

### 3. **Fault Tolerance**
- Automatic tracker reconnection with session restoration
- Download retry mechanism for failed pieces
- Graceful handling of peer disconnections

### 4. **Peer-to-Peer Architecture**
- Direct peer connections for file piece transfer
- Eliminates central server bottleneck for data transfer
- Scalable performance with more peers



---

## Technical Details

### Communication Protocols

**1. Tracker Communication**
- TCP connection for command/response
- Length-prefixed message protocol
- Session management with login/logout
- File metadata exchange

**2. Peer Communication**
- Direct TCP connections between clients
- Binary piece transfer protocol
- Index-based piece requests
- Concurrent connections support

### Thread Management Architecture

```
Thread Pool (MAX_THREADS = 20)
├── Download threads (piece-based)
├── Upload/serving threads (peer requests)
├── Listener thread (accept connections)
└── Command processing thread
```

### File Management

**File Structure:**
- Files divided into fixed-size pieces
- SHA-based piece verification
- Concurrent piece assembly
- Atomic file writes with proper locking

**Download Algorithm:**
1. Shuffle piece indices randomly
2. Assign seeders using modulo operation
3. Create download threads for each piece
4. Write pieces to correct file offsets
5. Verify complete file integrity

---

## Error Handling

### Network Resilience
- Automatic tracker reconnection
- Peer connection timeout handling
- Socket buffer management
- Connection state validation

### Data Integrity
- Piece-level verification
- Download retry mechanisms
- File corruption detection
- Partial download recovery

### Thread Safety
- Mutex protection for shared resources
- Atomic operations for counters
- Condition variables for synchronization
- Exception handling in threaded operations

---

## System Limitations

### 1. **Technical Limitations**
- **No encryption support** - All communication is in plaintext including authentication
- **Limited NAT traversal** - Assumes direct peer connectivity without firewall restrictions
- **No bandwidth throttling** - Uses maximum available bandwidth which may impact network
- **Basic error recovery** - Limited retry logic and manual intervention required for some errors
- **Fixed thread pool size** - Maximum 20 concurrent threads with no dynamic scaling
- **Memory constraints** - Large files may consume significant memory for metadata storage

### 2. **Protocol Limitations**
- **No compression** - File transfers use raw binary data without compression algorithms
- **Limited message queuing** - Basic FIFO processing without prioritization
- **Maximum message size** - Limited to 1MB for tracker communication messages
- **No resume capability** - Downloads must restart from beginning if interrupted

### 3. **User Interface Limitations**
- **Command-line only** - No graphical interface or progress visualization
- **Basic input validation** - Limited sanitization and error handling for user input
- **No command history** - No auto-completion or previous command recall

### 4. **Network Limitations**
- **Single tracker per session** - Can only communicate with one tracker at a time
- **No load balancing** - Limited distribution of requests across multiple trackers
- **Connection timeout** - Fixed 10-second timeout for peer connections

---

## Testing Procedures and Sample Commands

### 1. **Basic Functionality Testing**

#### Start Client
```bash
# Terminal 1 - Start first client
./client 127.0.0.1:5000 tracker_info.txt

# Terminal 2 - Start second client  
./client 127.0.0.1:6000 tracker_info.txt
```

#### User Management Testing
```bash
# Create new user account
> create_user alice password123
Expected: User created successfully.

# Login with credentials
> login alice password123  
Expected: Login successful.

# Test duplicate user creation
> create_user alice newpass
Expected: User already exists.

# Test invalid login
> login bob wrongpass
Expected: Login failed.
```

#### Group Management Testing
```bash
# Create new group
> create_group project_team
Expected: Group created successfully.

# List available groups
> list_groups
Expected: Display all groups including project_team.

# Join existing group
> join_group project_team
Expected: Join request sent successfully.

# Accept join request (as group owner)
> list_requests project_team
> accept_request project_team alice
Expected: Request accepted successfully.
```

### 2. **File Operations Testing**

#### File Upload Testing
```bash
# Upload file to group
> upload_file project_team /home/user/document.pdf
Expected: File uploaded successfully.

# List files in group
> list_files project_team
Expected: Display document.pdf in file list.

# Test invalid file path
> upload_file project_team /nonexistent/file.txt
Expected: File does not exist error.
```

#### File Download Testing
```bash
# Download file from group
> download_file project_team document.pdf ./downloads/
Expected: Download started in background.

# Check download progress
> show_downloads
Expected: Display download status and progress.

# Download to specific file name
> download_file project_team document.pdf ./mydoc.pdf
Expected: File downloaded as mydoc.pdf.
```

### 3. **Multi-Client Testing**

#### Peer-to-Peer File Sharing
```bash
# Terminal 1 (User Alice)
> login alice pass123
> upload_file shared_group largefile.bin

# Terminal 2 (User Bob) 
> login bob pass456
> join_group shared_group
> download_file shared_group largefile.bin ./received_file.bin

# Expected: Bob downloads directly from Alice's client
```

#### Concurrent Operations
```bash
# Start multiple downloads simultaneously
> download_file group1 file1.txt ./file1_copy.txt
> download_file group1 file2.txt ./file2_copy.txt  
> download_file group2 file3.txt ./file3_copy.txt

# Check all download statuses
> show_downloads
Expected: Multiple downloads in progress with piece completion status.
```

### 4. **Error Handling Testing**

#### Network Failure Scenarios
```bash
# Test tracker disconnection (stop tracker during operation)
> download_file group1 largefile.bin ./test.bin
# Stop tracker process
Expected: Client attempts reconnection to backup tracker.

# Test peer disconnection (kill seeder during download)
> download_file group1 file.txt ./test.txt
# Kill seeder client process
Expected: Download continues from other available seeders.
```

#### Resource Limitation Testing  
```bash
# Test disk space (fill disk during download)
> download_file group1 hugefile.bin ./test.bin
Expected: Download fails with disk space error.

# Test invalid permissions
> download_file group1 file.txt /root/restricted.txt
Expected: Permission denied error.
```

### 5. **Performance Testing**

#### Large File Handling
```bash
# Test with large files (100MB+)
> upload_file testgroup /path/to/largefile.bin
> download_file testgroup largefile.bin ./downloaded_large.bin
Expected: Efficient piece-based transfer with progress updates.

# Test multiple seeders for same file
# Upload same file from multiple clients, then download
Expected: Faster download using round-robin seeder selection.
```

#### Stress Testing
```bash
# Start multiple clients and perform concurrent operations
# Client 1-5: Upload different files
# Client 6-10: Download files simultaneously
Expected: System handles concurrent operations without crashes.
```

### 6. **Session Management Testing**

#### Login/Logout Flow
```bash
# Test session persistence
> login alice password123
> logout
> login alice password123
Expected: Successful login after logout.

# Test automatic reconnection
> login alice password123
# Disconnect network briefly
Expected: Client reconnects and offers to re-login.
```

#### Exit Procedures
```bash
# Graceful exit during operations
> download_file group1 file.txt ./test.txt
> exit
Expected: Clean shutdown with proper resource cleanup.
```

---

## Performance Optimizations

### Network Efficiency
- Concurrent piece downloads using thread pool
- Round-robin load balancing across multiple seeders
- Socket buffer management with drain_socket()
- Connection timeout handling for peer connections

### Memory Management
- Efficient string operations with reserve() and resize()
- Shared pointers for automatic memory management
- Thread-safe operations with proper mutex usage
- Minimal memory copies in piece transfers

### I/O Operations
- Direct file positioning using lseek64() for large files
- Chunked reading/writing for better performance
- File locking with mutex for concurrent access
- Atomic file operations for data integrity

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

✅ **Completed Features:**
- Client startup and tracker connection
- Thread pool management
- User authentication and group management  
- File upload to tracker
- **Enhanced piece-based download with random selection**
- **Round-robin seeder assignment for load balancing**
- **Peer-to-peer file piece serving**
- Automatic tracker failover
- Interactive command interface


---
