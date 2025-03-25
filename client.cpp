#include <arpa/inet.h>
#include <netinet/in.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include <filesystem>
#include <vector>
#include <sstream>

constexpr char IP_ADDRESS[] = "127.0.0.1";
constexpr int PORT_NO = 15050;
constexpr int NET_BUF_SIZE = 1024;
constexpr char clientDir[] = "client_files";

enum class Protocol { TCP, UDP };


void createClientDirectory() {
    if (!std::filesystem::exists(clientDir)) {
        std::filesystem::create_directory(clientDir);
        std::cout << "Created directory: " << clientDir << std::endl;
    }
}


void clearBuf(char* b, int size = NET_BUF_SIZE) {
    memset(b, '\0', size);
}


void setupTCPConnection(int sockfd, sockaddr_in& serverAddr, const std::string& username) {
    if (connect(sockfd, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        std::cerr << "TCP Connection Failed!\n";
        exit(1);
    }
    
    std::string command = "USERNAME " + username;
    send(sockfd, command.c_str(), command.size() + 1, 0);
}


void setupUDPConnection(int sockfd, sockaddr_in& serverAddr, const std::string& username) {
    std::string command = "USERNAME " + username;
    sendto(sockfd, command.c_str(), command.size() + 1, 0,
          reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr));
}

/////////////////////////////////////////////////////////////


void requestFileListTCP(int sockfd) {
    const char* command = "LIST";
    send(sockfd, command, strlen(command) + 1, 0);

    std::cout << "\nAvailable files on server:\n";
    std::cout << "--------------------------\n";

    char buffer[NET_BUF_SIZE];
    while (true) {
        clearBuf(buffer);
        int bytesReceived = recv(sockfd, buffer, NET_BUF_SIZE, 0);
        
        if (bytesReceived <= 0 || buffer[0] == EOF) {
            break;
        }
        std::cout << buffer;
    }
    std::cout << "--------------------------\n";
}


void downloadFileTCP(int sockfd, const char* filename) {
    std::string command = "DOWNLOAD " + std::string(filename);
    send(sockfd, command.c_str(), command.size() + 1, 0);

    std::string filepath = std::string(clientDir) + "/" + filename;
    std::ofstream file(filepath, std::ios::binary);
    
    if (!file.is_open()) {
        std::cerr << "Failed to create file: " << filename << std::endl;
        return;
    }

    char buffer[NET_BUF_SIZE];
    while (true) {
        clearBuf(buffer);
        int bytesReceived = recv(sockfd, buffer, NET_BUF_SIZE, 0);
        
        if (bytesReceived <= 0) {
            break;  // Connection closed
        }
        if (buffer[0] == EOF) {
            break;  // End of file
        }
        file.write(buffer, bytesReceived);
    }

    file.close();
    std::cout << "File downloaded: " << filename << std::endl;
}


void uploadFileTCP(int sockfd, const char* filename) {
    std::string filepath = std::string(clientDir) + "/" + filename;
    std::ifstream file(filepath, std::ios::binary);
    
    if (!file.is_open()) {
        std::cerr << "File not found: " << filename << std::endl;
        return;
    }

    // Send UPLOAD command
    std::string command = "UPLOAD " + std::string(filename);
    send(sockfd, command.c_str(), command.size() + 1, 0);

    // Wait for server to be ready (optional)
    char ack[NET_BUF_SIZE];
    recv(sockfd, ack, NET_BUF_SIZE, 0);

    char buffer[NET_BUF_SIZE];
    while (!file.eof()) {
        clearBuf(buffer);
        file.read(buffer, NET_BUF_SIZE);
        int bytesRead = file.gcount();
        if (bytesRead > 0) {
            send(sockfd, buffer, bytesRead, 0);
        }
    }

    // Send EOF marker
    clearBuf(buffer);
    buffer[0] = EOF;
    send(sockfd, buffer, 1, 0);

    file.close();
    std::cout << "File uploaded: " << filename << std::endl;
}


/////////////////////////////////////////////////////////////


void requestFileListUDP(int sockfd, sockaddr_in& serverAddr, socklen_t addrlen) {
    const char* command = "LIST";
    sendto(sockfd, command, strlen(command) + 1, 0,
          reinterpret_cast<sockaddr*>(&serverAddr), addrlen);

    std::cout << "\nAvailable files on server:\n";
    std::cout << "--------------------------\n";

    char buffer[NET_BUF_SIZE];
    bool transferComplete = false;
    
    while (!transferComplete) {
        clearBuf(buffer);
        int bytesReceived = recvfrom(sockfd, buffer, NET_BUF_SIZE, 0,
                                   reinterpret_cast<sockaddr*>(&serverAddr), &addrlen);
        
        if (bytesReceived <= 0 || buffer[0] == EOF) {
            transferComplete = true;
        } else {
            std::cout << buffer;
        }
    }
    std::cout << "--------------------------\n";
}


void downloadFileUDP(int sockfd, sockaddr_in& serverAddr, socklen_t addrlen, const char* filename) {
    std::string command = "DOWNLOAD " + std::string(filename);
    sendto(sockfd, command.c_str(), command.size() + 1, 0,
          reinterpret_cast<sockaddr*>(&serverAddr), addrlen);

    std::string filepath = std::string(clientDir) + "/" + filename;
    std::ofstream file(filepath, std::ios::binary);
    
    if (!file.is_open()) {
        std::cerr << "Failed to create file: " << filename << std::endl;
        return;
    }

    char buffer[NET_BUF_SIZE];
    bool transferComplete = false;
    
    while (!transferComplete) {
        clearBuf(buffer);
        int bytesReceived = recvfrom(sockfd, buffer, NET_BUF_SIZE, 0,
                                   reinterpret_cast<sockaddr*>(&serverAddr), &addrlen);
        
        if (bytesReceived <= 0 || buffer[0] == EOF) {
            transferComplete = true;
        } else {
            file.write(buffer, bytesReceived - 1); // -1 to exclude null terminator
        }
    }

    file.close();
    std::cout << "File downloaded: " << filename << std::endl;
}


void uploadFileUDP(int sockfd, sockaddr_in& serverAddr, socklen_t addrlen, const char* filename) {
    std::string filepath = std::string(clientDir) + "/" + filename;
    std::ifstream file(filepath, std::ios::binary);
    
    if (!file.is_open()) {
        std::cerr << "File not found: " << filename << std::endl;
        return;
    }

    std::string command = "UPLOAD " + std::string(filename);
    sendto(sockfd, command.c_str(), command.size() + 1, 0,
          reinterpret_cast<sockaddr*>(&serverAddr), addrlen);

    char buffer[NET_BUF_SIZE];
    while (!file.eof()) {
        clearBuf(buffer);
        file.read(buffer, NET_BUF_SIZE - 1);
        int bytesRead = file.gcount();
        if (bytesRead > 0) {
            sendto(sockfd, buffer, bytesRead + 1, 0,
                  reinterpret_cast<sockaddr*>(&serverAddr), addrlen);
        }
    }

    // Send EOF marker
    clearBuf(buffer);
    buffer[0] = EOF;
    sendto(sockfd, buffer, NET_BUF_SIZE, 0,
          reinterpret_cast<sockaddr*>(&serverAddr), addrlen);

    file.close();
    std::cout << "File uploaded: " << filename << std::endl;
}


// OUR BELOVED MAIN FUNCTION.
int main() {
    createClientDirectory();

    std::string username;
    std::cout << "Enter your username: ";
    std::getline(std::cin, username);

    Protocol protocol;
    std::cout << "Select protocol (1 for TCP, 2 for UDP): ";
    int choice;
    std::cin >> choice;
    std::cin.ignore(); // Clear input buffer
    
    if (choice == 1) {
        std::cout << "TCP protocol selected\n";
        protocol = Protocol::TCP;
    } else if (choice == 2) {
        std::cout << "UDP protocol selected\n";
        protocol = Protocol::UDP;
    } else {
        std::cerr << "Invalid protocol selection!\n";
        return 1;
    }

    int sockfd;
    sockaddr_in serverAddr;
    socklen_t addrlen = sizeof(serverAddr);
    
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT_NO);
    serverAddr.sin_addr.s_addr = inet_addr(IP_ADDRESS);
    
    if (protocol == Protocol::TCP) {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            std::cerr << "TCP Socket creation failed!\n";
            return 1;
        }
        setupTCPConnection(sockfd, serverAddr, username);
    } else {
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd < 0) {
            std::cerr << "UDP Socket creation failed!\n";
            return 1;
        }
        setupUDPConnection(sockfd, serverAddr, username);
    }

    while (true) {
        std::cout << "\nChoose operation:\n";
        std::cout << "1. List available files on server\n";
        std::cout << "2. Download file from server\n";
        std::cout << "3. Upload file to server\n";
        std::cout << "4. Exit\n";
        std::cout << "Enter choice: ";
        
        int operation;
        std::cin >> operation;
        std::cin.ignore(); // Clear input buffer

        if (operation == 4) break;

        switch (operation) {
            case 1:
                if (protocol == Protocol::TCP) {
                    requestFileListTCP(sockfd);
                } else {
                    requestFileListUDP(sockfd, serverAddr, addrlen);
                }
                break;
            case 2: {
                std::string filename;
                std::cout << "Enter filename to download: ";
                std::getline(std::cin, filename);
                if (protocol == Protocol::TCP) {
                    downloadFileTCP(sockfd, filename.c_str());
                } else {
                    downloadFileUDP(sockfd, serverAddr, addrlen, filename.c_str());
                }
                break;
            }
            case 3: {
                std::string filename;
                std::cout << "Enter filename to upload: ";
                std::getline(std::cin, filename);
                if (protocol == Protocol::TCP) {
                    uploadFileTCP(sockfd, filename.c_str());
                } else {
                    uploadFileUDP(sockfd, serverAddr, addrlen, filename.c_str());
                }
                break;
            }
            default:
                std::cout << "Invalid choice!\n";
        }
    }

    close(sockfd);
    return 0;
}