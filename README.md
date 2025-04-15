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

## 📁 Project Structure

```bash
.
├── client.cpp           # Client-side logic
├── client_files/        # Local client file storage
│   ├── 1.txt
│   ├── 2.txt
│   ├── 3.txt
│   └── 4.txt
├── server.cpp           # Server-side logic
├── server_files/        # Central server file repository
│   ├── 1.txt
│   ├── 2.txt
│   ├── 3.txt
│   └── 4.txt
├── server_log.txt       # Logs of server activity
└── Makefile             # Build instructions

```
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
2. **Command to Run Server**:
   ```bash
   g++ -o s.out server.cpp && ./s.out
3. **Command to Run Client**:
   ```bash
   g++ -o c.out client.cpp && ./s.out

=> **Do what ever you want to do then but it will have limitations obviously.**
