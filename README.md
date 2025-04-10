# File Transfer System using TCP and UDP

A simple yet efficient file transfer system that allows clients to upload/download files from a server using both TCP and UDP protocols. Implemented in C++ with socket programming.

---

## 📌 Features

- **Multi-user Support**: Multiple clients can connect simultaneously
- **Dual Protocol Support**: Both TCP (reliable) and UDP (fast) implementations
- **File Operations**:
  - Upload files to server
  - Download files from server
  - List available files on server
- **Comprehensive Logging**: Server maintains logs of all connections and transfers
- **Simple Authentication**: Username-based client identification

---

## 🗂 File Structure
.
├── client.cpp # Client-side implementation
├── client_files/ # Default client file storage
│ ├── 1.txt
│ ├── 2.txt
│ ├── 3.txt
│ └── 4.txt
├── server.cpp # Server-side implementation
├── server_files/ # Server file storage
│ ├── 1.txt
│ ├── 2.txt
│ ├── 3.txt
│ └── 4.txt
├── server_log.txt # Server activity logs
└── Makefile # Compilation instructions

---

## 🚀 How It Works

### Basic Flow
1. Client connects to server
2. Server prompts for username
3. Client enters username (logged by server)
4. Client selects operation:
   - Upload file
   - Download file
   - List files
5. Operation executes
6. Session continues until client types "exit"

### Protocol Differences
- **TCP**: Reliable, ordered file transfer with connection-oriented communication
- **UDP**: Connectionless fast transfer with potential packet loss

## 🛠 Setup & Usage

1. **Compile**:
   ```bash
   make all
