#include "./utils_header.h"
#include "file_header.h"
#include <sys/stat.h>
#include <bits/stdc++.h>
#include <string>
#include <fcntl.h>
#include <arpa/inet.h>
#include <openssl/sha.h>
#define _FILE_OFFSET_BITS 64
using namespace std;

bool file_validation(const string& filepath) {
    struct stat file_state;

    if(stat(filepath.c_str(),&file_state)==-1){
        cerr << "File does not exist: " << filepath << "\n";
        return false;
    }

    if(file_state.st_size == 0) {
        cerr << "File is empty: " << filepath << "\n";
        return false;
    }

    return true;
}

bool ip_address_validation(const string& ip_address, const int port) {

    if (port < 1024 || port > 65535) {
        cerr << "Port number must be between 1024 and 65535.\n";
        return false;
    }

    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, ip_address.c_str(), &(sa.sin_addr));
    if(result == 0) {
        cerr << "Invalid IP address format: " << ip_address << "\n";
        return false;
    }
    else if (result < 0) { 
        perror("inet_pton failed");
        return false;
    }


    return true;
}

string read_file_by_line_number(const string& filepath, int line_number) {
    int fd = open(filepath.c_str(), O_RDONLY);
    if (fd == -1) {
        perror("open failed");
        return "";
    }

    string line;
    string current;
    char buffer[1024];
    ssize_t bytes_read;
    int current_line = 0;

    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        for (ssize_t i = 0; i < bytes_read; i++) {
            char c = buffer[i];

            if (c == '\n') {
                current_line++;
                if (current_line == line_number) {
                    close(fd);
                    return current;
                }
                current.clear();
            } else {
                current.push_back(c);
            }
        }
    }

    if (!current.empty()) {
        current_line++;
        if (current_line == line_number) {
            close(fd);
            return current;
        }
        current.clear();    
    }

    close(fd);
    return "";
}

bool string_address_validation(const string& line){
    size_t colon_pos = line.find(':');
        size_t space_pos = line.find(' ', colon_pos);
        if (colon_pos == string::npos || space_pos == string::npos) {
            return false;
        }

        string ip = line.substr(0, colon_pos);
        string port_str = line.substr(colon_pos + 1, space_pos - colon_pos - 1);
        string id_str = line.substr(space_pos + 1);

        if(!ip_address_validation(ip, stoi(port_str))) {
            return false;
        }
        return true;
}

bool tracker_file_entry_validation(const string& filepath) {
    int fd = open(filepath.c_str(), O_RDONLY);
    if (fd == -1) {
        perror("open failed");
        return false;
    }

    char buffer[1024];
    ssize_t bytes_read;
    string current;
    int line_number = 0;

    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        for (ssize_t i = 0; i < bytes_read; i++) {
            char c = buffer[i];

            if (c == '\n') {
                line_number++;
                if (!string_address_validation(current)) {
                    cerr << "Invalid entry in tracker file at line "<< line_number << ": " << current << "\n";
                    close(fd);
                    return false;
                }
                current.clear();
            } else {
                current.push_back(c);
            }
        }
    }


    if (!current.empty()) {
        line_number++;
        if (!string_address_validation(current)) {
            cerr << "Invalid entry in tracker file at line "
                 << line_number << ": " << current << "\n";
            close(fd);
            return false;
        }
    }

    close(fd);
    return true;
}

bool client_argument_validation(int argc, char *argv[]) {
    if(argc != 3) {
        cerr << "Usage: " << argv[0] << " <server_ip>:<server_port>  <tracker.txt>\n";
        return false;
    }

    if(!file_validation(argv[2])) {
        return false;
    }

    if(!tracker_file_entry_validation(argv[2])) {
        return false;
    }
    
    string address = argv[1];
    size_t colon_pos = address.find(':');
    if (colon_pos == string::npos) {
        cerr << "Invalid server address format. Expected <server_ip>:<server_port>\n";
        return false;
    }
    if(!ip_address_validation(address.substr(0, colon_pos), stoi(address.substr(colon_pos + 1)))) {
        return false;
    }

    return true;
}

void trim_whitespace(string &str){
    size_t start = str.find_first_not_of(" \n\r\t\0");
    size_t end = str.find_last_not_of(" \n\r\t\0");
    if (start == string::npos || end == string::npos) {
        str = "";
    } else {
        str = str.substr(start, end - start + 1);
    }
}

void to_lowercase(string &str){
    for (char &c : str) {
        c = tolower(c);
    }
}

void sanitize(string &s) {
    s.erase(remove(s.begin(), s.end(), '\0'), s.end());
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == string::npos) {
        s.clear();
        return;
    }
    size_t end = s.find_last_not_of(" \t\r\n");
    s = s.substr(start, end - start + 1);
}

void tokenize(const string& str, vector<string>& tokens){
    stringstream ss(str);
    string token;
    while (ss >> token) {
        sanitize(token);
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
}

bool validate_file_existence(const string& file_path){
    struct stat buffer;
    return (stat(file_path.c_str(), &buffer) == 0);
}

string calculate_SHA(const string& piece_data) {
    if (piece_data.empty()) {
        cerr << "Empty piece data, cannot calculate SHA\n";
        return "";
    }

    vector<char> buffer(piece_data.begin(), piece_data.end());

    // SHA256 calculation
    unsigned char piece_hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX piece_ctx;
    SHA256_Init(&piece_ctx);
    SHA256_Update(&piece_ctx, buffer.data(), buffer.size());
    SHA256_Final(piece_hash, &piece_ctx);

    // Convert hash to hex string
    char output[2 * SHA256_DIGEST_LENGTH + 1];
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(output + (i * 2), "%02x", piece_hash[i]);
    }
    output[2 * SHA256_DIGEST_LENGTH] = 0;

    return string(output);
}

// -------------------- Full file SHA --------------------
string calculate_full_file_SHA(const string& file_path) {
    int fd = open(file_path.c_str(), O_RDONLY);
    if (fd < 0) {
        perror(("open " + file_path).c_str());
        return "";
    }

    SHA256_CTX sha256;
    SHA256_Init(&sha256);

    const size_t BUF_SIZE = 512 * 1024; // 512 KB buffer
    vector<char> buffer(BUF_SIZE);

    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer.data(), BUF_SIZE)) > 0) {
        SHA256_Update(&sha256, buffer.data(), bytes_read);
    }

    if (bytes_read < 0) {
        perror(("read " + file_path).c_str());
        close(fd);
        return "";
    }

    close(fd);

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &sha256);

    char hex[2 * SHA256_DIGEST_LENGTH + 1];
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
        sprintf(hex + i * 2, "%02x", hash[i]);
    hex[2 * SHA256_DIGEST_LENGTH] = '\0';

    return string(hex);
}

// -------------------- Piece-wise SHA --------------------
vector<string> calculate_piece_SHA(const string& file_path, uint64_t piece_size) {
    vector<string> pieces;

    int fd = open(file_path.c_str(), O_RDONLY);
    if (fd < 0) {
        perror(("open " + file_path).c_str());
        return pieces;
    }

    vector<char> buffer(piece_size);
    ssize_t bytes_read;

    while ((bytes_read = read(fd, buffer.data(), piece_size)) > 0) {
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256, buffer.data(), bytes_read);

        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256_Final(hash, &sha256);

        char hex[2 * SHA256_DIGEST_LENGTH + 1];
        for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
            sprintf(hex + i * 2, "%02x", hash[i]);
        hex[2 * SHA256_DIGEST_LENGTH] = '\0';

        pieces.push_back(string(hex));
    }

    if (bytes_read < 0) perror(("read " + file_path).c_str());

    close(fd);
    return pieces;
}

// Generate FileInfo message
string generate_file_message(const string& command, const string& username, const string& ip, int port) {
    vector<string> tokens;
    stringstream ss(command);
    string token;
    while (ss >> token) tokens.push_back(token);

    if (tokens.size() != 3) return "";

    string file_path = tokens[2];
    FileInfo fileInfo;
    fileInfo.name = file_path.substr(file_path.find_last_of("/\\") + 1);
    fileInfo.path = file_path;

    char full_path[PATH_MAX];
    if (realpath(file_path.c_str(), full_path) != nullptr) {
        fileInfo.path = string(full_path);
    }
    fileInfo.owner = username;
    fileInfo.group = tokens[1];
    fileInfo.piece_size = 512 * 1024; // 512 KB

    struct stat st;
    if (stat(file_path.c_str(), &st) != 0) return "";
    fileInfo.size = st.st_size;

    fileInfo.full_SHA = calculate_full_file_SHA(file_path);
    fileInfo.piece_SHA = calculate_piece_SHA(file_path, fileInfo.piece_size);

    fileInfo.seeder_users[username] = Address{ip, port};

    fileInfo.user_file_map[username] = fileInfo.path;

    return fileInfo.toString();
}

bool create_file(const string& path, uint64_t size) {
    int fd = open(path.c_str(), O_CREAT | O_WRONLY, 0644);
    if (fd == -1) {
        return false;
    }
    if (ftruncate(fd, size) == -1) {
        close(fd);
        return false;
    }
    close(fd);
    return true;
}


