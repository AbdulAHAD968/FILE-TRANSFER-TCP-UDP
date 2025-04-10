#include<iostream>
#include<fstream>
#include<cstring>
#include<vector>
#include<sstream>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<filesystem>
#include<unistd.h>


constexpr char IP_ADDRESS[] = "127.0.0.1";
constexpr int PORT_NO = 15050;
constexpr int NET_BUF_SIZE = 1024;
constexpr char client_dir[] = "client_files";


using namespace std;


enum class Protocol{ TCP, UDP };


// MAKE CLIENT DIRECTORY [JUST FOR EASE IN TESTING]
void MAKE_CLIENT_DIR(){
    if(!filesystem::exists(client_dir)){
        filesystem::create_directory(client_dir);
        cout<<"\n=> CREATED DIRECTORY :: [ "<<client_dir<<" ]"<<endl;
    }
}


// FREEUP ENTIRE BUFFER SPACE
void FREEUP_BUFFER(char* b, int size = NET_BUF_SIZE){
    memset(b, '\0', size);
}


// ESTABLISH TCP CONNECTION.
void MAKE_TCP_CONNECT(int sockfd, sockaddr_in& serverAddr, const string& username){

    if(connect(sockfd, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr))<0){
        cerr<<"TCP Connection Failed!\n";
        exit(1);
    }
    string command = "USERNAME " + username;
    send(sockfd, command.c_str(), command.size() + 1, 0);
}


// ESTABLISH UDP CONNECTION.
void MAKE_UDP_CONNECT(int sockfd, sockaddr_in& serverAddr, const string& username){
    string command = "USERNAME " + username;
    sendto(sockfd, command.c_str(), command.size() + 1, 0, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr));
}


/////////////////////////////////////////////////////////////
////////////////////////// { TCP } //////////////////////////
/////////////////////////////////////////////////////////////


// LIST FILES USING TCP CONNECTION.
void LIST_FILE_TCP(int sockfd){

    const char* command = "LIST";
    send(sockfd, command, strlen(command) + 1, 0);

    cout<<"\n=> FILES AVAILABLE ON SERVER ::\n";
    cout<<"|----------------------------|\n";

    char buffer[NET_BUF_SIZE];
    bool FLAG = false;
    
    while(!FLAG){
        FREEUP_BUFFER(buffer);
        int byt_rev = recv(sockfd, buffer, NET_BUF_SIZE, 0);
        
        if(byt_rev <= 0 || buffer[0] == EOF){
            FLAG = true;
        }
        else{
            cout<<buffer;
        }
    }
    cout<<"|----------------------------|\n";
}


// DOWNLOAD FILE FROM SERVER.
void DOWNLOAD_TCP(int sockfd, const char* filename){

    string command = "DOWNLOAD " + string(filename);
    send(sockfd, command.c_str(), command.size() + 1, 0);

    // FIRST RECIEVE THE FILE SIZE.
    streamsize fileSize;
    recv(sockfd, &fileSize, sizeof(fileSize), 0);
    
    if(fileSize<=0){
        cerr<<"\n FILE NOT AVAILABLE OR IS EMPTY <^-^> "<<endl;
        return;
    }

    string filepath = string(client_dir) + "/" + filename;
    ofstream file(filepath, ios::binary);
    
    if(!file.is_open()){
        cerr<<"\n FAILED TO CREATE FILE :: [ "<<filename<<" ]"<<endl;
        return;
    }

    char buffer[NET_BUF_SIZE];
    streamsize crr_rev = 0;
    
    while(crr_rev<fileSize){

        streamsize remaining = fileSize - crr_rev;
        streamsize chunkSize = min(static_cast<streamsize>(NET_BUF_SIZE), remaining);
        
        ssize_t byt_rev = recv(sockfd, buffer, chunkSize, 0);
        if(byt_rev <= 0){
            cerr<<"\nERROR RECIEVING FILE DATA."<<endl;
            break;
        }
        
        file.write(buffer, byt_rev);
        crr_rev += byt_rev;
    }

    file.close();
    
    if(crr_rev == fileSize){
        cout<<"\n=> FILE DOWNLOADED SUCCESSFULLY :: [ "<<crr_rev<<" BYT ]  :: { "<<filename<<" } "<<endl;
    }
    else{
        cout<<"\n=> ISSUE DOWNLOADING FILE :: [ "<<crr_rev<<" ] / { "<<fileSize<<" BYT} :: [ "<<filename<<" ]"<<endl;
    }
}


// UPLOAD FILE TO SERVER
void UPLOAD_TCP(int sockfd, const char* filename){
    
    string command = "UPLOAD " + string(filename);
    if(send(sockfd, command.c_str(), command.size() + 1, 0) <= 0){
        cerr<<"\n FAILED TO SEND UPLOAD COMMAND TO SERVER."<<endl;
        return;
    }

    string filepath = string(client_dir) + "/" + filename;
    ifstream file(filepath, ios::binary | ios::ate);
    if(!file.is_open()){
        cerr<<"FILE NOT FOUND :: [ "<<filepath<<" ]"<<endl;
        return;
    }

    uint64_t fileSize = file.tellg();
    file.seekg(0, ios::beg);
    uint64_t fs_net = htobe64(fileSize);
    
    if(send(sockfd, &fs_net, sizeof(fs_net), 0) != sizeof(fs_net)){
        cerr<<"Failed to send file size."<<endl;
        file.close();
        return;
    }
    
    char buffer[NET_BUF_SIZE];
    uint64_t totalSent = 0;
    while(totalSent<fileSize){
        uint64_t remaining = fileSize - totalSent;
        uint64_t chunkSize = min((uint64_t)NET_BUF_SIZE, remaining);
        
        file.read(buffer, chunkSize);
        ssize_t sent = send(sockfd, buffer, chunkSize, 0);
        if(sent<=0){
            cerr<<"Error sending file data."<<endl;
            file.close();
            return;
        }
        
        totalSent += sent;
        cout<<"\n UPLOADING FILE :: { "<<(totalSent*100/fileSize)<<" } % " <<flush<<endl;
    }
    
    file.close();
    cout<<"\n=> FILE UPLOADED SUCCESSFULLY :: [ "<<filename<<" ] :: { "<<totalSent<<" BYTES}"<<endl;
    
    // GET THE SERVER ACK.
    char ack;
    if(recv(sockfd, &ack, 1, 0) > 0 && ack == '1'){
        cout<<"Server acknowledged file upload."<<endl;
    }
    else{
        cerr<<"No acknowledgment from server."<<endl;
    }
}


/////////////////////////////////////////////////////////////
////////////////////////// { UDP } //////////////////////////
/////////////////////////////////////////////////////////////


// FUNCTION TO LIST FILES USING UDP.
void LIST_FILE_UDP(int sockfd, sockaddr_in& serverAddr, socklen_t addrlen) {
    
    const char* command = "LIST";
    sendto(sockfd, command, strlen(command)+1, 0, reinterpret_cast<sockaddr*>(&serverAddr), addrlen);

    cout<<"\nAvailable files on server:\n";
    cout<<"--------------------------\n";

    char buffer[NET_BUF_SIZE];
    bool FLAG = false;
    
    while(!FLAG){
        
        FREEUP_BUFFER(buffer);
        int byt_rev = recvfrom(sockfd, buffer, NET_BUF_SIZE, 0, reinterpret_cast<sockaddr*>(&serverAddr), &addrlen);
        
        if(byt_rev <= 0 || buffer[0] == EOF){
            FLAG = true;
        }
        else{
            cout << buffer;
        }
    }
    cout<<"--------------------------\n";
}


// DOWNLOAD FILES FROM SERVER.
void DOWNLOAD_UDP(int sockfd, sockaddr_in& serverAddr, socklen_t addrlen, const char* filename) {
    
    string command = "DOWNLOAD " + string(filename);
    sendto(sockfd, command.c_str(), command.size() + 1, 0, reinterpret_cast<sockaddr*>(&serverAddr), addrlen);

    string filepath = string(client_dir) + "/" + filename;
    ofstream file(filepath, ios::binary);
    
    if(!file.is_open()){
        cout<<"\n FAILED TO CREATE FILE  :: { "<<filename<<" }"<<endl;
        return;
    }

    char buffer[NET_BUF_SIZE];
    bool FLAG = false;
    
    while(!FLAG){
        FREEUP_BUFFER(buffer);
        int byt_rev = recvfrom(sockfd, buffer, NET_BUF_SIZE, 0, reinterpret_cast<sockaddr*>(&serverAddr), &addrlen);
        
        if(byt_rev <= 0 || buffer[0] == EOF){
            FLAG = true;
        }
        else{
            file.write(buffer, byt_rev - 1);
        }
    }

    file.close();
    cout<<"\n FILE DOWNLOADED  :: { "<<filename<<" }"<<endl;
}


// UPLOAD FILES TO SERVER USING UDP.
void UPLOAD_UDP(int sockfd, sockaddr_in& serverAddr, socklen_t addrlen, const char* filename){
    
    string filepath = string(client_dir) + "/" + filename;
    ifstream file(filepath, ios::binary);
    
    if(!file.is_open()){
        cout<<"\n FILE NOT FOUND  :: { "<<filename<<" }"<<endl;
        return;
    }

    string command = "UPLOAD " + string(filename);
    sendto(sockfd, command.c_str(), command.size() + 1, 0, reinterpret_cast<sockaddr*>(&serverAddr), addrlen);

    char buffer[NET_BUF_SIZE];
    while(!file.eof()){
        FREEUP_BUFFER(buffer);
        file.read(buffer, NET_BUF_SIZE - 1);
        int byt_read = file.gcount();
        if(byt_read>0){
            sendto(sockfd, buffer, byt_read + 1, 0, reinterpret_cast<sockaddr*>(&serverAddr), addrlen);
        }
    }

    // SEND EOF MARKER
    FREEUP_BUFFER(buffer);
    buffer[0] = EOF;
    sendto(sockfd, buffer, NET_BUF_SIZE, 0, reinterpret_cast<sockaddr*>(&serverAddr), addrlen);

    file.close();
    cout<<"\n FILE UPLOADED  :: { "<<filename<<" }"<<endl;
}


///////////////////////////////////////////////////////////////


// OUR BELOVED MAIN FUNCTION.
int main(){

    MAKE_CLIENT_DIR();

    string username;
    cout<<"\n ENTER USERNAME TO PROCEED <^-^>  ::";
    getline(cin, username);

    Protocol protocol;
    cout<<"\n SELECT PROTOCOL [1]. TCP & [2]. UDP ";
    int choice;
    cin>>choice;
    cin.ignore();
    
    if(choice == 1){
        cout<<"TCP SELECTED. [RELIABLE AND CONNECTION ORIENTED]\n";
        protocol = Protocol::TCP;
    } 
    else if(choice == 2){
        cout<<"TCP SELECTED. [UNRELIABLE AND CONNECTION LESS]\n";
        protocol = Protocol::UDP;
    }
    else{
        cerr<<"SELECT CORRECT TYPE!\n";
        return 1;
    }

    int sockfd;
    sockaddr_in serverAddr;
    socklen_t addrlen = sizeof(serverAddr);
    
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT_NO);
    serverAddr.sin_addr.s_addr = inet_addr(IP_ADDRESS);
    
    if(protocol == Protocol::TCP){
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if(sockfd<0){
            cerr<<"TCP Socket creation failed!\n";
            return 1;
        }
        MAKE_TCP_CONNECT(sockfd, serverAddr, username);
    }
    else{
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if(sockfd<0){
            cerr<<"UDP Socket creation failed!\n";
            return 1;
        }
        MAKE_UDP_CONNECT(sockfd, serverAddr, username);
    }

    while(true){
        cout<<"\n=> SELECT OPERATION <^-^>\n";
        cout<<"[1]. LIST FILES ON SERVER\n";
        cout<<"[2]. DOWNLOAD FILES FROM SERVER.\n";
        cout<<"[3]. UPLOAD FILES TO SERVER.\n";
        cout<<"[0]. EXIT.\n";
        cout<<"Enter choice: ";
        
        int op;
        cin>>op;
        cin.ignore();

        if(op == 0){
            break;
        }

        switch(op){
            case 1:
            {
                if(protocol == Protocol::TCP){
                    LIST_FILE_TCP(sockfd);
                } 
                else{
                    LIST_FILE_UDP(sockfd, serverAddr, addrlen);
                }
                break;
            }
            case 2:
            {
                string filename;
                cout<<"\n ENTER FILE NAME ALONG EXTENXION TO DOWNLOAD.";
                getline(cin, filename);
                if(protocol == Protocol::TCP){
                    DOWNLOAD_TCP(sockfd, filename.c_str());
                }
                else{
                    DOWNLOAD_UDP(sockfd, serverAddr, addrlen, filename.c_str());
                }
                break;
            }
            case 3:
            {
                string filename;
                cout<<"\n ENTER FILENAME TO UPLOAD <^-^> ";
                getline(cin, filename);
                if(protocol == Protocol::TCP){
                    UPLOAD_TCP(sockfd, filename.c_str());
                }
                else{
                    UPLOAD_UDP(sockfd, serverAddr, addrlen, filename.c_str());
                }
                break;
            }
            default:
            {
                cout<<"INVALID CHOICE!\n";
            }
        }
    
    }
    close(sockfd);

return 0;
}

