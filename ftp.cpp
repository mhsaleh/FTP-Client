// @author Muhammed Saleh

#include "Socket.h"
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h> // for get timing

using namespace std;

// global variables
struct pollfd ufds;
Socket *controlSock;

// ftp command functions
int user(int sd, char* serverIp);
void pass(int sd);
void syst(int sd);
void ls(int sd);
void get(int sd, string file);
void put(int sd, string file);
void passive(int sd, int &port, string &ip);
void type(int sd, string typeChar);
void cd(int sd, string subdir);
void quit(int sd);

// extra credit commands
void mkd(int sd, string subdir);
void dele(int sd, string file);

// utility functions
void pollRead(int sd);
void readFile(int sd, int fileDescriptor);

// class constants
const int BUFLEN = 2000;
const int CMD_MAX = 256;
const int COMMANDLEN = 7;
const char* CMDMARKER = "\r\n";
const char* CLIENTPROMPT = "ftp> ";

//---------------------------------------------------------------------------
// Partial FTP client for sending and retrieving files to and from an FTP
// sever. Implements login, ls, get, put, cd, close, and quit commands.
int main(int argc, char* argv[]) {
    char* serverIp;
    
    // check for correct argument count
    if (argc > 1) {                         // if valid argument count
        serverIp = argv[1];                 // get hostname
    } else {
        cerr << "Invalid argument count.";  // else print error message
        return -1;
    }
    
    controlSock = new Socket(21);                          // create a new socket
    int clientSd = controlSock->getClientSocket(serverIp); // connect to server
    
    char buf[BUFLEN];                  // create a buffer for reading
    read(clientSd, buf, sizeof(buf));  // read line and print to console
    cout << buf;
    
    // check for error
    if (strncmp(buf, "220", 3) != 0) {
        cout << "Error connecting to server." << endl;
        return -1;
    }
    
    // set username and check for error
    if(user(clientSd, serverIp) == -1) {
        cout << "Login error." << endl;
        return -1;
    }
    
    pass(clientSd);  // set pass
    syst(clientSd);  // set syst
    
    cin.ignore();          // clear cin to avoid printing CLIENTPROMPT twice
    while (true) {
        
        cout << CLIENTPROMPT;  // print ftp>
        
        // get one line of input
        string input;
        getline(cin, input);
        
        // parse input and separate into two strings if there is a space
        int size = 0;
        bool space = false;
        while (input[size] != '\0' ) {
            if (input[size] == ' ') {
                space = true;
                break;
            }
            size++;
        }
        
        string command;
        string file;
        
        command = input.substr(0, size); // command is the string before a space
        
        // if there is a space, file is the string after the space
        if (space) {
            file = input.substr(size + 1, input.size());
        }
        
        // call methods corresponding to command
        if(command == "quit") {
            break;
        } else if (command == "close") {
            quit(clientSd);
        } else if(command == "ls") {
            ls(clientSd);
        } else if(command == "get") {
            
            struct timeval startTime;   // timing code
            struct timeval endTime;
            
            gettimeofday( &startTime, NULL );
            
            get(clientSd, file);
            
            gettimeofday( &endTime, NULL );
            cout << "Elapsed Time: "
            << ( endTime.tv_sec - startTime.tv_sec ) * 1000000 +
            ( endTime.tv_usec - startTime.tv_usec )
            << " seconds." << endl;
            
        } else if(command == "put") {
            put(clientSd, file);
        } else if(command == "cd") {
            cd(clientSd, file);
        } else if(command == "mkd") {
            mkd(clientSd, file);  
        } else if(command == "dele") {
            dele(clientSd, file);  
        }        
    }
    close(clientSd); // close socket
    
}

// --------------------------------------------------------------------------------
// user
// Takes in a socket descriptor and prompts the user to enter a user.
// Then sends the passed in user along with the USER command to set the
// username. Returns 1 if error.
int user(int sd, char* serverIp) {
    
    char buf[BUFLEN];
    
    // get user login and print to console
    string userString(getlogin());
    cout << "Name (" << serverIp << ":" << userString << "): ";
    
    string username;   // input username
    cin >> username;
    
    // send username with USER command
    char user[COMMANDLEN + username.size()];
    strcpy(user, "USER ");
    strcat(user, username.c_str());
    strcat(user, CMDMARKER);
    write(sd, user, sizeof(user));
    
    read(sd, buf, sizeof(buf)); // read line and print to console
    
    // check for error
    if (strncmp(buf, "331", 3) != 0) {
        return -1;
    }
    
    cout << buf;
    
    return 0;
}

// --------------------------------------------------------------------------------
// pass
// Takes in a socket descriptor and prompts the user to enter a password.
// Then sends the passed in password along with the PASS command to set the
// password. Reads all data sent from the server after the pass is sent.
void pass(int sd) {
    
    cout << "Password: ";  // prompt for password
    string password;       // input password
    cin >> password;
    
    // send password with PASS command
    char pass[COMMANDLEN + password.size()];
    strcpy(pass, "PASS ");
    strcat(pass, password.c_str());
    strcat(pass, CMDMARKER);
    write(sd, pass, sizeof(pass));
    
    pollRead(sd);  // read until there is nothing left to read
}

// --------------------------------------------------------------------------------
// syst
// Takes in a socket descriptor and sends the syst command to print the current
// system type
void syst(int sd) {
    
    char buf[BUFLEN];
    
    // send SYST command
    char syst[COMMANDLEN];
    strcpy(syst, "SYST ");
    strcat(syst, CMDMARKER);
    write(sd, syst, sizeof(syst));
    
    int nread = read(sd, buf, sizeof(buf)); // read  and print to console
    buf[nread] = '\0';
    cout << buf;
    
}

// --------------------------------------------------------------------------------
// ls
// Takes in a socket descriptor and prints the contents of the ftp server's current
// directory using the LIST command.
void ls(int sd) {
    
    // declare ip and port for pass by reference
    string ip = "";
    int port = 0;
    passive(sd, port, ip); // set to passive mode for data transfer
    
    // initialize new socket based on ip and port retrieved from passive
    Socket *dataSock = new Socket(port);
    int dataSd = dataSock->getClientSocket((char*)ip.c_str());
    
    // send LIST command
    char list[COMMANDLEN];
    strcpy(list, "LIST ");
    strcat(list, CMDMARKER);
    write(sd, list, sizeof(list));
    
    // read line and print to console
    char buf[BUFLEN];
    int nread = read(sd, buf, sizeof(buf));
    buf[nread] = '\0';
    cout << buf;
    
    pollRead(dataSd); // read until there is nothing left to read
    
    close(dataSd); // close data socket
    
    nread = read(sd, buf, sizeof(buf)); // read line and print to console
    buf[nread] = '\0';
    cout << buf;
}

// --------------------------------------------------------------------------------
// get
// Takes in a socket descriptor and retrieves a file from the ftp server.
// If no file name is passed in, this method prompts for a remote file and a
// local file which it then uses to retrieve the corresponding remote file
// and save it based on the local file name.
void get(int sd, string file) {
    
    string remoteFile;   // file to be retrieved
    string localFile;    // file name that the retrieved file will be saved as
    
    // if the filename has not already been specified
    if (file.empty()) {
        cout << "(remote-file) "; // input a remote file name from the user
        cin >> remoteFile;
        
        cout << "(local-file) "; // input a local file name from the user
        cin >> localFile;
        cin.ignore();          // clear cin to avoid printing CLIENTPROMPT twice
    } else {
        remoteFile = file;  // otherwise the passed in file is both filenames
        localFile = file;
    }
    
    // declare ip and port for pass by reference
    string ip = "";
    int port = 0;
    passive(sd, port, ip);  // set to passive mode for data transfer
    type(sd, "I");          // set the transfer mode to binary
    
    // initialize new socket based on ip and port retrieved from passive
    Socket *dataSock = new Socket(port);
    int dataSd = dataSock->getClientSocket((char*)ip.c_str());
    
    // send RETR command
    char retr[COMMANDLEN + remoteFile.size()];
    strcpy(retr, "RETR ");
    strcat(retr, remoteFile.c_str());
    strcat(retr, CMDMARKER);
    write(sd, retr, sizeof(retr));
    
    // read line and print to console
    char buf[BUFLEN];
    int nread = read(sd, buf, sizeof(buf));
    buf[nread] = '\0';
    cout << buf;
    
    // if the filename is valid create file and transfer data into it
    if (strncmp(buf, "550", 3) != 0) {
        
        mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;  // set file flags
        int fileDescriptor = open(localFile.c_str(), O_WRONLY | O_CREAT, mode );
        
        readFile(dataSd, fileDescriptor);         // read from socket into file
        
        // read line and print to console
        nread = read(sd, buf, sizeof(buf));
        buf[nread] = '\0';
        cout << buf;
    }
    
    close(dataSd); // close data socket
}

// --------------------------------------------------------------------------------
// put
// Takes in a socket descriptor and saves a file to the ftp server.
// If no file name is passed in, this method prompts for a remote file and a
// local file which it then uses to retrieve the corresponding local file
// and save it based on the remote file name. This function works regardless
// of whether or not the file exists locally. If the file does not exist it will
// save an empty file of the designated name.
void put(int sd, string file) {
    
    string remoteFile;   // file to be retrieved
    string localFile;    // file name that the retrieved file will be saved as
    
    // if the filename has not already been specified
    if (file.empty()) {
        cout << "(local-file) "; // input a local file name from the user
        cin >> localFile;
        
        cout << "(remote-file) "; // input a remote file name from the user
        cin >> remoteFile;
        
        cin.ignore();          // clear cin to avoid printing CLIENTPROMPT twice
    } else {
        remoteFile = file;  // otherwise the passed in file is both filenames
        localFile = file;
    }
    
    // declare ip and port for pass by reference
    string ip = "";
    int port = 0;
    passive(sd, port, ip);  // set to passive mode for data transfer
    type(sd, "I");          // set the transfer mode to binary
    
    // initialize new socket based on ip and port retrieved from passive
    Socket *dataSock = new Socket(port);
    int dataSd = dataSock->getClientSocket((char*)ip.c_str());
    
    // send STOR command
    char stor[COMMANDLEN + remoteFile.size()];
    strcpy(stor, "STOR ");
    strcat(stor, remoteFile.c_str());
    strcat(stor, CMDMARKER);
    write(sd, stor, sizeof(stor));
    
    // read line and print to console
    char buf[BUFLEN];
    int nread = read(sd, buf, sizeof(buf));
    buf[nread] = '\0';
    cout << buf;
    
    // open local file for writing to ftp
    int fileDescriptor = open(localFile.c_str(), O_RDONLY);
    readFile(fileDescriptor, dataSd);  // read from file and write to ftp
    
    close(dataSd); // close socket
    
    // read line and print to console
    nread = read(sd, buf, sizeof(buf));
    buf[nread] = '\0';
    cout << buf;
}


// --------------------------------------------------------------------------------
// cd
// Takes in a socket descriptor and a string subdir and sets the ftp server's
// current directory to the passed in string
void cd(int sd, string subdir) {
    
    // send CWD command with passed in subdirectory
    char cwd[COMMANDLEN - 1 + subdir.size()];
    strcpy(cwd, "CWD ");
    strcat(cwd, subdir.c_str());
    strcat(cwd, CMDMARKER);
    write(sd, cwd, sizeof(cwd));
    
    // read line and print to console
    char buf[BUFLEN];
    int nread = read(sd, buf, sizeof(buf));
    buf[nread] = '\0';
    cout << buf;
}


// --------------------------------------------------------------------------------
// passive
// Takes in a socket descriptor, a reference int port, and a reference string ip.
// Initializes passive mode and extracts the port and ip returned from the server.
void passive(int sd, int &port, string &ip) {
    
    // send PASV command
    char pasv[COMMANDLEN];
    strcpy(pasv, "PASV ");
    strcat(pasv, CMDMARKER);
    write(sd, pasv, sizeof(pasv));
    
    // read line and print to console
    char buf[BUFLEN];
    int nread = read(sd, buf, sizeof(buf));
    buf[nread] = '\0';
    cout << buf;
    
    // variables for parsing
    string p1 = "";
    string p2 = "";
    bool start = false;
    int count = 0;
    
    // parse line and separate out ip and port
    for (int i =0; buf[i] != ')'; i++) {       // parse until ) is reached
        if (start) {
            if (count < 4) {                   // for the first 4 commas
                if (buf[i] == ',') {
                    count++;
                    if (start && count < 4) {
                        ip += '.';
                    }
                } else {                       // add the char to the ip
                    ip += buf[i];
                }
            } else {                           // after 4 commas
                if (buf[i] == ',') {
                    count++;
                } else if (count == 4) {       // extract p1
                    p1 += buf[i];
                } else if (count == 5) {       // extract p2
                    p2 += buf[i];
                }
            }
        }
        
        if (buf[i] == '(') {                    // start parsing at (
            start = true;
        }
    }
    
    // calculate port based on retrieved port values
    port = atoi(p1.c_str()) * 256 + atoi(p2.c_str());
    
}

// --------------------------------------------------------------------------------
// type
// Takes in a socket descriptor and a string type and sets the ftp server's
// data transfer time based on the passed in string
void type(int sd, string typeChar) {
    
    // send TYPE command along with string
    char type[COMMANDLEN + typeChar.size()];
    strcpy(type, "TYPE ");
    strcat(type, typeChar.c_str());
    strcat(type, CMDMARKER);
    write(sd, type, sizeof(type));
    
    // read line and print to console
    char buf[BUFLEN];
    int nread = read(sd, buf, sizeof(buf));
    buf[nread] = '\0';
    cout << buf;
}


// --------------------------------------------------------------------------------
// quit
// Takes in a socket descriptor and sends the quit command to close the connection
void quit(int sd) {
    
    // send QUIT command
    char quit[COMMANDLEN];
    strcpy(quit, "QUIT ");
    strcat(quit, CMDMARKER);
    write(sd, quit, sizeof(quit));
    
    // read line and print to console
    char buf[BUFLEN];
    int nread = read(sd, buf, sizeof(buf));
    buf[nread] = '\0';
    cout << buf;
}

// --------------------------------------------------------------------------------
// pollRead
// Takes in a socket descriptor sd and polls until there is nothing left to read.
// Everything read from the socket is printed to console.
void pollRead(int sd) {
    ufds.fd = sd;                     // a socket descriptor to exmaine for read
    ufds.events = POLLIN;             // check if this sd is ready to read
    ufds.revents = 0;                 // simply zero-initialized
    
    int nread = -1;
    while (poll(&ufds, 1, 1000) > 0 && nread != 0) { // the socket is ready to read
        char buf[BUFLEN];
        nread = read(sd, buf, sizeof(buf)); // guaranteed to return from read
        buf[nread] = '\0';                  // append endline to avoid junk
        cout << buf;                        // print to console
    }
}

// --------------------------------------------------------------------------------
// readFile
// Takes in a socket descriptor sd and polls until there is nothing left to read.
// Everything read from the socket is saved to the passed in file descriptor
void readFile(int sd, int fileDescriptor) {
    ufds.fd = sd;                     // a socket descriptor to exmaine for read
    ufds.events = POLLIN;             // check if this sd is ready to read
    ufds.revents = 0;                 // simply zero-initialized
    
    int nread = -1;
    while (poll(&ufds, 1, 1000) > 0 && nread != 0) {  // the socket is ready to read
        char buf[BUFLEN];
        nread = read(sd, buf, sizeof(buf)); // guaranteed to return from read
        buf[nread] = '\0';                  // append endline to avoid junk
        write(fileDescriptor, buf, nread);  // write to file
    }
}

// --------------------------------------------------------------------------------
// mkd (extra credit)
// Takes in a socket descriptor and a string subdir and creates a new subdirectory
// with that name
void mkd(int sd, string subdir) {
    
    // send MKD command with passed in subdirectory
    char mkd[COMMANDLEN - 1 + subdir.size()];
    strcpy(mkd, "MKD ");
    strcat(mkd, subdir.c_str());
    strcat(mkd, CMDMARKER);
    write(sd, mkd, sizeof(mkd));
    
    // read line and print to console
    char buf[BUFLEN];
    int nread = read(sd, buf, sizeof(buf));
    buf[nread] = '\0';
    cout << buf;
}

// --------------------------------------------------------------------------------
// dele(extra credit)
// Takes in a socket descriptor and a string file and deletes the remote file with
// that name.
void dele(int sd, string file) {
    
    // send DELE command with passed in file name
    char dele[COMMANDLEN + file.size()];
    strcpy(dele, "DELE ");
    strcat(dele, file.c_str());
    strcat(dele, CMDMARKER);
    write(sd, dele, sizeof(dele));
    
    // read line and print to console
    char buf[BUFLEN];
    int nread = read(sd, buf, sizeof(buf));
    buf[nread] = '\0';
    cout << buf;
}