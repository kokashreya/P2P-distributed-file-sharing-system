#include "manager.h"

bool GroupManager::createGroup(const string &owner,const string &group_name)
{
    lock_guard<mutex> lock(group_mtx);
    if (groups.count(group_name))
        return false;
    GroupInfo g;
    g.owner = owner;
    g.members.push_back(owner);
    groups[group_name] = g;
    owner_groups[owner].insert(group_name);
    return true;
}

string GroupManager::getOwnerName(const string &group_name){
    lock_guard<mutex> lock(group_mtx);
    if (!groups.count(group_name))
        return "";

    auto g=groups[group_name];
    return g.owner;
}

bool GroupManager::isGroupAvailabel(const string &group_name){
    lock_guard<mutex> lock(group_mtx);
    return groups.count(group_name);
}

vector<string> GroupManager::groupList() {
    lock_guard<std::mutex> lock(group_mtx);
    vector<string> result;

    for (const auto &entry : groups) {
        result.push_back(entry.first);
    }

    return result;
}

bool GroupManager::requestToJoin( const string &user,const string &group_name)
{
    lock_guard<mutex> lock(group_mtx);
    if (!groups.count(group_name))
        return false;
    auto &g = groups[group_name];
    if (find(g.members.begin(), g.members.end(), user) != g.members.end())
        return false;
    g.pending.insert(user);
    return true;
}

bool GroupManager::isMemberOfGroup(const string &user,const string &group_name){
    lock_guard<mutex> lock(group_mtx);
    auto &g = groups[group_name];
    if (find(g.members.begin(), g.members.end(), user) != g.members.end()){
        return true;
    }
    
    return false;

}

unordered_map<string, unordered_set<string>> GroupManager::showPendingRequests(const string &user, const string &group_name="all")
{
    lock_guard<mutex> lock(group_mtx);
    unordered_map<string, unordered_set<string>>  result;
    if(owner_groups.find(user) == owner_groups.end()) {
        return result;
    }

    if(groups.find(group_name) == groups.end()) {
        return result;
    }
    if (group_name == "all") {
        for (const auto& g_name : owner_groups.at(user)) {
            if (groups.count(g_name)) {
                const auto& g_info = groups.at(g_name);
                if (g_info.pending.count(user)) {
                    result[g_name] = unordered_set<string>(g_info.pending.begin(), g_info.pending.end());
                }
            }
        }
    } else {
        const auto& g = groups.at(group_name);
        result[group_name] = unordered_set<string>(g.pending.begin(), g.pending.end());
        return result;
    }

    return result;      
}

bool GroupManager::isGroupOwner(const string &user, const string &group_name){
    
    lock_guard<mutex> lock(group_mtx); 
    auto it = owner_groups.find(user);
    if (it == owner_groups.end()) {
        return false;
    }
    const auto& groups_set = it->second;
    return groups_set.find(group_name) != groups_set.end();
}

bool GroupManager::approveRequest(const string &approver, const string &user,const string &group_name)
{
    lock_guard<mutex> lock(group_mtx);
    if (!groups.count(group_name))
        return false;
    auto &g = groups[group_name];
    if (g.owner != approver)
    {
        return false;
    }
    if (!g.pending.count(user))
    {
        return false;
    }
    g.pending.erase(user);
    g.members.push_back(user);
    return true;
}

bool GroupManager::isPenddingRequest(const string &user,const string &group_name){

    lock_guard<mutex> lock(group_mtx);
    if (!groups.count(group_name))
        return false;
    auto &g = groups[group_name];

    if (!g.pending.count(user))
    {
        return false;
    }
    return true;

}

bool GroupManager::leaveGroup(const string &user,const string &group_name)
{
    lock_guard<mutex> lock(group_mtx);
    if (!groups.count(group_name))
        return false;
    auto &g = groups[group_name];
    auto it = find(g.members.begin(), g.members.end(), user);
    if (it == g.members.end())
        return false;

    bool wasOwner = (user == g.owner);
    g.members.erase(it);
    
    

    if (g.members.empty())
    {
        auto &ownerGroupSet = owner_groups[g.owner];
        ownerGroupSet.erase(group_name);
        groups.erase(group_name); // delete group
        
    }
    else if (wasOwner)
    {
        // Remove group_name from the previous owner's group list
        auto &ownerGroupSet = owner_groups[g.owner];
        ownerGroupSet.erase(group_name);
        if (ownerGroupSet.empty()) {
            owner_groups.erase(g.owner);
        }

        owner_groups[g.members.front()].insert(group_name);
        g.owner = g.members.front(); // new owner = next member
    }
    return true;
}

void GroupManager::showGroup(const string &group_name)
{
    lock_guard<mutex> lock(group_mtx);
    if (!groups.count(group_name))
    {
        cout << "Group not found\n";
        return;
    }
    const auto &g = groups.at(group_name);
    cout << "Group: " << group_name << "\n";
    cout << "Owner: " << g.owner << "\n";
    cout << "Members: ";
    for (auto &m : g.members)
        cout << m << " ";
    cout << "\nPending: ";
    for (auto &p : g.pending)
        cout << p << " ";
    cout << "\n";
}



