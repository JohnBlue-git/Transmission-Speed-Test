#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h> // for sleep() usleep()

#include <pthread.h>
#include <arpa/inet.h> // for socket
#include <errno.h> // for socket tracebility

#include "functions.h"

#define PORT 8080 // Port
#define MAX_CONNECTIONS 9 // Max 9 connection
#define TIME_OUT 10 // second

//
// Server side
//

void *handleClient(void *arg) {
    int clientSocket = *((int *)arg);
    
    // Respond to ping test
    if ( false == pingResponse(clientSocket) ) {
        pthread_exit(NULL);
    }

    // Handle upload from client 
    if ( false == recvFile(clientSocket) ) {
        pthread_exit(NULL);
    }
    
    // Proceed download to client
    double downloadSpeed = 0.0;
    if ( false == sendFile(clientSocket, &downloadSpeed) ) {
        pthread_exit(NULL);
    }
    
    // Send download speed
    send(clientSocket, &downloadSpeed, 8, 0); // size 8 because double is 8 bytes

    pthread_exit(NULL);
}

void *createServer(void *arg) {
    int serverSocket;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);

    // Create socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        perror("Error: Server side could not create socket");
        pthread_exit(NULL);
    }

    // Initialize server address structure
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    // Force to bind via setting (ref: https://stackoverflow.com/questions/24194961/how-do-i-use-setsockoptso-reuseaddr)
    int reuse = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");
#ifdef SO_REUSEPORT
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEPORT, (const char*)&reuse, sizeof(reuse)) < 0) 
        perror("setsockopt(SO_REUSEPORT) failed");
#endif

    // Bind socket to address
    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Error: Server side bind failed");
        pthread_exit(NULL);
    }

    // Listen for incoming connections
    if (listen(serverSocket, MAX_CONNECTIONS) < 0) {
        perror("Error: Server side listen failed");
        pthread_exit(NULL);
    }

    printf("Server side listening on port %d\n\n", PORT);

    int que = 0;
    int clientSocket[MAX_CONNECTIONS];
    while (1) {
        // Set timeout
        struct timeval timeout;
        timeout.tv_sec = TIME_OUT;
        timeout.tv_usec = 0;
        setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        // Accept incoming connections and create a thread for each client
        clientSocket[que] = accept(serverSocket, (struct sockaddr *)&clientAddr, &clientAddrLen);
        if (clientSocket[que] < 0) {
            perror("Error: Server side accept failed");
            break;
        }

        // Retrieve and print the client's IP address
        //char client_ip[INET_ADDRSTRLEN];
        //inet_ntop(AF_INET, &clientAddr.sin_addr, client_ip, sizeof(client_ip));
        //printf("Client connected: %s:%d\n", client_ip, ntohs(clientAddr.sin_port));
        
        // Create thread for client
        pthread_t tid;
        if (pthread_create(&tid, NULL, handleClient, &clientSocket[que]) != 0) {
            perror("Error: Server side could not create thread for client");
            pthread_join(tid, NULL);
            continue;
        }
        pthread_detach(tid);
        
        // To next accept
        if (++que == MAX_CONNECTIONS) {
            que = 0;
        }
    }

    // Close server socket
    close(serverSocket);

    pthread_exit(NULL);
}

//
// Main
//

int main()
{
    // Create server thread
    pthread_t tid;
    if (pthread_create(&tid, NULL, createServer, NULL) != 0) {
        perror("Error: Could not create Server side");
        exit(EXIT_FAILURE);
    }
    
    // Join thread
    pthread_join(tid, NULL);
    
   return EXIT_SUCCESS;
}
