#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h> 

#include "socket_utils.h"

#define GET_ENERGY_CMD 1
#define GET_PWR_LIMIT_CMD 2
#define SET_PWR_LIMIT_CMD 3

int           portnum = 54001;

// Talk to the server and get energy consumed (since the last query)
int get_energy_consumed(int sock, char* input_buffer, char *output_buffer)
{
    int n=0; 
    int err = 0; 
    double energy = 0.0; 
    double elapsed_time;

    memset(input_buffer, ' ', MAXMSG);
    memset(output_buffer, ' ', MAXMSG);

    // Send the GET_ENERGY_CMD command to server
    snprintf(output_buffer, MAXMSG-1, "%d %lf\n", GET_ENERGY_CMD, energy);
    err = socket_send(sock, output_buffer); 

    // Receive response from the server
    err = socket_recv(sock, input_buffer); 
    if(0 == err)
    {
        sscanf( input_buffer, "%lf %lf %d", &energy, &elapsed_time, &err);
        fprintf(stderr,"=====================================\n"); 
        fprintf(stderr,"The energy consumed (since the last time I asked) is: %lf\n", energy); 
        fprintf(stderr,"Elapsed time between samples is: %lf\n", elapsed_time);
        fprintf(stderr,"The average power consumed is: %lf error_code: %d\n", energy / elapsed_time, err); 
        fprintf(stderr,"=====================================\n\n"); 
    }
    
    return err; 
}

// Talk to the server and get the current power limit at the package domain
int get_power_limit(int sock, char* input_buffer, char *output_buffer)
{
    int n=0; 
    int err = 0; 
    double power_limit = 0.0; 

    memset(input_buffer, ' ', MAXMSG);
    memset(output_buffer, ' ', MAXMSG);

    // Send the GET_PWR_LIMIT command to server
    snprintf(output_buffer, MAXMSG-1, "%d %lf\n", GET_PWR_LIMIT_CMD, power_limit);
    err = socket_send(sock, output_buffer); 

    // Receive response from the server
    err = socket_recv(sock, input_buffer); 
    if(0 == err)
    {
        sscanf( input_buffer, "%lf %d", &power_limit, &err);
        fprintf(stderr,"The current power limit is: %lf. Error code: %d\n", power_limit, err);
    }
    
    return err; 
}

// Talk to the server and set the power limit at the package domain
int set_power_limit(int sock, char* input_buffer, 
                    char *output_buffer, double power_limit)
{
    int n=0; 
    int err = 0; 
    double new_power_limit = 0.0; 

    memset(input_buffer, ' ', MAXMSG);
    memset(output_buffer, ' ', MAXMSG);

    // Send the SET_PWR_LIMIT command to server
    snprintf(output_buffer, MAXMSG-1, "%d %lf\n", SET_PWR_LIMIT_CMD, power_limit);
    err = socket_send(sock, output_buffer); 

    // Receive response from the server
    err = socket_recv(sock, input_buffer); 
    if(0 == err)
    {
        sscanf( input_buffer, "%lf %d", &new_power_limit, &err);
        fprintf(stderr,"The power limit was set to %lf. Error code: %d\n", new_power_limit, err);
    }

    return err; 
}

int main(int argc, char *argv[])
{
    int sock = 0; 
    int err = 0;
    char input_buffer[MAXMSG];
    char output_buffer[MAXMSG];
    struct sockaddr_in serv_addr; 
    errno = 0; 
    double power_limit = 0.0; 

    if(argc != 2)
    {
        fprintf(stderr, "\n Usage: %s <ip of server> \n", argv[0]);
        exit(MY_ERROR);
    } 

    memset(&serv_addr, ' ', sizeof(serv_addr)); 

    if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr,"Error: Could not create socket; %s\n", strerror(errno));
        exit(MY_ERROR);
    } 

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portnum); 

    if(inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) <= 0)
    {
        fprintf(stderr,"Error: inet_pton error occured; %s\n", strerror(errno));
        exit(MY_ERROR);
    } 

    if( connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        fprintf(stderr,"Error: Connect Failed; %s\n", strerror(errno));
        exit(MY_ERROR);
    }    

    // Send commands to server
    err = get_power_limit(sock, input_buffer, output_buffer);
    err = set_power_limit(sock, input_buffer, output_buffer, 110.0);
    err = get_power_limit(sock, input_buffer, output_buffer);
    err = set_power_limit(sock, input_buffer, output_buffer, 120.0);
    err = get_power_limit(sock, input_buffer, output_buffer);
    err = set_power_limit(sock, input_buffer, output_buffer, 130.0);
    err = get_power_limit(sock, input_buffer, output_buffer);

    err = get_energy_consumed(sock, input_buffer, output_buffer);
    sleep(2); 
    err = get_energy_consumed(sock, input_buffer, output_buffer);
    sleep(2); 
    err = get_energy_consumed(sock, input_buffer, output_buffer);
    sleep(2); 
    err = get_energy_consumed(sock, input_buffer, output_buffer);
    sleep(2); 
    err = get_energy_consumed(sock, input_buffer, output_buffer);
    
    close(sock); 

    return err;
}
