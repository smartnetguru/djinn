/* Johann Hauswald
 * jahausw@umich.edu
 * 2014
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <arpa/inet.h>   
#include <unistd.h>  
#include <errno.h>

#include "boost/program_options.hpp" 
#include "socket.h"
#include "thread.h"

using namespace std;
namespace po = boost::program_options;

std::string csv_file_name;
pthread_mutex_t csv_lock;

po::variables_map parse_opts( int ac, char** av )
{
    // Declare the supported options.
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "Produce help message")

        ("portno,p", po::value<int>()->default_value(8080), "Open server on port (default: 8080)")

        ("gpu,u", po::value<bool>()->default_value(false), "Use GPU?")
        ("debug,v", po::value<bool>()->default_value(false), "Turn on all debug")
        ("csv,c", po::value<string>()->default_value("./timing.csv"), "CSV file to put the timings in.")
        ("threadcnt,t", po::value<int>()->default_value(0), "Number of threads to spawn before exiting the server.")
        ;

    po::variables_map vm;
    po::store(po::parse_command_line(ac, av, desc), vm);
    po::notify(vm);    

    if (vm.count("help")) {
        cout << desc << "\n";
        return vm;
    }
    return vm;
}

int main(int argc , char *argv[])
{
    // Main thread for the server
    // Spawn a new thread for each request
    po::variables_map vm = parse_opts(argc, argv);
 
    Caffe::set_phase(Caffe::TEST);
    if(vm["gpu"].as<bool>()){
        Caffe::set_mode(Caffe::GPU);
    }
    else
        Caffe::set_mode(Caffe::CPU);

    // Initialize csv file lock
    if(pthread_mutex_init(&csv_lock, NULL) != 0){
      printf("Mutex init failed.\n");
      exit(1);
    }
    // Set csv file name
    csv_file_name = vm["csv"].as<string>();

    FILE* csv_file = fopen(csv_file_name.c_str(), "w");
    fprintf(csv_file, "REQ, PID, FWD_PASS_LAT,\n");
    fclose(csv_file);

    int total_thread_cnt = vm["threadcnt"].as<int>();

    int server_sock = SERVER_init(vm["portno"].as<int>());

    // Listen on socket
    listen(server_sock, 10);
    printf("Server is listening for request on %d\n", vm["portno"].as<int>());

    // Main Loop
    int thread_cnt = 0;
    while(1) {
        int client_sock;
        pthread_t new_thread_id;
        client_sock = accept(server_sock, (sockaddr*) 0, (unsigned int *) 0);
        if(client_sock == -1)
            printf("Failed to accept.\n");
        else {
            // Create a new thread, pass the socket number to it
            new_thread_id = request_thread_init(client_sock);

        }
       thread_cnt++;
       if(thread_cnt == total_thread_cnt){
         if(pthread_join(new_thread_id, NULL) != 0){
           printf("Failed to join.\n");
           exit(1);
         }
         cudaDeviceReset();
         break;
       }
    }
    return 0;
}