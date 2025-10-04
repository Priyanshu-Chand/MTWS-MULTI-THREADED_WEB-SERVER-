#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
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

// --- Global Counters and a single global ThreadPool object ---
atomic<int> total_requests(0);
atomic<int> active_threads(0);
atomic<int> closed_connections(0);
atomic<int> peak_connections(0);
mutex cout_mutex;

SimpleThreadPool pool(32);

// --- Helper Functions ---
bool endsWith(const string& str, const string& suffix) {
    if (str.length() < suffix.length()) return false;
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

string readFile(const string& path) {
    // We assume a 'www' sub-directory for web files
    ifstream file("www/" + path, ios::in | ios::binary);
    if (!file.is_open()) return "";
    stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// UPDATED: getContentType to handle more file types
string getContentType(const string& path) {
    if (endsWith(path, ".html")) return "text/html";
    if (endsWith(path, ".css")) return "text/css";
    if (endsWith(path, ".js")) return "application/javascript";
    if (endsWith(path, ".jpg") || endsWith(path, ".jpeg")) return "image/jpeg";
    if (endsWith(path, ".png")) return "image/png";
    if (endsWith(path, ".gif")) return "image/gif";
    if (endsWith(path, ".mp4")) return "video/mp4";
    return "application/octet-stream"; 
}

// NEW: Function to list files in the media directory as a JSON array
string listFilesAsJson(const string& directoryPath) {
    string json = "[";
    DIR* dir = opendir(directoryPath.c_str());
    if (dir == nullptr) {
        cerr << "Error: Could not open media directory at " << directoryPath << endl;
        return "[]";
    }

    struct dirent* entry;
    bool first = true;
    while ((entry = readdir(dir)) != nullptr) {
        string name = entry->d_name;
        if (name != "." && name != "..") {
            if (!first) {
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


// UPDATED: This is our main task function. The thread pool will call this.
void handle_client(int client_socket) {
    active_threads++;
    total_requests++;
    
    char buffer[4096];
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        string request(buffer);
        string firstLine = request.substr(0, request.find("\r\n"));
        string path = firstLine.substr(firstLine.find(" ") + 1);
        path = path.substr(0, path.find(" "));

        if (path == "/" || path.empty()) path = "index.html";

        string response;
        string response_body;

        if (path == "/status") {
            response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n";
            long current_connected = total_requests.load() - closed_connections.load();
            
            int current_peak = peak_connections.load();
            while (current_connected > current_peak) {
                if(peak_connections.compare_exchange_weak(current_peak, current_connected)) break;
            }

            response += "{\"activeThreads\":" + to_string(active_threads.load()) +
                        ",\"totalRequests\":" + to_string(total_requests.load()) + 
                        ",\"connectedUsers\":" + to_string(current_connected) +
                        ",\"closedConnections\":" + to_string(closed_connections.load()) +
                        ",\"peakConnections\":" + to_string(peak_connections.load()) +
                        ",\"queuedTasks\":" + to_string(pool.get_queue_size()) + "}";
            
            send(client_socket, response.c_str(), response.size(), 0);

        } else if (path == "/api/files") {
            // New endpoint to list files from the 'www/media' directory
            string file_list_json = listFilesAsJson("www/media");
            response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n" + file_list_json;
            send(client_socket, response.c_str(), response.size(), 0);

        } else {
            // General file serving logic
            response_body = readFile(path); // readFile prepends "www/"
            if (response_body.empty()) {
                response = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nFile Not Found";
                send(client_socket, response.c_str(), response.size(), 0);
            } else {
                response = "HTTP/1.1 200 OK\r\n";
                response += "Content-Type: " + getContentType(path) + "\r\n";
                response += "Content-Length: " + to_string(response_body.length()) + "\r\n";
                response += "\r\n";
                
                // Send headers first, then the body. This is better for binary files.
                send(client_socket, response.c_str(), response.size(), 0);
                send(client_socket, response_body.c_str(), response_body.length(), 0);
            }
        }
    }
    
    this_thread::sleep_for(chrono::seconds(1));
    
    close(client_socket);
    
    closed_connections++;
    active_threads--;
}

// main function remains unchanged
int main() {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        cerr << "Socket creation failed: " << strerror(errno) << endl;
        return 1;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        cerr << "Bind failed: " << strerror(errno) << endl;
        close(server_socket);
        return 1;
    }

    if (listen(server_socket, SOMAXCONN) == -1) {
        cerr << "Listen failed: " << strerror(errno) << endl;
        close(server_socket);
        return 1;
    }


    cout << "=========================================" << endl;
    cout << "Server started with a Simple Thread Pool!" << endl;
    cout << "Open in browser: http://127.0.0.1:8080" << endl;
    cout << "=========================================" << endl;

    while (true) {
        int client_socket = accept(server_socket, nullptr, nullptr);
        if (client_socket == -1) {
            cerr << "Accept failed: " << strerror(errno) << endl;
            continue;
        }
        pool.enqueue(client_socket);
    }

    close(server_socket);
    return 0;
}
