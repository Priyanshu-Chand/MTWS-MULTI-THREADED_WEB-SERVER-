//         g++ server.cpp -o server.exe -pthread -std=c++17 -lws2_32

//-------------------------------------------------------------------------------//

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#endif

#include <errno.h>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <fstream>
#include <sstream>
#include <string>
#include <chrono>
#include <dirent.h>

#include "SimpleThreadPool.h"

using namespace std;

// Cross-platform function to close a socket
void closeSocket(int sock)
{
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}

// --- Global Counters and a single global ThreadPool object ---
atomic<int> total_requests(0);
atomic<int> active_threads(0);
atomic<int> closed_connections(0);
atomic<int> peak_connections(0);
mutex cout_mutex;

SimpleThreadPool pool(32);

// --- Helper Functions ---
bool endsWith(const string &str, const string &suffix)
{
    if (str.length() < suffix.length())
    {
        return false;
    }
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

string readFile(const string &path)
{
    ifstream file("www/" + path, ios::in | ios::binary);
    if (!file.is_open())
    {
        return "";
    }
    stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

string getContentType(const string &path)
{
    if (endsWith(path, ".html"))
        return "text/html";
    if (endsWith(path, ".css"))
        return "text/css";
    if (endsWith(path, ".js"))
        return "application/javascript";
    if (endsWith(path, ".jpg") || endsWith(path, ".jpeg"))
        return "image/jpeg";
    if (endsWith(path, ".png"))
        return "image/png";
    if (endsWith(path, ".gif"))
        return "image/gif";
    if (endsWith(path, ".mp4"))
        return "video/mp4";
    return "application/octet-stream";
}

string listFilesAsJson(const string &directoryPath)
{
    string json = "[";
    DIR *dir = opendir(directoryPath.c_str());
    if (dir == nullptr)
    {
        cerr << "Error: Could not open media directory at " << directoryPath << endl;
        return "[]";
    }
    struct dirent *entry;
    bool first = true;
    while ((entry = readdir(dir)) != nullptr)
    {
        string name = entry->d_name;
        if (name != "." && name != "..")
        {
            if (!first)
            {
                json += ",";
            }
            json += "\"" + name + "\"";
            first = false;
        }
    }
    closedir(dir);
    json += "]";
    return json;
}

// This is our main task function. The thread pool will call this.
void handle_client(int client_socket)
{
    active_threads++;
    total_requests++;
    char buffer[4096];
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received > 0)
    {
        buffer[bytes_received] = '\0';
        string request(buffer);
        string firstLine = request.substr(0, request.find("\r\n"));
        string path = firstLine.substr(firstLine.find(" ") + 1);
        path = path.substr(0, path.find(" "));
        if (path == "/" || path.empty())
        {
            path = "index.html";
        }
        string response_headers;
        string response_body;
        if (path == "/status")
        {
            response_headers = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n";
            long current_connected = total_requests.load() - closed_connections.load();
            int current_peak = peak_connections.load();
            while (current_connected > current_peak)
            {
                if (peak_connections.compare_exchange_weak(current_peak, current_connected))
                    break;
            }
            response_body = "{\"activeThreads\":" + to_string(active_threads.load()) +
                            ",\"totalRequests\":" + to_string(total_requests.load()) +
                            ",\"connectedUsers\":" + to_string(current_connected) +
                            ",\"closedConnections\":" + to_string(closed_connections.load()) +
                            ",\"peakConnections\":" + to_string(peak_connections.load()) +
                            ",\"queuedTasks\":" + to_string(pool.get_queue_size()) + "}";
            string full_response = response_headers + response_body;
            send(client_socket, full_response.c_str(), full_response.size(), 0);
        }
        else if (path == "/api/files")
        {
            string file_list_json = listFilesAsJson("www/media");
            response_headers = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n";
            string full_response = response_headers + file_list_json;
            send(client_socket, full_response.c_str(), full_response.size(), 0);
        }
        else
        {
            response_body = readFile(path);
            if (response_body.empty())
            {
                response_headers = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nFile Not Found";
                send(client_socket, response_headers.c_str(), response_headers.size(), 0);
            }
            else
            {
                response_headers = "HTTP/1.1 200 OK\r\n";
                response_headers += "Content-Type: " + getContentType(path) + "\r\n";
                response_headers += "Content-Length: " + to_string(response_body.length()) + "\r\n";
                response_headers += "\r\n";
                send(client_socket, response_headers.c_str(), response_headers.size(), 0);
                send(client_socket, response_body.c_str(), response_body.length(), 0);
            }
        }
    }
    this_thread::sleep_for(chrono::seconds(1));
    closeSocket(client_socket);
    closed_connections++;
    active_threads--;
}

// This is the new main function with debug statements
int main()
{
    cout << "DEBUG: Program started." << endl;

    // Initialize Winsock on Windows
#ifdef _WIN32
    cout << "DEBUG: Initializing Winsock..." << endl;
    WSADATA wsaData;
    int wsa_startup_result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsa_startup_result != 0)
    {
        cerr << "FATAL ERROR: WSAStartup failed with error code: " << wsa_startup_result << endl;
        // The program will exit here if Winsock fails to start
        return 1;
    }
    cout << "DEBUG: Winsock initialized successfully." << endl;
#endif

    cout << "DEBUG: Creating socket..." << endl;
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        cerr << "FATAL ERROR: Socket creation failed: " << strerror(errno) << endl;
        return 1;
    }
    cout << "DEBUG: Socket created successfully." << endl;

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);

    int opt = 1;
#ifdef _WIN32
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));
#else
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    cout << "DEBUG: Binding socket to port 8080..." << endl;
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        cerr << "FATAL ERROR: Bind failed: " << strerror(errno) << endl;
        closeSocket(server_socket);
        return 1;
    }
    cout << "DEBUG: Socket bound successfully." << endl;

    cout << "DEBUG: Setting socket to listen..." << endl;
    if (listen(server_socket, SOMAXCONN) == -1)
    {
        cerr << "FATAL ERROR: Listen failed: " << strerror(errno) << endl;
        closeSocket(server_socket);
        return 1;
    }
    cout << "DEBUG: Socket is listening. Server is ready to start." << endl;

    // If we get here, everything worked. Now print the normal startup message.
    cout << "=========================================" << endl;
    cout << "Server started with a Simple Thread Pool!" << endl;
    cout << "Open in browser: http://127.0.0.1:8080" << endl;
    cout << "=========================================" << endl;

    while (true)
    {
        int client_socket = accept(server_socket, nullptr, nullptr);
        if (client_socket == -1)
        {
            cerr << "Accept failed: " << strerror(errno) << endl;
            continue;
        }
        pool.enqueue(client_socket);
    }

    closeSocket(server_socket);

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}