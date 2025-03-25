#include <arpa/inet.h>
#include <netinet/in.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/stat.h>
#include <filesystem>
#include <vector>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <chrono>
#include <thread>

constexpr int PORT_NO = 15050;
constexpr int NET_BUF_SIZE = 1024;
constexpr char nofile[] = "File Not Found!";
constexpr char serverDir[] = "server_files";
constexpr char logFile[] = "server_log.txt";


enum class Protocol { TCP, UDP };


void logActivity(const std::string& username, const std::string& action, const std::string& filename, const std::string& protocol) {
    std::ofstream log(logFile, std::ios::app);
    if (log.is_open()) {
        auto now = std::chrono::system_clock::now();
        auto now_time = std::chrono::system_clock::to_time_t(now);
        log << "[" << std::put_time(std::localtime(&now_time), "%Y-%m-%d %H:%M:%S") << "] "
            << "User: " << username << " | "
            << "Action: " << action << " | "
            << "File: " << filename << " | "
            << "Protocol: " << protocol << "\n";
        log.close();
    }
}


void createServerDirectory() {
    if (!std::filesystem::exists(serverDir)) {
        std::filesystem::create_directory(serverDir);
        std::cout << "Created directory: " << serverDir << std::endl;
    }
}


void clearBuf(char* b, int size = NET_BUF_SIZE) {
    memset(b, '\0', size);
}


std::vector<std::string> getFileList() {
    std::vector<std::string> files;
    for (const auto& entry : std::filesystem::directory_iterator(serverDir)) {
        if (entry.is_regular_file()) {
            files.push_back(entry.path().filename().string());
        }
    }
    return files;
}



void sendFileListTCP(int sockfd) {
    auto files = getFileList();
    std::ostringstream oss;
    for (const auto& file : files) {
        oss << file << "\n";
    }
    std::string fileList = oss.str();
    
    // Send the file list in chunks
    size_t pos = 0;
    while (pos < fileList.size()) {
        char buffer[NET_BUF_SIZE];
        clearBuf(buffer);
        size_t chunkSize = std::min(static_cast<size_t>(NET_BUF_SIZE - 1), fileList.size() - pos);
        fileList.copy(buffer, chunkSize, pos);
        pos += chunkSize;
        send(sockfd, buffer, NET_BUF_SIZE, 0);
    }
    
    // Send EOF marker
    char eofBuffer[NET_BUF_SIZE];
    clearBuf(eofBuffer);
    eofBuffer[0] = EOF;
    send(sockfd, eofBuffer, NET_BUF_SIZE, 0);
}



void sendFileListUDP(int sockfd, sockaddr_in& clientAddr, socklen_t addrlen) {
    auto files = getFileList();
    std::ostringstream oss;
    for (const auto& file : files) {
        oss << file << "\n";
    }
    std::string fileList = oss.str();
    
    // Send the file list in chunks
    size_t pos = 0;
    while (pos < fileList.size()) {
        char buffer[NET_BUF_SIZE];
        clearBuf(buffer);
        size_t chunkSize = std::min(static_cast<size_t>(NET_BUF_SIZE - 1), fileList.size() - pos);
        fileList.copy(buffer, chunkSize, pos);
        pos += chunkSize;
        
        sendto(sockfd, buffer, NET_BUF_SIZE, 0,
              reinterpret_cast<sockaddr*>(&clientAddr), addrlen);
    }
    
    // Send EOF marker
    char eofBuffer[NET_BUF_SIZE];
    clearBuf(eofBuffer);
    eofBuffer[0] = EOF;
    sendto(sockfd, eofBuffer, NET_BUF_SIZE, 0,
          reinterpret_cast<sockaddr*>(&clientAddr), addrlen);
}



void handleDownloadTCP(int sockfd, const char* filename, const std::string& username) {
    std::string filepath = std::string(serverDir) + "/" + filename;
    std::ifstream file(filepath, std::ios::binary);
    
    if (!file.is_open()) {
        char response[NET_BUF_SIZE];
        clearBuf(response);
        strcpy(response, nofile);
        send(sockfd, response, NET_BUF_SIZE, 0);
        std::cerr << "File not found: " << filename << std::endl;
        logActivity(username, "Download Failed", filename, "TCP");
        return;
    }

    char buffer[NET_BUF_SIZE];
    while (!file.eof()) {
        clearBuf(buffer);
        file.read(buffer, NET_BUF_SIZE - 1);
        int bytesRead = static_cast<int>(file.gcount());
        if (bytesRead > 0) {
            send(sockfd, buffer, bytesRead + 1, 0);
        }
    }

    // Send EOF marker
    clearBuf(buffer);
    buffer[0] = EOF;
    send(sockfd, buffer, NET_BUF_SIZE, 0);

    file.close();
    std::cout << "File sent via TCP: " << filename << std::endl;
    logActivity(username, "Downloaded", filename, "TCP");
}



void handleDownloadUDP(int sockfd, sockaddr_in& clientAddr, socklen_t addrlen, const char* filename, const std::string& username) {
    std::string filepath = std::string(serverDir) + "/" + filename;
    std::ifstream file(filepath, std::ios::binary);
    
    if (!file.is_open()) {
        char response[NET_BUF_SIZE];
        clearBuf(response);
        strcpy(response, nofile);
        sendto(sockfd, response, NET_BUF_SIZE, 0, 
              reinterpret_cast<sockaddr*>(&clientAddr), addrlen);
        std::cerr << "File not found: " << filename << std::endl;
        logActivity(username, "Download Failed", filename, "UDP");
        return;
    }

    char buffer[NET_BUF_SIZE];
    while (!file.eof()) {
        clearBuf(buffer);
        file.read(buffer, NET_BUF_SIZE - 1);
        int bytesRead = static_cast<int>(file.gcount());
        if (bytesRead > 0) {
            sendto(sockfd, buffer, bytesRead + 1, 0,
                  reinterpret_cast<sockaddr*>(&clientAddr), addrlen);
        }
    }

    // Send EOF marker
    clearBuf(buffer);
    buffer[0] = EOF;
    sendto(sockfd, buffer, NET_BUF_SIZE, 0,
          reinterpret_cast<sockaddr*>(&clientAddr), addrlen);

    file.close();
    std::cout << "File sent via UDP: " << filename << std::endl;
    logActivity(username, "Downloaded", filename, "UDP");
}



void handleUploadTCP(int sockfd, const char* filename, const std::string& username) {
    std::string filepath = std::string(serverDir) + "/" + filename;
    std::ofstream file(filepath, std::ios::binary);
    
    if (!file.is_open()) {
        std::cerr << "Failed to create file: " << filename << std::endl;
        logActivity(username, "Upload Failed", filename, "TCP");
        return;
    }

    char buffer[NET_BUF_SIZE];
    bool transferComplete = false;
    
    while (!transferComplete) {
        clearBuf(buffer);
        int bytesReceived = recv(sockfd, buffer, NET_BUF_SIZE, 0);
        
        if (bytesReceived <= 0) {
            // Connection closed or error
            transferComplete = true;
        } 
        else if (bytesReceived > 0 && buffer[0] == EOF) {
            // EOF marker received
            transferComplete = true;
        } 
        else if (bytesReceived > 0) {
            // Write actual data (excluding null terminator if present)
            file.write(buffer, bytesReceived);
        }
    }

    file.close();
    std::cout << "File received via TCP: " << filename << std::endl;
    logActivity(username, "Uploaded", filename, "TCP");
}



void handleUploadUDP(int sockfd, sockaddr_in& clientAddr, socklen_t addrlen, const char* filename, const std::string& username) {
    std::string filepath = std::string(serverDir) + "/" + filename;
    std::ofstream file(filepath, std::ios::binary);
    
    if (!file.is_open()) {
        std::cerr << "Failed to create file: " << filename << std::endl;
        logActivity(username, "Upload Failed", filename, "UDP");
        return;
    }

    char buffer[NET_BUF_SIZE];
    bool transferComplete = false;
    
    while (!transferComplete) {
        clearBuf(buffer);
        int bytesReceived = recvfrom(sockfd, buffer, NET_BUF_SIZE, 0,
                                   reinterpret_cast<sockaddr*>(&clientAddr), &addrlen);
        
        if (bytesReceived <= 0 || buffer[0] == EOF) {
            transferComplete = true;
        } else if (bytesReceived > 0) {
            file.write(buffer, bytesReceived - 1); // -1 to exclude null terminator
        }
    }

    file.close();
    std::cout << "File received via UDP: " << filename << std::endl;
    logActivity(username, "Uploaded", filename, "UDP");
}




void handleTCPClient(int clientSock) {
    char net_buf[NET_BUF_SIZE];
    std::string clientUsername;
    
    // Get username first
    clearBuf(net_buf);
    int bytesReceived = recv(clientSock, net_buf, NET_BUF_SIZE, 0);
    if (bytesReceived > 0 && strncmp(net_buf, "USERNAME ", 9) == 0) {
        clientUsername = std::string(net_buf + 9);
        logActivity(clientUsername, "Connected", "", "TCP");
        send(clientSock, "OK", 3, 0);  // Acknowledge username
    } else {
        close(clientSock);
        return;
    }

    while (true) {
        clearBuf(net_buf);
        bytesReceived = recv(clientSock, net_buf, NET_BUF_SIZE, 0);

        if (bytesReceived <= 0) {
            std::cout << "Client disconnected" << std::endl;
            if (!clientUsername.empty()) {
                logActivity(clientUsername, "Disconnected", "", "TCP");
            }
            break;
        }

        std::string command(net_buf);
        
        if (command == "LIST") {
            sendFileListTCP(clientSock);
            logActivity(clientUsername, "Listed files", "", "TCP");
        } 
        else if (command.find("DOWNLOAD ") == 0) {
            std::string filename = command.substr(9);
            handleDownloadTCP(clientSock, filename.c_str(), clientUsername);
        } 
        else if (command.find("UPLOAD ") == 0) {
            std::string filename = command.substr(7);
            handleUploadTCP(clientSock, filename.c_str(), clientUsername);
        }
    }
    close(clientSock);
}




void handleUDPConnection(int udpSock) {
    sockaddr_in clientAddr;
    socklen_t addrlen = sizeof(clientAddr);
    char net_buf[NET_BUF_SIZE];
    std::string clientUsername;
    
    while (true) {
        clearBuf(net_buf);
        int bytesReceived = recvfrom(udpSock, net_buf, NET_BUF_SIZE, 0,
                                   reinterpret_cast<sockaddr*>(&clientAddr), &addrlen);

        if (bytesReceived > 0) {
            std::string command(net_buf);
            
            if (command.find("USERNAME ") == 0) {
                clientUsername = command.substr(9);
                logActivity(clientUsername, "Connected", "", "UDP");
            }
            else if (command == "LIST") {
                sendFileListUDP(udpSock, clientAddr, addrlen);
                logActivity(clientUsername, "Listed files", "", "UDP");
            } 
            else if (command.find("DOWNLOAD ") == 0) {
                std::string filename = command.substr(9);
                handleDownloadUDP(udpSock, clientAddr, addrlen, filename.c_str(), clientUsername);
            } 
            else if (command.find("UPLOAD ") == 0) {
                std::string filename = command.substr(7);
                handleUploadUDP(udpSock, clientAddr, addrlen, filename.c_str(), clientUsername);
            }
        }
    }
}



int main() {
    createServerDirectory();

    // Create TCP socket
    int tcpSock = socket(AF_INET, SOCK_STREAM, 0);
    if (tcpSock < 0) {
        std::cerr << "TCP Socket creation failed!\n";
        return 1;
    }

    // Create UDP socket
    int udpSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSock < 0) {
        std::cerr << "UDP Socket creation failed!\n";
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT_NO);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // Bind TCP socket
    if (bind(tcpSock, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        std::cerr << "TCP Binding failed!\n";
        return 1;
    }

    // Bind UDP socket
    if (bind(udpSock, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        std::cerr << "UDP Binding failed!\n";
        return 1;
    }

    // Listen for TCP connections
    if (listen(tcpSock, 5) < 0) {
        std::cerr << "TCP Listen failed!\n";
        return 1;
    }

    std::cout << "Server started. Waiting for connections on TCP and UDP...\n";

    // Start UDP handler in a separate thread
    std::thread udpThread(handleUDPConnection, udpSock);

    // Handle TCP connections in main thread
    while (true) {
        sockaddr_in clientAddr;
        socklen_t addrlen = sizeof(clientAddr);
        
        // In your main function where you create the thread:
        int clientSock = accept(tcpSock, reinterpret_cast<sockaddr*>(&clientAddr), &addrlen);
        if (clientSock < 0) {
            std::cerr << "TCP Accept failed!\n";
            continue;
        }

        std::cout << "New TCP connection from: " << inet_ntoa(clientAddr.sin_addr) << std::endl;

        // Now only pass the socket descriptor to the thread
        std::thread tcpThread(handleTCPClient, clientSock);
        tcpThread.detach();
    }

    // Cleanup (though we'll never reach here in this simple server)
    close(tcpSock);
    close(udpSock);
    udpThread.join();

    return 0;
}