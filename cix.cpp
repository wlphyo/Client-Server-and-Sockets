// $Id: cix.cpp,v 1.9 2019-04-05 15:04:28-07 - - $
/*
 William Phyo (wlphyo@ucsc.edu)
Yuance Lin (ylin198@ucsc.edu) */
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <fstream>
#include <vector>
#include <sstream>
using namespace std;

#include <libgen.h>
#include <sys/types.h>
#include <unistd.h>

#include "protocol.h"
#include "logstream.h"
#include "sockets.h"
logstream outlog (cout);
struct cix_exit: public exception {};

unordered_map<string,cix_command> command_map {
   {"exit", cix_command::EXIT},
   {"help", cix_command::HELP},
   {"ls"  , cix_command::LS  },
   {"put"  , cix_command::PUT },
   {"rm"  , cix_command::RM },
   {"get"  , cix_command::GET },
};

static const char help[] = R"||(
exit         - Exit the program.  Equivalent to EOF.
get filename - Copy remote file to local host.
help         - Print help summary.
ls           - List names of files on remote server.
put filename - Copy local file to remote host.
rm filename  - Remove file from remote server.
)||";

void cix_help() {
   cout << help;
}

void cix_ls (client_socket& server) {
   cix_header header;
   header.command = cix_command::LS;
   outlog << "sending header " << header << endl;
   send_packet (server, &header, sizeof header);
   recv_packet (server, &header, sizeof header);
   outlog << "received header " << header << endl;
   if (header.command != cix_command::LSOUT) {
      outlog << "sent LS, server did not return LSOUT" << endl;
      outlog << "server returned " << header << endl;
   }else {
      size_t host_nbytes = ntohl (header.nbytes);
      auto buffer = make_unique<char[]> (host_nbytes + 1);
      recv_packet (server, buffer.get(), host_nbytes);
      outlog << "received " << host_nbytes << " bytes" << endl;
      buffer[host_nbytes] = '\0';
      cout << buffer.get();
   }
}
void cix_rm(client_socket& server, string filename) {
   cix_header header;
   snprintf(header.filename,filename.length()+1,filename.c_str());
   header.command = cix_command::RM;
   header.nbytes = 0;
   outlog << "sending header " << header << endl;
   send_packet (server, &header, sizeof header);
   recv_packet (server, &header, sizeof header);
   outlog << "received header " << header << endl;
   if(header.command == cix_command::NAK) {
      outlog <<"NAK received: RM failed to remove "<<filename<< endl;
   }else if(header.command == cix_command::ACK){
      outlog<<"ACK received: file has been removed"<<endl;
   }
}
void cix_get(client_socket& server, string filename) {
   cix_header header;
   header.command = cix_command::GET;
   //header.filename store c str
   snprintf(header.filename,filename.length()+1,filename.c_str());
   outlog << "sending header " << header << endl;
   send_packet (server, &header, sizeof header);
   recv_packet (server, &header, sizeof header);
   outlog << "received header " << header << endl;
   if (header.command == cix_command::FILEOUT) {
      auto buffer = make_unique<char[]> (header.nbytes + 1);
      recv_packet (server, buffer.get(), header.nbytes);
      outlog << "received " << header.nbytes << " bytes" << endl;
      buffer[header.nbytes] = '\0';
      ofstream temp1(header.filename,ofstream::binary);
      temp1.write(buffer.get(),header.nbytes);
      temp1.close();
      outlog<< filename<< " has been retrieved."<<endl;
   }else {
      outlog << filename<< " does not exists on the serve." << endl;
   }

}
void cix_put(client_socket& server, string filename) {
   cix_header header;
   //header.filename store c str
   snprintf(header.filename,filename.length()+1,filename.c_str());
   ifstream temp1(header.filename,ifstream::binary);
   if(temp1){
      //get length of file (cplusplus.com)
      temp1.seekg(0, temp1.end); //set poistion in input sequence
      int length = temp1.tellg();
      temp1.seekg (0, temp1.beg);
      char *buffer = new char[length];
      temp1.read(buffer,length);
      header.command = cix_command::PUT;
      header.nbytes = length;
      outlog << "sending header " << header << endl;
      send_packet (server, &header, sizeof header); 
      send_packet(server,buffer,length);
      recv_packet (server, &header, sizeof header);
      outlog << "received header " << header << endl;
      delete[] buffer;
   }else {
      outlog <<"Error: file does not exists: " << filename<< endl;
   } 
   if(header.command == cix_command::NAK) {
      outlog <<"NAK received: PUT failed import file to the server\n";
   }else if(header.command == cix_command::ACK){
      outlog<<"ACK received: file has been PUT on the server"<<endl;
   }
   temp1.close();
}


void usage() {
   cerr << "Usage: " << outlog.execname() << " [host] [port]" << endl;
   throw cix_exit();
}

int main (int argc, char** argv) {
   string host;
   in_port_t port;
   outlog.execname (basename (argv[0]));
   outlog << "starting" << endl;
   vector<string> args (&argv[1], &argv[argc]);
   if (args.size() > 2) usage();
   host = get_cix_server_host (args, 0);
   port = get_cix_server_port (args, 1);
   outlog << to_string (hostinfo()) << endl;
   try {
      outlog << "connecting to " << host << " port " << port << endl;
      client_socket server (host, port);
      outlog << "connected to " << to_string (server) << endl;
      for (;;) {
         string line,str;
         getline (cin, line);
         if (cin.eof()) throw cix_exit();
         vector<string> data;
         istringstream iss(line);
         while(getline(iss,str,' '))
         {
            data.push_back(str);
         }
         outlog << "command " << line << endl;
         const auto& itor = command_map.find (data[0]);    
         cix_command cmd = itor == command_map.end()
                         ? cix_command::ERROR : itor->second;
         switch (cmd) {
            case cix_command::EXIT:
               throw cix_exit();
               break;
            case cix_command::HELP:
               cix_help();
               break;
            case cix_command::LS:
               cix_ls (server);
               break;
            case cix_command::RM:
               cix_rm (server,data[1]);
               break; 
            case cix_command::GET:
               cix_get (server,data[1]);
               break;
            case cix_command::PUT:
               cix_put (server,data[1]);
               break;         
            default:
               outlog << line << ": invalid command" << endl;
               break;
         }
      }
   }catch (socket_error& error) {
      outlog << error.what() << endl;
   }catch (cix_exit& error) {
      outlog << "caught cix_exit" << endl;
   }
   outlog << "finishing" << endl;
   return 0;
}

