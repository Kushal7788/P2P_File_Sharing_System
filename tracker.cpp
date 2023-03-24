

// inet_addr
#include <arpa/inet.h>

// For threading, link with lpthread
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <bits/stdc++.h>

using namespace std;

// Semaphore variables
sem_t x, y;
pthread_t tid;
// pthread_t writerthreads[100];
// pthread_t readerthreads[100];
int server_socket, client_socket;
struct sockaddr_in server_addr;
// struct sockaddr_storage serverStorage;
socklen_t addr_size;

class User
{
public:
    string user_id, password;
    bool isLogin;
    string ip_addr, port;
};

class FileSystem
{
public:
    string file_name;
    long long file_size;
    string sha1;
    map<string, pair<bool, string>> userWithFile;
    // vector<map<int,vector<User *>>> filespieceswithusers
};

class Group
{
public:
    string grp_id;
    User *grp_owner;
    vector<User *> grp_members;
    vector<User *> pending_members;
    vector<FileSystem *> sharable_files;
};
// grp->sharable_files[0]->userWithFile[0].first

unordered_map<string, User *> user_map;
unordered_map<string, Group *> group_map;

vector<string> lineToString(string cmd_line)
{
    vector<string> args;
    string temp;
    for (int i = 0; i < cmd_line.size(); i++)
    {
        if (cmd_line[i] == ' ')
        {
            args.push_back(temp);
            temp = "";
        }
        else
        {
            temp += cmd_line[i];
        }
    }
    args.push_back(temp);
    return args;
}

string executeCommand(vector<string> args, int client_sock, User *&session_owner)
{
    string ret_val = "";

    if (args[0] == "create_user")
    {
        if (user_map.find(args[1]) != user_map.end())
        {
            ret_val = "User already exists";
            return ret_val;
        }
        else
        {
            User *new_user = new User();
            new_user->user_id = args[1];
            new_user->password = args[2];
            new_user->isLogin = false;
            user_map[args[1]] = new_user;
            ret_val = "New User created";
            return ret_val;
        }
    }

    else if (args[0] == "login")
    {
        if (session_owner && session_owner->isLogin)
        {
            ret_val = "Someone is already logged in. Please logout to login again";
            return ret_val;
        }
        else
        {
            if (user_map.find(args[1]) == user_map.end())
            {
                ret_val = "User dosen't exists";
                return ret_val;
            }
            else
            {
                User *curr_user = user_map[args[1]];
                if (curr_user->isLogin)
                {
                    ret_val = "User Already Logged in from another device";
                    return ret_val;
                }
                if (curr_user->password != args[2])
                {
                    ret_val = "Incorrect Password";
                    return ret_val;
                }
                else
                {
                    session_owner = curr_user;
                    session_owner->isLogin = true;
                    session_owner->ip_addr = args[3];
                    session_owner->port = args[4];
                    ret_val = "Login Successful";
                    return ret_val;
                }
            }
        }
    }

    else if (args[0] == "create_group")
    {
        if (session_owner && session_owner->isLogin)
        {
            if (group_map.find(args[1]) != group_map.end())
            {
                ret_val = "Group Already Exists";
                return ret_val;
            }
            else
            {
                Group *new_group = new Group();
                new_group->grp_id = args[1];
                new_group->grp_owner = session_owner;
                new_group->grp_members.push_back(session_owner);
                group_map[args[1]] = new_group;
                ret_val = "Group Created Successfully";
                return ret_val;
            }
        }
        else
        {
            ret_val = "User not logged in";
            return ret_val;
        }
    }

    else if (args[0] == "join_group")
    {
        if (session_owner && session_owner->isLogin)
        {
            if (group_map.find(args[1]) == group_map.end())
            {
                ret_val = "Group does not exists";
                return ret_val;
            }
            else
            {
                Group *curr_group = group_map[args[1]];
                bool user_exists = false;
                for (int k = 0; k < curr_group->grp_members.size(); k++)
                {
                    if (curr_group->grp_members[k]->user_id == session_owner->user_id)
                    {
                        user_exists = true;
                        break;
                    }
                }
                for (int k = 0; k < curr_group->pending_members.size(); k++)
                {
                    if (curr_group->pending_members[k]->user_id == session_owner->user_id)
                    {
                        user_exists = true;
                        break;
                    }
                }
                if (user_exists)
                {
                    ret_val = "User already exists in the group";
                    return ret_val;
                }
                else
                {
                    curr_group->pending_members.push_back(session_owner);
                    ret_val = "Join Request sent to group owner";
                    return ret_val;
                }
            }
        }
        else
        {
            ret_val = "User not logged in";
            return ret_val;
        }
    }

    else if (args[0] == "leave_group")
    {
        if (session_owner && session_owner->isLogin)
        {
            if (group_map.find(args[1]) == group_map.end())
            {
                ret_val = "Group does not exists";
                return ret_val;
            }
            else
            {
                Group *curr_group = group_map[args[1]];
                bool user_exists = false;
                if (curr_group->grp_owner->user_id == session_owner->user_id)
                {
                    group_map.erase(curr_group->grp_id);
                    ret_val = "Admin removed and group deleted successfully";
                    return ret_val;
                }
                for (int k = 0; k < curr_group->grp_members.size(); k++)
                {
                    if (curr_group->grp_members[k]->user_id == session_owner->user_id)
                    {
                        curr_group->grp_members.erase(curr_group->grp_members.begin() + k);
                        user_exists = true;
                        break;
                    }
                }
                for (int k = 0; k < curr_group->pending_members.size(); k++)
                {
                    if (curr_group->pending_members[k]->user_id == session_owner->user_id)
                    {
                        curr_group->pending_members.erase(curr_group->pending_members.begin() + k);
                        user_exists = true;
                        break;
                    }
                }

                if (user_exists)
                {
                    ret_val = "User removed successfully";
                    return ret_val;
                }
                else
                {
                    ret_val = "User dosen't exists in the given group";
                    return ret_val;
                }
            }
        }
        else
        {
            ret_val = "User not logged in";
            return ret_val;
        }
    }

    else if (args[0] == "list_requests")
    {
        if (session_owner && session_owner->isLogin)
        {
            if (group_map.find(args[1]) == group_map.end())
            {
                ret_val = "Group does not exists";
                return ret_val;
            }
            else
            {
                Group *curr_group = group_map[args[1]];
                if (curr_group->grp_owner->user_id == session_owner->user_id)
                {
                    ret_val = "Pending requests are \n";
                    for (int k = 0; k < curr_group->pending_members.size(); k++)
                    {
                        ret_val += curr_group->pending_members[k]->user_id + "\n";
                    }
                    return ret_val;
                }
                else
                {
                    ret_val = "Only Group Owner can perform this operation";
                    return ret_val;
                }
            }
        }
        else
        {
            ret_val = "User not logged in";
            return ret_val;
        }
    }

    else if (args[0] == "accept_request")
    {
        if (session_owner && session_owner->isLogin)
        {
            if (group_map.find(args[1]) == group_map.end())
            {
                ret_val = "Group does not exists";
                return ret_val;
            }
            else
            {
                Group *curr_group = group_map[args[1]];
                if (curr_group->grp_owner->user_id == session_owner->user_id)
                {
                    if (user_map.find(args[2]) != user_map.end())
                    {
                        User *accept_user = user_map[args[2]];
                        bool user_exists = false;
                        for (int k = 0; k < curr_group->pending_members.size(); k++)
                        {
                            if (curr_group->pending_members[k]->user_id == accept_user->user_id)
                            {
                                curr_group->pending_members.erase(curr_group->pending_members.begin() + k);
                                curr_group->grp_members.push_back(accept_user);
                                user_exists = true;
                                break;
                            }
                        }
                        if (user_exists)
                        {
                            ret_val = "User added into group successfully";
                            return ret_val;
                        }
                        else
                        {
                            ret_val = "User dosen't exists in the given group";
                            return ret_val;
                        }
                    }
                    else
                    {
                        ret_val = "User dosent exists for given user_id";
                        return ret_val;
                    }
                }
                else
                {
                    ret_val = "Only Group Owner can perform this operation";
                    return ret_val;
                }
            }
        }
        else
        {
            ret_val = "User not logged in";
            return ret_val;
        }
    }

    else if (args[0] == "list_groups")
    {
        if (session_owner && session_owner->isLogin)
        {
            ret_val = "Groups list \n";
            for (auto k : group_map)
            {
                ret_val += k.first + "\n";
            }
            return ret_val;
        }
        else
        {
            ret_val = "User not logged in";
            return ret_val;
        }
    }

    else if (args[0] == "logout")
    {
        if (session_owner && session_owner->isLogin)
        {
            session_owner->isLogin = false;
            session_owner->ip_addr = "";
            session_owner->port = "";
            session_owner = NULL;
            ret_val = "User logged out successfully";
            return ret_val;
        }
        else
        {
            ret_val = "User not logged in";
            return ret_val;
        }
    }

    else if (args[0] == "upload_file")
    {
        if (session_owner && session_owner->isLogin)
        {
            if (group_map.find(args[2]) == group_map.end())
            {
                ret_val = "Group does not exists";
                return ret_val;
            }
            else
            {
                Group *curr_group = group_map[args[2]];
                bool user_exists = false;
                for (int k = 0; k < curr_group->grp_members.size(); k++)
                {
                    if (curr_group->grp_members[k]->user_id == session_owner->user_id)
                    {
                        user_exists = true;
                        break;
                    }
                }

                if (user_exists)
                {
                    bool file_exist = false;
                    FileSystem *file_obj = NULL;
                    for (int k = 0; k < curr_group->sharable_files.size(); k++)
                    {
                        if (curr_group->sharable_files[k]->file_name == args[3])
                        {
                            file_exist = true;
                            file_obj = curr_group->sharable_files[k];
                            break;
                        }
                    }
                    if (file_exist)
                    {
                        file_obj->userWithFile[session_owner->user_id].first = true;
                        file_obj->userWithFile[session_owner->user_id].second = args[1];
                    }
                    else
                    {
                        file_obj = new FileSystem();
                        file_obj->file_name = args[3];
                        file_obj->file_size = stoll(args[4]);
                        file_obj->sha1 = args[5];
                        file_obj->userWithFile[session_owner->user_id].first = true;
                        file_obj->userWithFile[session_owner->user_id].second = args[1];
                        curr_group->sharable_files.push_back(file_obj);
                    }

                    ret_val = "User shared file successfully";
                    return ret_val;
                }
                else
                {
                    ret_val = "User dosen't exists in the given group";
                    return ret_val;
                }
            }
        }
        else
        {
            ret_val = "User not logged in";
            return ret_val;
        }
    }

    else if (args[0] == "list_files")
    {
        if (session_owner && session_owner->isLogin)
        {
            if (group_map.find(args[1]) == group_map.end())
            {
                ret_val = "Group does not exists";
                return ret_val;
            }
            else
            {
                Group *curr_group = group_map[args[1]];
                bool user_exists = false;
                for (int k = 0; k < curr_group->grp_members.size(); k++)
                {
                    if (curr_group->grp_members[k]->user_id == session_owner->user_id)
                    {
                        user_exists = true;
                        break;
                    }
                }

                if (user_exists)
                {
                    ret_val = "Files shared in the groups are \n";
                    for (int i = 0; i < curr_group->sharable_files.size(); i++)
                    {
                        ret_val += curr_group->sharable_files[i]->file_name + "\n";
                    }
                    return ret_val;
                }
                else
                {
                    ret_val = "User dosen't exists in the given group";
                    return ret_val;
                }
            }
        }
        else
        {
            ret_val = "User not logged in";
            return ret_val;
        }
    }

    else if (args[0] == "stop_share")
    {
        if (session_owner && session_owner->isLogin)
        {
            if (group_map.find(args[1]) == group_map.end())
            {
                ret_val = "Group does not exists";
                return ret_val;
            }
            else
            {
                Group *curr_group = group_map[args[1]];
                bool user_exists = false;
                for (int k = 0; k < curr_group->grp_members.size(); k++)
                {
                    if (curr_group->grp_members[k]->user_id == session_owner->user_id)
                    {
                        user_exists = true;
                        break;
                    }
                }

                if (user_exists)
                {
                    bool file_exists_in_group = false;
                    for (int i = 0; i < curr_group->sharable_files.size(); i++)
                    {
                        if (curr_group->sharable_files[i]->file_name == args[2])
                        {
                            file_exists_in_group = true;
                            map<string, pair<bool, string>> user_share_map = curr_group->sharable_files[i]->userWithFile;
                            if (user_share_map.find(session_owner->user_id) != user_share_map.end())
                            {
                                if (user_share_map[session_owner->user_id].first)
                                {
                                    user_share_map[session_owner->user_id].first = false;
                                    ret_val = "User has stopped sharing the file";
                                    return ret_val;
                                }
                                else
                                {
                                    ret_val = "User is not sharing the file";
                                    return ret_val;
                                }
                            }
                            else
                            {
                                ret_val = "User has not uploaded the file";
                                return ret_val;
                            }
                        }
                    }
                    if (!file_exists_in_group)
                    {
                        ret_val = "File does not exists in this group";
                        return ret_val;
                    }
                }
                else
                {
                    ret_val = "User dosen't exists in the given group";
                    return ret_val;
                }
            }
        }
        else
        {
            ret_val = "User not logged in";
            return ret_val;
        }
    }

    else if (args[0] == "download_file")
    {
        if (session_owner && session_owner->isLogin)
        {
            if (group_map.find(args[1]) == group_map.end())
            {
                ret_val = "Group does not exists";
                return ret_val;
            }
            else
            {
                Group *curr_group = group_map[args[1]];
                bool user_exists = false;
                for (int k = 0; k < curr_group->grp_members.size(); k++)
                {
                    if (curr_group->grp_members[k]->user_id == session_owner->user_id)
                    {
                        user_exists = true;
                        break;
                    }
                }

                if (user_exists)
                {
                    bool file_exists_in_group = false;
                    FileSystem *file_obj = NULL;
                    for (int i = 0; i < curr_group->sharable_files.size(); i++)
                    {
                        if (curr_group->sharable_files[i]->file_name == args[2])
                        {
                            file_exists_in_group = true;
                            file_obj = curr_group->sharable_files[i];
                        }
                    }
                    if (file_exists_in_group)
                    {
                        ret_val = "online " + to_string(file_obj->file_size);
                        string ip_details = "";
                        for (auto k : file_obj->userWithFile)
                        {
                            if (k.second.first == true && user_map[k.first]->isLogin)
                            {
                                ip_details += user_map[k.first]->ip_addr + ":" + user_map[k.first]->port + " ";
                                ip_details += k.second.second + " ";
                            }
                        }
                        if (ip_details == "")
                        {
                            ret_val = "No seeders available";
                            return ret_val;
                        }
                        else
                        {
                            ret_val += " " + ip_details;
                            return ret_val;
                        }
                    }
                    else
                    {
                        ret_val = "File does not exists in this group";
                        return ret_val;
                    }
                }
                else
                {
                    ret_val = "User dosen't exists in the given group";
                    return ret_val;
                }
            }
        }
        else
        {
            ret_val = "User not logged in";
            return ret_val;
        }
    }

    else
    {
        ret_val = "New User created";
        return ret_val;
    }
}

void *handleClientConnection(void *param)
{
    int nsoc = *((int *)param);
    User *session_owner = NULL;
    while (1)
    {
        char buffer[20000];
        int bytes = recv(nsoc, buffer, 20000, 0);
        if (bytes == 0)
        {
            cout << nsoc << " shut down!\n";
            break;
        }
        cout << "Received from " << nsoc << ": " << buffer << endl;
        string cmd = buffer;
        vector<string> args = lineToString(cmd);

        for (int i = 0; i < 20000; i++)
        {
            buffer[i] = '\0';
        }

        string response = executeCommand(args, nsoc, session_owner);
        strcpy(buffer, response.c_str());
        send(nsoc, buffer, strlen(buffer), 0);

        for (int i = 0; i < 20000; i++)
        {
            buffer[i] = '\0';
        }
    }

    pthread_exit(NULL);
}

pair<string, string> getIpPort(string ip_port)
{
    string ip_addr, port;
    for (int i = 0; i < ip_port.size(); i++)
    {
        if (ip_port[i] != ':')
        {
            ip_addr += ip_port[i];
        }
        else
        {
            port = ip_port.substr(i + 1, ip_port.size() - i + 1);
            break;
        }
    }
    return {ip_addr, port};
}

void makeMainServer(string ip_addr, int port)
{
    int opt = 1;

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        cout << "\n Socket creation error \n";
        exit(1);
    }

    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        cout << "\n Set Sockopt Error\n";
        exit(1);
    }

    if (inet_pton(AF_INET, &ip_addr[0], &server_addr.sin_addr) <= 0)
    {
        cout << "\nInvalid address/ Address not supported \n";
        exit(1);
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    // Bind the socket to the
    // address and port number.
    bind(server_socket,
         (struct sockaddr *)&server_addr,
         sizeof(server_addr));

    // Listen on the socket,
    // with 40 max connection
    // requests queued
    if (listen(server_socket, 50) == 0)
        cout << "Listening\n";
    else
        cout << "Error\n";

    // close(server_socket);
}

// Driver Code
int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        cout << "Invalid Arguments passed\n";
        exit(1);
    }

    // Read from file
    string tracker_details;
    ifstream MyReadFile(argv[1]);
    getline(MyReadFile, tracker_details);

    // Tracker Server socket creation
    pair<string, string> ip_port = getIpPort(tracker_details);
    makeMainServer(ip_port.first, stoi(ip_port.second));

    // Array for thread
    pthread_t tid[60];

    int i = 0;

    while (1)
    {
        struct sockaddr_storage serverStorage;

        addr_size = sizeof(serverStorage);

        client_socket = accept(server_socket,
                               (struct sockaddr *)&serverStorage,
                               &addr_size);

        pthread_t *thread = new pthread_t();

        if (pthread_create(thread, NULL, handleClientConnection, &client_socket) != 0)
        {
            cout << "Failed to create thread\n";
        }
    }

    return 0;
}
