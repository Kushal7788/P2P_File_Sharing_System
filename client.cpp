#include <sys/socket.h>
#include <unistd.h>
#include <fstream>
#include <arpa/inet.h>
#include <bits/stdc++.h>
#include <sys/stat.h>
// For threading, link with lpthread
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <openssl/sha.h>

#define BLOCKSIZE 65536

using namespace std;

int client_socket;
struct sockaddr_in server_addr;
socklen_t addr_size;
string server_ip;
string server_port;
vector<pair<string, string>> downloaded_files;

// struct sockaddr_storage serverStorage;

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

void connectTracker(int &tracker_socket, struct sockaddr_in &tracker_addr, int port, string ip_addr)
{

    if ((tracker_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        cout << "\n Socket creation error \n";
        return;
    }

    if (inet_pton(AF_INET, &ip_addr[0], &tracker_addr.sin_addr) <= 0)
    {
        cout << "\nInvalid address/ Address not supported \n";
        return;
    }
    tracker_addr.sin_family = AF_INET;
    tracker_addr.sin_port = htons(port);

    if (connect(tracker_socket, (struct sockaddr *)&tracker_addr, sizeof(tracker_addr)) < 0)
    {
        cout << "\nConnection Failed \n";
        return;
    }
}

void sendToTracker(string msg, int tracker_socket)
{
    send(tracker_socket, &msg[0], msg.size(), 0);
}

bool validateCommand(vector<string> args)
{

    if (args[0] == "create_user" || args[0] == "login" || args[0] == "accept_request" ||
        args[0] == "upload_file" || args[0] == "stop_share")
    {
        if (args.size() != 3)
        {
            cout << "Invalid Command\n";
            return false;
        }
    }
    else if (args[0] == "download_file")
    {
        if (args.size() != 4)
        {
            cout << "Invalid Command\n";
            return false;
        }
    }
    else if (args[0] == "create_group" || args[0] == "join_group" || args[0] == "leave_group" || args[0] == "list_files" || args[0] == "list_requests")
    {
        if (args.size() != 2)
        {
            cout << "Invalid Command\n";
            return false;
        }
    }
    else if (args[0] == "show_downloads" || args[0] == "logout" || args[0] == "list_groups")
    {
        if (args.size() != 1)
        {
            cout << "Invalid Command\n";
            return false;
        }
    }
    else
    {
        cout << "Invalid Command\n";
        return false;
    }
    return true;
}

string getFileName(string path)
{
    string fname;
    for (int i = path.size() - 1; i >= 0; i--)
    {
        if (path[i] == '/')
        {
            break;
        }
        else
        {
            fname += path[i];
        }
    }
    reverse(fname.begin(), fname.end());
    return fname;
}

long long getFileSize(string path)
{
    FILE *fp = fopen(&path[0], "r");
    if (fp == NULL)
    {
        cout << "File Not Found!\n";
        return -1;
    }
    fseek(fp, 0L, SEEK_END);
    long int res = ftell(fp);
    fclose(fp);
    return res;
}

bool uploadFileCmd(vector<string> &args)
{
    string fname = getFileName(args[1]);
    long long fsize = getFileSize(args[1]);
    if (fsize != -1)
    {
        int last_block_size = BLOCKSIZE;
        int total_blocks = 0;
        if (fsize % BLOCKSIZE == 0)
        {
            total_blocks = fsize / BLOCKSIZE;
        }
        else
        {
            total_blocks = fsize / BLOCKSIZE;
            total_blocks++;
            last_block_size = fsize % BLOCKSIZE;
        }

        int fd = open(args[1].c_str(), O_RDONLY);

        string hash_val = "";

        for (int k = 0; k < total_blocks; k++)
        {
            if (k != total_blocks - 1)
            {
                unsigned char *buf = new unsigned char[BLOCKSIZE];
                read(fd, buf, BLOCKSIZE);

                unsigned char temp[SHA_DIGEST_LENGTH];
                char sha_str[SHA_DIGEST_LENGTH * 2];

                memset(sha_str, 0x0, SHA_DIGEST_LENGTH * 2);
                memset(temp, 0x0, SHA_DIGEST_LENGTH);

                SHA1((unsigned char *)buf, BLOCKSIZE, temp);

                for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
                {
                    sprintf((char *)&(sha_str[i * 2]), "%02x", temp[i]);
                }

                string curr_hash = sha_str;
                hash_val += curr_hash;

                free(buf);
            }
            else
            {
                unsigned char *buf = new unsigned char[last_block_size];
                read(fd, buf, last_block_size);

                unsigned char temp[SHA_DIGEST_LENGTH];
                char sha_str[SHA_DIGEST_LENGTH * 2];

                memset(sha_str, 0x0, SHA_DIGEST_LENGTH * 2);
                memset(temp, 0x0, SHA_DIGEST_LENGTH);
                SHA1((unsigned char *)buf, last_block_size, temp);

                for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
                {
                    sprintf((char *)&(sha_str[i * 2]), "%02x", temp[i]);
                }

                string curr_hash = sha_str;
                hash_val += curr_hash;

                free(buf);
            }
        }
        if (hash_val.size() > 19000)
        {
            hash_val.substr(0, 19000);
        }
        args.push_back(fname);
        args.push_back(to_string(fsize));
        args.push_back(hash_val);
        return true;
    }
    else
    {
        return false;
    }

    // unsigned char temp[SHA_DIGEST_LENGTH];
    // char buf[SHA_DIGEST_LENGTH * 2];
    // memset(buf, 0x0, SHA_DIGEST_LENGTH * 2);
    // memset(temp, 0x0, SHA_DIGEST_LENGTH);

    // SHA1((unsigned char *)s.c_str(), s.size(), temp);
}

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

bool receiveFromTracker(int tracker_socket)
{
    char buffer[10000];
    int bytes = recv(tracker_socket, buffer, 10000, 0);
    if (!bytes)
    {
        cout << "tracker shut down!\n";
        return false;
    }
    cout << buffer << endl;
    for (int i = 0; i < 10000; i++)
    {
        buffer[i] = '\0';
    }
    return true;
}

void *uploadToPeer(void *param)
{
    int nsoc = *((int *)param);
    char buffer[10000];
    int bytes = recv(nsoc, buffer, 10000, 0);
    if (!bytes)
    {
        cout << nsoc << " shut down!\n";
        pthread_exit(NULL);
    }
    string file_path = buffer;

    string fname = getFileName(file_path);
    long long fsize = getFileSize(file_path);
    if (fsize != -1)
    {
        int last_block_size = BLOCKSIZE;
        int total_blocks = 0;
        if (fsize % BLOCKSIZE == 0)
        {
            total_blocks = fsize / BLOCKSIZE;
        }
        else
        {
            total_blocks = fsize / BLOCKSIZE;
            total_blocks++;
            last_block_size = fsize % BLOCKSIZE;
        }

        int fd = open(file_path.c_str(), O_RDONLY);
        char *dummy_buf = new char[10];
        char *buf_last_piece = new char[last_block_size];
        char *buf_norm = new char[BLOCKSIZE];
        for (int j = 0; j < BLOCKSIZE; j++)
        {
            buf_norm[j] = '\0';
        }
        for (int j = 0; j < last_block_size; j++)
        {
            buf_last_piece[j] = '\0';
        }
        ifstream infile(file_path, ios::in);
        for (int k = 0; k < total_blocks; k++)
        {
            if (k != total_blocks - 1)
            {
                infile.read(buf_norm, BLOCKSIZE);
                write(nsoc, buf_norm, BLOCKSIZE);

                int bytes = read(nsoc, dummy_buf, 10);
                if (!bytes)
                {
                    cout << nsoc << " shut down!\n";
                }
                for (int m = 0; m < BLOCKSIZE; m++)
                {
                    buf_norm[m] = '\0';
                }
            }
            else
            {

                infile.read(buf_last_piece, last_block_size);

                write(nsoc, buf_last_piece, last_block_size);

                int bytes = read(nsoc, dummy_buf, 10);
                if (!bytes)
                {
                    cout << nsoc << " shut down!\n";
                }
                for (int m = 0; m < last_block_size; m++)
                {
                    buf_last_piece[m] = '\0';
                }
            }
        }
    }
    for (int i = 0; i < 10000; i++)
    {
        buffer[i] = '\0';
    }
    pthread_exit(NULL);
}

bool receiveDownloadDetailsTracker(int tracker_socket, string dest_path)
{
    char buffer[10000];
    for (int j = 0; j < 10000; j++)
    {
        buffer[j] = '\0';
    }
    int bytes = recv(tracker_socket, buffer, 10000, 0);
    if (!bytes)
    {
        cout << "tracker shut down!\n";
        return false;
    }

    string temp = buffer;
    string temp_vec = "";
    vector<string> args;
    for (int i = 0; i < temp.size(); i++)
    {
        if (temp[i] == ' ')
        {
            args.push_back(temp_vec);
            temp_vec = "";
        }
        else
        {
            temp_vec += temp[i];
        }
    }
    args.push_back(temp_vec);

    if (args[0] == "online")
    {

        pair<string, string> seeder_ip_port = getIpPort(args[2]);
        int seeder_socket;
        struct sockaddr_in seeder_addr;

        connectTracker(seeder_socket, seeder_addr, stoi(seeder_ip_port.second), seeder_ip_port.first);

        send(seeder_socket, &args[3][0], args[3].size(), 0);

        long long int fsize = stoll(args[1]);
        int last_block_size = BLOCKSIZE;
        int total_blocks = 0;
        if (fsize % BLOCKSIZE == 0)
        {
            total_blocks = fsize / BLOCKSIZE;
        }
        else
        {
            total_blocks = fsize / BLOCKSIZE;
            total_blocks++;
            last_block_size = fsize % BLOCKSIZE;
        }

        string hash_val = "";
        char *dummy_buf = new char[10];
        char *buf_last_piece = new char[last_block_size];
        char *buf_norm = new char[BLOCKSIZE];
        // int fd = open(&dest_path[0], O_RDWR | O_CREAT, 0777);
        for (int j = 0; j < BLOCKSIZE; j++)
        {
            buf_norm[j] = '\0';
        }
        for (int j = 0; j < last_block_size; j++)
        {
            buf_last_piece[j] = '\0';
        }
        ofstream outfile(dest_path, ios::out | ios::trunc);
        int nw;
        for (int k = 0; k < total_blocks; k++)
        {
            if (k != (total_blocks - 1))
            {
                int bytes = read(seeder_socket, buf_norm, BLOCKSIZE);
                if (!bytes)
                {
                    cout << seeder_socket << " shut down!\n";
                }
                outfile.write(buf_norm, BLOCKSIZE);
                for (int j = 0; j < BLOCKSIZE; j++)
                {
                    buf_norm[j] = '\0';
                }
                write(seeder_socket, dummy_buf, 10);
            }
            else
            {

                int bytes = read(seeder_socket, buf_last_piece, BLOCKSIZE);
                if (bytes <= 0)
                {
                    cout << seeder_socket << " shut down!\n";
                }
                outfile.write(buf_last_piece, last_block_size);

                for (int j = 0; j < last_block_size; j++)
                {
                    buf_last_piece[j] = '\0';
                }
                write(seeder_socket, dummy_buf, 10);
            }
        }
    }
    else
    {
        cout << buffer << endl;
    }
    for (int i = 0; i < 10000; i++)
    {
        buffer[i] = '\0';
    }
    return true;
}

void *clientListener(void *param)
{
    int opt = 1;
    int server_socket;

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        cout << "\n Socket creation error \n";
        pthread_exit(NULL);
    }

    if (inet_pton(AF_INET, &server_ip[0], &server_addr.sin_addr) <= 0)
    {
        cout << "\nInvalid address/ Address not supported \n";
        pthread_exit(NULL);
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(&server_port[0]));

    bind(server_socket,
         (struct sockaddr *)&server_addr,
         sizeof(server_addr));

    if (listen(server_socket, 50) == 0)
        cout << "Listening\n";
    else
        cout << "Error\n";
    while (1)
    {
        struct sockaddr_storage serverStorage;
        addr_size = sizeof(serverStorage);

        client_socket = accept(server_socket,
                               (struct sockaddr *)&serverStorage,
                               &addr_size);
        pthread_t *thread1 = new pthread_t();

        if (pthread_create(thread1, NULL, uploadToPeer, &client_socket) != 0)
        {
            cout << "Failed to create thread\n";
        }
    }
    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        cout << "Invalid Arguments passed\n";
        exit(1);
    }

    // Client Server socket creation
    pair<string, string> server_ip_port = getIpPort(argv[1]);
    server_ip = server_ip_port.first;
    server_port = server_ip_port.second;

    // Read from file
    string tracker_details;
    ifstream MyReadFile(argv[2]);
    getline(MyReadFile, tracker_details);

    // Tracker Connection Code
    int tracker_socket;
    struct sockaddr_in tracker_addr;

    pair<string, string> tracker_ip_port = getIpPort(tracker_details);
    connectTracker(tracker_socket, tracker_addr, stoi(tracker_ip_port.second), tracker_ip_port.first);

    pthread_t *thread = new pthread_t();

    if (pthread_create(thread, NULL, clientListener, NULL) != 0)
    {
        cout << "Failed to create thread\n";
    }

    while (1)
    {
        string cmd;
        cout << "\nEnter Command\n";
        getline(cin, cmd);
        vector<string> args = lineToString(cmd);
        if (args[0] == "close" && args.size() == 1)
        {
            break;
        }
        else
        {
            if (validateCommand(args))
            {
                if (args[0] == "upload_file")
                {
                    if (uploadFileCmd(args))
                    {
                        cmd += " " + args[3] + " " + args[4] + " " + args[5];
                        sendToTracker(cmd, tracker_socket);
                        receiveFromTracker(tracker_socket);
                    }
                }
                else if (args[0] == "login")
                {
                    cmd += " " + server_ip + " " + server_port;
                    sendToTracker(cmd, tracker_socket);
                    receiveFromTracker(tracker_socket);
                }
                else if (args[0] == "download_file")
                {
                    sendToTracker(cmd, tracker_socket);
                    receiveDownloadDetailsTracker(tracker_socket, args[3] + "/" + args[2]);
                    downloaded_files.push_back({args[1], args[2]});
                }
                else if (args[0] == "show_downloads")
                {
                    cout << "\nDownloaded Files are:\n";
                    for (int k = 0; k < downloaded_files.size(); k++)
                    {
                        cout << "[C] "
                             << "[" << downloaded_files[k].first << "]"
                             << " " << downloaded_files[k].second << endl;
                    }
                }
                else
                {
                    sendToTracker(cmd, tracker_socket);
                    receiveFromTracker(tracker_socket);
                }
            }
        }
    }

    close(tracker_socket);

    return 0;
}