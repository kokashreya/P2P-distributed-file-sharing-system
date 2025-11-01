#pragma once
#ifndef UTILS_H
#define UTILS_H

#include <iostream>
#include <string>
#include <netinet/in.h>
#include <bits/stdc++.h>
using namespace std;

bool file_validation(const string& filepath);

string read_file_by_line_number(const string& filepath, int line_number);

bool string_address_validation(const string& line);

bool ip_address_validation(const string& ip_address, const int port);

bool client_argument_validation(int argc, char *argv[]);

void trim_whitespace(string &str);

void to_lowercase(string &str);

void tokenize(const string& str, vector<string>& tokens);

bool validate_file_existence(const string& file_path);

string calculate_SHA(const string& data);

string calculate_full_file_SHA(const string& file_path);

string generate_file_message(const string& command, const string& username, const string& ip, int port);

bool create_file(const string& path, uint64_t size);



#endif
