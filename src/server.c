#include "server.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

/* --- Globals --- */
static spotd_server_callbacks *g_callbacks;

/* --- Function definitions --- */
static void *server_thread(void *socket_desc);
static void *connection_handler(void *socket_desc);

/* -- Functions --- */

/**
 * Start the SPOTD server
 *
 * @param  port  The port to start the server on
 * @param  callbacks  The callbacks struct, to receive commands from clients
 */
spotd_error start_server(int port, spotd_server_callbacks *callbacks) {
  int socket_desc, *socket_desc_copy;
  struct sockaddr_in server;

  g_callbacks = callbacks;

  // Create socket
  socket_desc = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_desc == -1) {
    printf("Could not create socket");
    return SPOTD_ERROR_OTHER_PERMANENT;
  }
  puts("Socket created");

  // Prepare the sockaddr_in structure
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons( port );

  // Bind
  if (bind(socket_desc,(struct sockaddr *)&server, sizeof(server)) < 0) {
    // Print the error message
    perror("Bind failed. Error");
    return SPOTD_ERROR_BIND_FAILED;
  }
  puts("bind done");

  // Start the server thread
  pthread_t server_thread_id;

  socket_desc_copy = (int *)malloc(sizeof(int));
  *socket_desc_copy = socket_desc;

  if (pthread_create(&server_thread_id, NULL, server_thread, (void*) socket_desc_copy) < 0) {
    perror("could not create thread");
    return SPOTD_ERROR_OTHER_PERMANENT;
  }

  return SPOTD_ERROR_OK;
}

/**
 * Start the server thread
 *
 * @param  socket_desc  The server socket descriptor
 */
static void *server_thread(void *socket_desc) {
  int sock = *(int*)socket_desc;
  int client_sock, c;
  struct sockaddr_in client;

  free(socket_desc);

  // Listen
  listen(sock, 3);

  // Accept and incoming connection
  puts("Waiting for incoming connections...");
  c = sizeof(struct sockaddr_in);

  // Accept and incoming connection
  puts("Waiting for incoming connections...");
  c = sizeof(struct sockaddr_in);

  pthread_t thread_id;

  while ((client_sock = accept(sock, (struct sockaddr *)&client, (socklen_t*)&c))) {
    puts("Connection accepted");

    int *client_sock_copy = (int *)malloc(sizeof(int));
    *client_sock_copy = client_sock;

    if (pthread_create(&thread_id, NULL, connection_handler, (void*) client_sock_copy) < 0) {
      perror("could not create thread");
      return 0;
    }

    // Now join the thread, so that we dont terminate before the thread
    // pthread_join(thread_id, NULL);
    puts("Handler assigned");
  }

  if (client_sock < 0) {
    perror("accept failed");
    return 0;
  }

  return 0;
}

/**
 * Handle connections
 *
 * @param  socket_desc  The client socket descriptor
 */
static void *connection_handler(void *socket_desc) {
  // Get the socket descriptor
  int sock = *(int*)socket_desc;
  int read_size;
  char message_buf[2000], client_message[2000], *message;

  free(socket_desc);

  // Send some messages to the client
  snprintf(message_buf, 2000, "spotd v%s\n", VERSION);
  write(sock, message_buf, strlen(message_buf));

  // Receive a message from client
  while((read_size = recv(sock, client_message, 2000, 0)) > 0) {
    // end of string marker
    client_message[read_size] = '\0';

    // Pass the message to a callback, if it is set
    if (g_callbacks->command_received != NULL) {
      g_callbacks->command_received(client_message);
    }

    // Send the response
    message = "OK\n";
    write(sock, message, strlen(message));

    // clear the message buffer
    memset(client_message, 0, 2000);
  }

  if (read_size == 0) {
    puts("Client disconnected");
    fflush(stdout);
  } else if(read_size == -1) {
    perror("recv failed");
  }

  return 0;
}