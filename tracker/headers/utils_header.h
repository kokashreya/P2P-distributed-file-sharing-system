#pragma once
#ifndef UTILS_H
#define UTILS_H
#include "../managers/manager.h"
#include <iostream>
#include <string>
#include <netinet/in.h>
#include <vector>
using namespace std;

bool file_validation(const string& filepath);

string read_file_by_line_number(const string& filepath, int line_number);

bool string_address_validation(const string& line);

Address get_address(const string& line);

bool ip_address_validation(const string& ip_address, const int port);

bool client_argument_validation(int argc, char *argv[]);

bool tracker_argument_validation(int argc, char *argv[]);

int no_entries_in_file(const string& filepath);

void trim_whitespace(string &str);

void to_lowercase(string &str);

void tokenize(const string& str, vector<string>& tokens);

#endif
