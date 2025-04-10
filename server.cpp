#include<iostream>
#include<fstream>
#include<cstring>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<unistd.h>
#include<sys/stat.h>
#include<filesystem>
#include<vector>
#include<sstream>
#include<algorithm>
#include<ctime>
#include<iomanip>
#include<chrono>
#include<thread>


constexpr int PORT_NO = 15050;
constexpr int NET_BUF_SIZE = 1024;
constexpr char nofile[] = "FILE NOT FOUND!";
constexpr char server_dir[] = "server_files";

using namespace std;


enum class Protocol{ TCP, UDP };


void LOG_ACTIVITY(const string& username, const string& action, const string& filename, const string& protocol) {
    
    // OPEN FILE IN APPENDING MODE TO AVOID TRUNCATING DATA...
    ofstream log("server_log.txt", ios::app);
    
    if(log.is_open()){

        auto now = chrono::system_clock::now();        
        auto now_time = chrono::system_clock::to_time_t(now);

        log <<"["<<put_time(localtime(&now_time), "%Y-%m-%d %H:%M:%S")<< "] "
            <<"USER_NAME: "<<username<< " | "
            <<"ACTVITY: "<<action<<" | "
            <<"FILE_NAME: "<<filename<<" | "
            <<"PROTOCOL: "<<protocol<<"\n";
        log.close();
    }
}


// FUNCTION TO CREATE THE SERVER DIRECTORY,
// ACTUALLY FOR TESTING FILES, I HAVE ALSO USED THE 
// SIMILAR APPROAH ON THE CLIENT SIDE AS WELL.
void MAKE_SERVER_DIR(){
    if(!filesystem::exists("server_files")){
        filesystem::create_directory("server_files");
        cout<<"\n MADE TEH DIRECTORY :: "<<"server_files"<<endl;
    }
}


void FREEUP_BUFFER(char* b, int size = NET_BUF_SIZE) {
    memset(b, '\0', size);
}


vector<string> READ_FILES(){

    vector<string> files;

    // ACTUALLY WE ARE PARSING THROUGH THE DIRECTORY AND APPENDING REGULAR FILES TO VECOTR.
    for(const auto& entry : filesystem::directory_iterator(server_dir)){
        if(entry.is_regular_file()){
            files.push_back(entry.path().filename().string());
        }
    }
    return files;
}

// SHARE THE LIST OF FILES WITH THE CLIENT OVER UDP
void SEND_FL_UDP(int sockfd, sockaddr_in& clientAddr, socklen_t addrlen){

    auto files = READ_FILES();
    
    ostringstream oss;
    for(const auto& file : files){
        oss<<file<<"\n";
    }
    
    string F_LIST = oss.str();
    
    // SENDING THE LIST IN CHUNKS OF DATA, INSTEAD OF WHOLE STRING AT ONCE.
    size_t pos = 0;
    while(pos<F_LIST.size()){
        
        char buffer[NET_BUF_SIZE];
        FREEUP_BUFFER(buffer);
        
        size_t chunk_size = min(static_cast<size_t>(NET_BUF_SIZE - 1), F_LIST.size() - pos);
        F_LIST.copy(buffer, chunk_size, pos);
        pos += chunk_size;
        sendto(sockfd, buffer, NET_BUF_SIZE, 0, reinterpret_cast<sockaddr*>(&clientAddr), addrlen);
    }
    
    // SEND THE EOF MARKER AS WELL.
    char Eof_buff[NET_BUF_SIZE];
    FREEUP_BUFFER(Eof_buff);
    Eof_buff[0] = EOF;
    sendto(sockfd, Eof_buff, NET_BUF_SIZE, 0, reinterpret_cast<sockaddr*>(&clientAddr), addrlen);
}


// BASICALLY THIS FUNCTION IS FOR SENDING FILES TO CLIENTS. 
void HANDLE_DOWNLOAD_UDP(int sockfd, sockaddr_in& clientAddr, socklen_t addrlen, const char* filename, const string& username){
    
    string filepath = string(server_dir) + "/" + filename;
    
    ifstream file(filepath, ios::binary);
    
    if(!file.is_open()){
        char response[NET_BUF_SIZE];
        FREEUP_BUFFER(response);
        strcpy(response, nofile);
        sendto(sockfd, response, NET_BUF_SIZE, 0, reinterpret_cast<sockaddr*>(&clientAddr), addrlen);
        cerr<<"\n FILE NOT FOUND :: [ "<<filename<<" ]"<<endl;
        LOG_ACTIVITY(username, "DOWNLOAD FAILED", filename, "UDP");
        return;
    }

    char buffer[NET_BUF_SIZE];
    while(!file.eof()){
        FREEUP_BUFFER(buffer);
        file.read(buffer, NET_BUF_SIZE - 1);
        int bytesRead = static_cast<int>(file.gcount());
        if(bytesRead>0){
            sendto(sockfd, buffer, bytesRead + 1, 0, reinterpret_cast<sockaddr*>(&clientAddr), addrlen);
        }
    }

    // SEND THE EOF MARKER.
    FREEUP_BUFFER(buffer);
    buffer[0] = EOF;
    sendto(sockfd, buffer, NET_BUF_SIZE, 0, reinterpret_cast<sockaddr*>(&clientAddr), addrlen);

    file.close();
    cout<<"\nFILE SENT VIA UDP :: [ "<<filename<<" ]"<<endl;
    LOG_ACTIVITY(username, "DOWNLOAD SUCCESS", filename, "UDP");
}


// FUNCTION TO HANDLE USERS REQUEST TO UPLOAD FILES.
void RECIEVE_FILES_UDP(int sockfd, sockaddr_in& clientAddr, socklen_t addrlen, const char* filename, const string& username){
    
    string filepath = string(server_dir) + "/" + filename;
    ofstream file(filepath, ios::binary);
    
    if(!file.is_open()){
        cerr<<"\n FAILED TO CREATE FILE :: [ "<<filename<<" ]"<<endl;
        LOG_ACTIVITY(username, "UPLOAD FAILED", filename, "UDP");
        return;
    }

    char buffer[NET_BUF_SIZE];
    bool dummy_flag = false;
    
    while(!dummy_flag){

        FREEUP_BUFFER(buffer);
        int byte_rev = recvfrom(sockfd, buffer, NET_BUF_SIZE, 0, reinterpret_cast<sockaddr*>(&clientAddr), &addrlen);
        
        if(byte_rev<=0 || buffer[0]==EOF){
            dummy_flag = true;
        }
        else if(byte_rev>0){
            file.write(buffer, byte_rev - 1);
        }
    }

    file.close();
    cout<<"FILE RECIEVED VIA UDP :: [ "<<filename<<" ]"<<endl;
    LOG_ACTIVITY(username, "Uploaded", filename, "UDP");
}

// FUNCTION TO HANDLE CLIENTS REQUEST FOR UDP.
void UDP_CONNECTION(int udpSock){

    sockaddr_in clientAddr;
    socklen_t addrlen = sizeof(clientAddr);
    char net_buf[NET_BUF_SIZE];
    string usr_name;
    
    while(true){
        FREEUP_BUFFER(net_buf);
        int byte_rev = recvfrom(udpSock, net_buf, NET_BUF_SIZE, 0, reinterpret_cast<sockaddr*>(&clientAddr), &addrlen);

        if(byte_rev>0){
           
            string command(net_buf);
            
            if(command.find("USERNAME ") == 0){
                usr_name = command.substr(9);
                LOG_ACTIVITY(usr_name, "CONNECTED", "", "UDP");
            }
            else if (command == "LIST"){
                SEND_FL_UDP(udpSock, clientAddr, addrlen);
                LOG_ACTIVITY(usr_name, "LISTED FILES", "", "UDP");
            } 
            else if (command.find("DOWNLOAD ") == 0){
                string filename = command.substr(9);
                HANDLE_DOWNLOAD_UDP(udpSock, clientAddr, addrlen, filename.c_str(), usr_name);
            } 
            else if (command.find("UPLOAD ") == 0){
                string filename = command.substr(7);
                RECIEVE_FILES_UDP(udpSock, clientAddr, addrlen, filename.c_str(), usr_name);
            }
        }
    }
}


///////////////////////////////////////////////////////
/////////////// {TCP FUNCTIONS BELOW } ////////////////
///////////////////////////////////////////////////////


// SEND LIST OVER TCP CONNECTION.
void SEND_LIST_TCP(int clientSock){

    auto files = READ_FILES();
    ostringstream oss;
    for(const auto& file : files){
        oss<<file<<"\n";
    }
    string F_LIST = oss.str();
    
    // SEND THE LIST IN CHUNKS, SIMILAR TO WHAT WE DID IN UDP
    size_t pos = 0;
    while(pos<F_LIST.size()){
        char buffer[NET_BUF_SIZE];
        FREEUP_BUFFER(buffer);
        size_t chunk_size = min(static_cast<size_t>(NET_BUF_SIZE - 1), F_LIST.size() - pos);
        F_LIST.copy(buffer, chunk_size, pos);
        pos += chunk_size;
        
        send(clientSock, buffer, NET_BUF_SIZE, 0);
    }
    
    // SEND THE EOF MARKER
    char Eof_buff[NET_BUF_SIZE];
    FREEUP_BUFFER(Eof_buff);
    Eof_buff[0] = EOF;
    send(clientSock, Eof_buff, NET_BUF_SIZE, 0);
}


// PROVIDE FILE TO CLIENT
void SEND_FILE_TCP(int clientSock, const char* filename, const string& username){

    string filepath = string(server_dir) + "/" + filename;
    
    // OPEN FILE AND SEEK TO EOF.
    ifstream file(filepath, ios::binary | ios::ate);
    
    if(!file.is_open()){
        char response[NET_BUF_SIZE];
        FREEUP_BUFFER(response);
        strcpy(response, nofile);
        send(clientSock, response, NET_BUF_SIZE, 0);
        cerr<<"\n FILE UNAVAILABLE ::  [ "<<filename<<" ]"<<endl;
        LOG_ACTIVITY(username, "Download Failed", filename, "TCP");
        return;
    }

    // GET THE FILE SIZE.
    streamsize fileSize = file.tellg();
    file.seekg(0, ios::beg);

    // FIRST SEND THE FIEL SIZE.
    send(clientSock, &fileSize, sizeof(fileSize), 0);

    // THEN THE CONTENT
    char buffer[NET_BUF_SIZE];
    streamsize totalSent = 0;
    
    while(totalSent<fileSize){

        streamsize remaining = fileSize - totalSent;
        streamsize chunk_size = min(static_cast<streamsize>(NET_BUF_SIZE), remaining);
        file.read(buffer, chunk_size);
        streamsize bytesRead = file.gcount();
        
        if(bytesRead<=0){
            break;
        }
        
        ssize_t sent = send(clientSock, buffer, bytesRead, 0);
        if(sent<=0){
            cerr<<"\n ERROR IN SENDNG FILE DATA."<<endl;
            break;
        }
        totalSent += sent;
    }

    file.close();
    cout<<"\n FILE SENT VIA TCP => { "<<totalSent<<" BYTES }  :: [ "<<filename<<" ]"<<endl;
    LOG_ACTIVITY(username, "DOWNLOADED", filename, "TCP");
}


// RECIEVE FILE FROM CLIENT
void RCV_FILE_TCP(int clientSock, const char* filename, const string& username){

    string filepath = string(server_dir) + "/" + filename;
    ofstream file(filepath, ios::binary);
    if(!file.is_open()){
        cerr<<"\n FAILED TO CREATE TCP FILES :: [ "<<filepath<<" ]"<<endl;
        LOG_ACTIVITY(username, "Upload Failed", filename, "TCP");
        return;
    }
    
    // FIRST RCV. THE FILE SIZE.
    uint64_t fileSizeNetwork;
    ssize_t rcv_byte = recv(clientSock, &fileSizeNetwork, sizeof(fileSizeNetwork), 0);
    if(rcv_byte != sizeof(fileSizeNetwork)){
        cerr<<"\n FAILED TO RECIEVED THE FILE SIZE, GOT THIS INSTEAD [ "<<rcv_byte<<" ] BYT."<<endl;
        file.close();
        remove(filepath.c_str());
        LOG_ACTIVITY(username, "UPLOAD FAILED", filename, "TCP");
        return;
    }
    
    uint64_t fileSize = be64toh(fileSizeNetwork); // Convert to host byte order
    cout<<"RCV. FILE SIZE :: [ "<<fileSize<<" ] BYT."<<endl;
    
    // RECIEVE THE DATA NOW.
    char buffer[NET_BUF_SIZE];
    uint64_t rcv_yet = 0;
    
    while(rcv_yet<fileSize){

        uint64_t remaining = fileSize - rcv_yet;
        uint64_t chunk_size = min((uint64_t)NET_BUF_SIZE, remaining);
        
        ssize_t byte_rev = recv(clientSock, buffer, chunk_size, 0);
        if(byte_rev<=0){
            cerr<<"\n ERROR RECIEVING FILE DATA."<<endl;
            file.close();
            remove(filepath.c_str());
            LOG_ACTIVITY(username, "UPLOAD FAILED", filename, "TCP");
            return;
        }
        
        file.write(buffer, byte_rev);
        rcv_yet += byte_rev;
        
        // PROGRESS INDICATOR [WORKS FASTER THAN U THINK IN NORMAL (HAHA)]
        cout<<"\nRECIEVING :: "<<(rcv_yet*100/fileSize)<<"%"<<flush;
    }
    
    file.close();
    cout<<"\n FILE RECIEVED :: [ "<<rcv_yet<<" bytes ]  :: { "<<filename<<" }"<<endl;
    LOG_ACTIVITY(username, "Uploaded", filename, "TCP");
    
    // SEND ACK. TO CLIENT
    char ack = '1';
    send(clientSock, &ack, 1, 0);
}


void HANDKE_TCP_SOCK(int clientSock){

    char net_buf[NET_BUF_SIZE];
    string usr_name;
    
    FREEUP_BUFFER(net_buf);
    recv(clientSock, net_buf, NET_BUF_SIZE, 0);
    string command(net_buf);
    
    if(command.find("USERNAME ") == 0){
        usr_name = command.substr(9);
        LOG_ACTIVITY(usr_name, "Connected", "", "TCP");
    }
    
    // PROCESS CLIENT COMMANDS.
    while(true){
        
        FREEUP_BUFFER(net_buf);
        int byte_rev = recv(clientSock, net_buf, NET_BUF_SIZE, 0);
        
        if(byte_rev<=0){
            // CLIENT DISCONNECTED.
            LOG_ACTIVITY(usr_name, "Disconnected", "", "TCP");
            break;
        }
        
        string command(net_buf);
        
        if(command == "LIST"){
            SEND_LIST_TCP(clientSock);
            LOG_ACTIVITY(usr_name, "LISTED CLIENTS", "", "TCP");
        } 
        else if (command.find("DOWNLOAD ") == 0) {
            string filename = command.substr(9);
            SEND_FILE_TCP(clientSock, filename.c_str(), usr_name);
        } 
        else if (command.find("UPLOAD ") == 0) {
            string filename = command.substr(7);
            RCV_FILE_TCP(clientSock, filename.c_str(), usr_name);
        }
    }
    
    close(clientSock);
}



// MAIN FUNCTION
int main(){

    MAKE_SERVER_DIR();

    // CRATE TCP SOCKET.
    int tcpSock = socket(AF_INET, SOCK_STREAM, 0);
    if(tcpSock<0){
        cerr<<"TCP SOCKET CREATION FAILED!\n";
        return 1;
    }

    // CREATE UDP SOCKET.
    int udpSock = socket(AF_INET, SOCK_DGRAM, 0);
    if(udpSock<0){
        cerr<<"UDP SOCKET CREATION FAILED!\n";
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT_NO);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // BIND THE TCP SOCKET
    if(bind(tcpSock, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr))<0){
        cerr<<"TCP Binding failed!\n";
        return 1;
    }

    // BIND THE UDP SOCKET
    if(bind(udpSock, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0){
        cerr<<"UDP Binding failed!\n";
        return 1;
    }

    // LISTEN FOR TCP CONNECTIONS.
    if(listen(tcpSock, 5)<0){
        cerr<<"TCP Listen failed!\n";
        return 1;
    }

    cout<<"\n=> SERVER RUNNING. WAITING FOR CONNECTIONS :: [TCP,UDP] \n";

    // PROCESS UDP IN A SEPERATE THREAD.
    thread udpThread(UDP_CONNECTION, udpSock);

    // HANDLE TCP IN MAIN THREAD.
    while(true){

        sockaddr_in clientAddr;
        socklen_t addrlen = sizeof(clientAddr);
        
        int clientSock = accept(tcpSock, reinterpret_cast<sockaddr*>(&clientAddr), &addrlen);
        if(clientSock<0){
            cerr<<"TCP Accept failed!\n";
            continue;
        }

        cout<<"\n=> New CONNECTION DEVELOPED :: [ "<<inet_ntoa(clientAddr.sin_addr)<<" ]"<<endl;

        // PASS THE SOCKET ADDRESS.
        thread tcpThread(HANDKE_TCP_SOCK, clientSock);
        tcpThread.detach();
    }

    close(tcpSock);
    close(udpSock);
    udpThread.join();

return 0;
}