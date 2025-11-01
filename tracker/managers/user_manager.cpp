#include "manager.h"

bool UserManager::registerUser(const string &username, const string &password)
{
    lock_guard<std::mutex> lock(mtx);
    if (users.count(username))
        return false;
    users[username] = password;

    return true;
}

bool UserManager:: isUser(const string &username){
    lock_guard<std::mutex> lock(mtx);
    return users.count(username);
}

bool UserManager::login(const string &username, const string &password, const Address &addr)
{
    lock_guard<std::mutex> lock(mtx);   
    if (!users.count(username))
        return false;
    if (users.at(username) != password)
        return false;
    logged_in[username] = addr;
    return true;
}

bool UserManager::logout(const string &username)
{
    lock_guard<std::mutex> lock(mtx);
    logged_in.erase(username);
    return true;
}

bool UserManager::isLoggedIn(const string &username)
{
    lock_guard<std::mutex> lock(mtx);
    int c=logged_in.count(username);
    return c;
}

void UserManager::showLoggedInUsers()
{
    lock_guard<std::mutex> lock(mtx);
    cout << "Currently logged in users:\n";
    for (auto &[user, addr] : logged_in)
    {
        cout << "  " << user << " -> " << addr.ip << ":" << addr.port << "\n";
    }
}

bool UserManager::getUserAddress(const string &username, Address &addr)
{
    lock_guard<std::mutex> lock(mtx);
    if (logged_in.count(username))
    {
        addr = logged_in[username];
        return true;
    }
    return false;
}