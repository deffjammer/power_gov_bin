/*
Copyright (c) 2012, Intel Corporation

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
    * Neither the name of Intel Corporation nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/* Written by Martin Dimitrov, Carl Strickland */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/types.h>

#include "rapl.h"
#include "socket_utils.h"

#define GET_ENERGY_CMD 1
#define GET_PWR_LIMIT_CMD 2
#define SET_PWR_LIMIT_CMD 3

char         *progname;
const char   *version = "1.0";
unsigned int  num_node = 0;
int           portnum = 54001; 

double        delay_unit = 1000000.0;
double        *prev_sample;
double         prev_time = 0.0; 

void cmdline(int argc, char **argv)
{

    progname = argv[0];

    if (argc != 2) 
    {
        fprintf(stderr, "Using default port number: %d.\n", portnum);;
    }
    else
    {
        portnum = atoi(argv[1]);
    }
}

// Get the total PKG domain energy consumed
// since the last sample and the sample interval
int get_energy_consumed(char *output_buffer)
{
    int i; 
    int err = 0; 
    double new_sample; 
    double delta; 
    double total_energy = 0.0; 
    struct timeval tv;
    double curr_time = 0.0;
    double elapsed_time = 0.0; 

    gettimeofday(&tv, NULL);
    curr_time = (double)(tv.tv_sec) + ((double)(tv.tv_usec)/1000000);
    elapsed_time = curr_time - prev_time; 

    for (i = 0; i < num_node; i++) 
    {
        err = get_pkg_total_energy_consumed(i, &new_sample);
        if(0 == err)
        {
            delta = new_sample - prev_sample[i];

            // Handle wraparound
            if (delta < 0.0) 
            {
                delta += MAX_ENERGY_STATUS_JOULES;
            }
            total_energy += delta; 
            prev_sample[i] = new_sample;
        }

    }

    prev_time = curr_time; 
    snprintf(output_buffer, MAXMSG-1, "%lf %lf %d\n", total_energy, elapsed_time, err);

    return err; 
}

// Read the current power limit on the package domain
// and write it into the output buffer
int get_power_limit(char *output_buffer)
{
    int err = 0; 
    double power_limit_watts; 
    pkg_rapl_power_limit_control_t pkg_plc; 

    err = get_pkg_rapl_power_limit_control(0, &pkg_plc); 
    if(0 == err)
    {
        power_limit_watts = pkg_plc.power_limit_watts_1; 
        snprintf(output_buffer, MAXMSG-1, "%lf %d\n", power_limit_watts, err);
    }
    
    return err; 
}

// Set the power limit on the package domain
// Write the error code into the output buffer
int set_power_limit(unsigned int num_node, double power_limit, char *output_buffer)
{
    int i; 
    int err = 0; 
    pkg_rapl_power_limit_control_t pkg_plc; 

    for(i=0; i< num_node; i++)
    {
        err += get_pkg_rapl_power_limit_control(i, &pkg_plc); 
        if(0 == err)
        {
            pkg_plc.power_limit_watts_1 = power_limit; 
            pkg_plc.limit_time_window_seconds_1 = 1.0; 
            pkg_plc.limit_enabled_1 = 1; 
            pkg_plc.clamp_enabled_1 = 1; 
            pkg_plc.lock_enabled = 0;
            err += set_pkg_rapl_power_limit_control(i, &pkg_plc);
        }
    }

    snprintf(output_buffer, MAXMSG-1, "%lf %d\n", power_limit, err);
    return err; 
}

// Decode client command and respond appropriately
int service_client_request(int connfd, char* input_buffer, char *output_buffer)
{
    int n = 0; 
    int m = 0; 
    int command; 
    int ret = 0; 
    double power_limit; 
    errno = 0; 

    memset(output_buffer, ' ', MAXMSG); 
    memset(input_buffer, ' ', MAXMSG); 

    ret = socket_recv(connfd, input_buffer); 

    if(0 == ret)
    {
        sscanf( input_buffer, "%d %lf", &command, &power_limit);
        switch(command)
        {
            case GET_ENERGY_CMD:
                fprintf(stderr, "Received a get energy command\n");
                if(0 != get_energy_consumed(output_buffer))
                    fprintf(stderr, "Error getting the energy consumed\n");
                socket_send(connfd, output_buffer); 
                break; 

            case GET_PWR_LIMIT_CMD:
                fprintf(stderr, "Received a get power limit command\n");
                if( 0 != get_power_limit(output_buffer))
                    fprintf(stderr, "Error getting the power limit\n");
                socket_send(connfd, output_buffer); 
                break; 

            case SET_PWR_LIMIT_CMD:
                fprintf(stderr, "Received a set power limit %lf command\n", power_limit);
                if(0 != set_power_limit(num_node, power_limit, output_buffer))
                    fprintf(stderr, "Error setting the power limit\n");
                socket_send(connfd, output_buffer); 
                break;

            default: 
                fprintf(stderr, "Bad command %d\n", command);
                break; 
        }
    }

    return ret; 
}

int
main(int argc, char **argv)
{
    int i;  
    int sock = 0; 
    char input_buffer[MAXMSG]; 
    char output_buffer[MAXMSG]; 
    struct sockaddr_in serv_addr; 
    fd_set active_fd_set, read_fd_set;
    struct sockaddr_in clientname;
    socklen_t size;
    errno = 0; 

    // First init the RAPL library
    if (0 != init_rapl()) 
    {
        fprintf(stdout, "Init failed!\n");
        exit(MY_ERROR);
    }
    num_node = get_num_rapl_nodes_pkg();
    prev_sample = (double*) malloc(num_node * sizeof(double));
    get_energy_consumed(output_buffer);

    // Read command line and assign port number
    cmdline(argc, argv);

    // Create socket 
    if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "Error: Could not create socket; %s\n", strerror(errno));
        exit(MY_ERROR); 
    }

    // Bind socket
    memset(&serv_addr, ' ', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(portnum); 
    if(bind(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
    {
        fprintf(stderr, "Error: Could not create socket; %s\n", strerror(errno));
        exit(MY_ERROR); 
    }

    // Start listening on the socket
    listen(sock, 1); 

    FD_ZERO (&active_fd_set);
    FD_SET (sock, &active_fd_set);

    // Wait for connections and service requests
    while(1)
    {
        // Block until input arrives on one or more active sockets.
        read_fd_set = active_fd_set;
        if (select (FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) < 0)
        {
            fprintf(stderr, "Error: select; %s\n", strerror(errno));
            exit(MY_ERROR);
        }

        // Service all the sockets with input pending. 
        for (i = 0; i < FD_SETSIZE; ++i)
        {
            if (FD_ISSET (i, &read_fd_set))
            {
                if (i == sock)
                {
                    // Connection request on original socket. 
                    int new_sock;
                    size = sizeof (clientname);
                    new_sock = accept (sock, (struct sockaddr *) &clientname, &size);
                    if (new_sock < 0)
                    {
                        fprintf(stderr, "Error: accept; %s\n", strerror(errno));
                        exit(MY_ERROR);
                    }
                    fprintf (stderr, "Server: connect from host %s\n", inet_ntoa (clientname.sin_addr));
                    FD_SET (new_sock, &active_fd_set);
                }
                else
                {
                    // Data arriving on an already-connected socket. 
                    if (service_client_request(i, input_buffer, output_buffer) < 0)
                    {
                        close (i);
                        FD_CLR (i, &active_fd_set);
                    }
                }
            } 
        }
        //sleep(1);
    }    
     
    //free memory
    free(output_buffer); 
    free(input_buffer); 
    terminate_rapl();

   return 0; 
}
