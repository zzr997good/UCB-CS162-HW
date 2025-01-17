#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unistd.h>

#include "libhttp.h"
#include "wq.h"

/*
 * Global configuration variables.
 * You need to use these in your implementation of handle_files_request and
 * handle_proxy_request. Their values are set up in main() using the
 * command line arguments (already implemented for you).
 */
wq_t work_queue; // Only used by poolserver
int num_threads; // Only used by poolserver
int server_port; // Default value: 8000
char* server_files_directory;
char* server_proxy_hostname;
int server_proxy_port;

void send_file_header(int fd, char* path) {
  int file_fd = open(path, O_RDONLY);
  if (file_fd < 0) {
    fprintf(stderr, "Failed to create file descriptor %d: %s\n", errno, strerror(errno));
    close(fd);
    exit(errno);
  }
  struct stat file_stat;
  stat(path, &file_stat);
  char file_size[8];
  sprintf(file_size, "%lu", file_stat.st_size);
  http_start_response(fd, 200);
  http_send_header(fd, "Content-Type", http_get_mime_type(path));
  http_send_header(fd, "Content-Length", file_size); // TODO: change this line too
  http_end_headers(fd);
  close(file_fd);
}

/*Send body to client socket
source==NULL means data is in buffer
otherwise means data is in file
*/
void send_body(int fd, char* buffer, ssize_t buffer_size, const char* source) {
  if (source == NULL) {
    write(fd, buffer, buffer_size);
  }
  //Read from file then send to fd
  else {
    int file_fd = open(source, O_RDONLY);
    ssize_t n;
    while ((n = read(file_fd, buffer, buffer_size)) > 0) {
      write(fd, buffer, n);
    }
    close(file_fd);
  }
}

/*
 * Serves the contents the file stored at `path` to the client socket `fd`.
 * It is the caller's reponsibility to ensure that the file stored at `path` exists.
 */

void serve_file(int fd, char* path) {

  /* TODO: PART 2 */
  /* PART 2 BEGIN */
  send_file_header(fd, path);
  char buffer[1024];
  send_body(fd, buffer, 1024, path);
  /* PART 2 END */
}

/*Double the buffer size by 2*/
void double_buffer_size(char** buffer, size_t* buffer_size) {
  (*buffer_size) *= 2;
  *buffer = realloc(*buffer, *buffer_size);
}

/*Concatenate two string, if the source size is not enough, extends it size*/
void concatenate_string(char** source, char* str, size_t* source_size) {
  if (strlen(*source) + strlen(str) + 1 > *source_size) {
    (*source_size) *= 2;
    *source = realloc(*source, *source_size);
  }
  strcat(*source, str);
}

void serve_directory(int fd, char* path) {
  http_start_response(fd, 200);
  http_send_header(fd, "Content-Type", http_get_mime_type(".html"));
  http_end_headers(fd);
  /* TODO: PART 3 */
  /* PART 3 BEGIN */

  // TODO: Open the directory (Hint: opendir() may be useful here)
  DIR* dir;
  struct dirent* dp;
  if ((dir = opendir(path)) == NULL) {
    perror("Cannot open directory");
    exit(1);
  }
  while ((dp = readdir(dir)) != NULL) {
    //Found the index.html
    if (strcmp(dp->d_name, "index.html") == 0) {
      printf("Index exits.\n");
      char index_path[1024];
      index_path[0] = '\0';
      http_format_index(index_path, path);
      serve_file(fd, index_path);
      closedir(dir);
      return;
    }
  }
  /**
   * TODO: For each entry in the directory (Hint: look at the usage of readdir() ),
   * send a string containing a properly formatted HTML. (Hint: the http_format_href()
   * function in libhttp.c may be useful here)
   */

  //Index.html not found
  closedir(dir);
  if ((dir = opendir(path)) == NULL) {
    perror("Cannot open directory");
    exit(1);
  }
  char* html = malloc(1024 * sizeof(char));
  size_t html_size = 1024;
  html[0] = '\0';
  strcat(html, "<!DOCTYPE html>\n<html>\n<head>\n\t<title>File linkes in "
               "directory.</title>\n</head>\n<body>");
  char* child_link = malloc(1024 * sizeof(char));
  size_t link_size = 128;
  child_link[0] = '\0';
  while ((dp = readdir(dir)) != NULL) {
    if (strlen("<a href=\"/") + strlen(path) + strlen("/") + strlen(dp->d_name) + strlen("\">") +
            strlen(dp->d_name) + strlen("</a><br/>") + 1 >
        link_size) {
      printf("Extend links buffer\n");
      double_buffer_size(&child_link, &link_size);
    }
    http_format_href(child_link, path, dp->d_name);
    concatenate_string(&html, "\n\t", &html_size);
    concatenate_string(&html, child_link, &html_size);
  }
  concatenate_string(&html, "\n</body>\n</html>\n", &html_size);
  send_body(fd, html, strlen(html), NULL);
  free(child_link);
  closedir(dir);
  /* PART 3 END */
}

/*
 * Reads an HTTP request from client socket (fd), and writes an HTTP response
 * containing:
 *
 *   1) If user requested an existing file, respond with the file
 *   2) If user requested a directory and index.html exists in the directory,
 *      send the index.html file.
 *   3) If user requested a directory and index.html doesn't exist, send a list
 *      of files in the directory with links to each.
 *   4) Send a 404 Not Found response.
 *
 *   Closes the client socket (fd) when finished.
 */
void handle_files_request(int fd) {

  struct http_request* request = http_request_parse(fd);

  if (request == NULL || request->path[0] != '/') {
    http_start_response(fd, 400);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
    close(fd);
    return;
  }

  if (strstr(request->path, "..") != NULL) {
    http_start_response(fd, 403);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
    close(fd);
    return;
  }

  /* Add `./` to the beginning of the requested path */
  char* path = malloc(2 + strlen(request->path) + 1);
  path[0] = '.';
  path[1] = '/';
  memcpy(path + 2, request->path, strlen(request->path) + 1);

  /*
   * TODO: PART 2 is to serve files. If the file given by `path` exists,
   * call serve_file() on it. Else, serve a 404 Not Found error below.
   * The `stat()` syscall will be useful here.
   *
   * TODO: PART 3 is to serve both files and directories. You will need to
   * determine when to call serve_file() or serve_directory() depending
   * on `path`. Make your edits below here in this function.
   */

  /* PART 2 & 3 BEGIN */
  struct stat file_stat;

  // Use the stat function to get information about the file
  if (stat(path, &file_stat) == 0) {
    if (S_ISREG(file_stat.st_mode)) {
      // The file exists and is a regular file
      printf("File exists.\n");
      serve_file(fd, path);
    } else if (S_ISDIR(file_stat.st_mode)) {
      // The file exists but is not a regular file (e.g., a directory)
      printf("The path exists, but is a directory\n");
      serve_directory(fd, path);
    }
  } else {
    // The file does not exist or there was an error
    printf("File does not exist or an error occurred.\n");
    http_start_response(fd, 404);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
  }
  /* PART 2 & 3 END */
  close(fd);
  printf("Serving finishes. Socket[%d] closed by proxy server\n", fd);
  return;
}

/*data structure for start_routine*/
typedef struct func_arg {
  int read_fd;
  int write_fd;
  pthread_t* depend_thread;
} func_arg_t;

void* forward(void* forward_args) {
  func_arg_t* args = (func_arg_t*)forward_args;
  char buffer[1024];
  ssize_t read_bytes;
  //Read will block until there is enough data
  //If read_bytes==0, it means the TCP connection loses
  while ((read_bytes = read(args->read_fd, buffer, 1024)) > 0) {
    //Write will not influce the read
    write(args->write_fd, buffer, read_bytes);
    printf("Sending data from socket[%d] to socket [%d]\n", args->read_fd, args->write_fd);
  }
  //TCP disconnects
  printf("TCP closes by client/target.\n");
  pthread_cancel(*(args->depend_thread));
  pthread_exit(0);
}
/*
 * Opens a connection to the proxy target (hostname=server_proxy_hostname and
 * port=server_proxy_port) and relays traffic to/from the stream fd and the
 * proxy target_fd. HTTP requests from the client (fd) should be sent to the
 * proxy target (target_fd), and HTTP responses from the proxy target (target_fd)
 * should be sent to the client (fd).
 *
 *   +--------+     +------------+     +--------------+
 *   | client | <-> | httpserver | <-> | proxy target |
 *   +--------+     +------------+     +--------------+
 *
 *   Closes client socket (fd) and proxy target fd (target_fd) when finished.
 */
void handle_proxy_request(int fd) {

  /*
  * The code below does a DNS lookup of server_proxy_hostname and
  * opens a connection to it. Please do not modify.
  */
  struct sockaddr_in target_address;
  memset(&target_address, 0, sizeof(target_address));
  target_address.sin_family = AF_INET;
  target_address.sin_port = htons(server_proxy_port);

  // Use DNS to resolve the proxy target's IP address
  struct hostent* target_dns_entry = gethostbyname2(server_proxy_hostname, AF_INET);

  // Create an IPv4 TCP socket to communicate with the proxy target.
  int target_fd = socket(PF_INET, SOCK_STREAM, 0);
  if (target_fd == -1) {
    fprintf(stderr, "Failed to create a new socket: error %d: %s\n", errno, strerror(errno));
    close(fd);
    exit(errno);
  }

  if (target_dns_entry == NULL) {
    fprintf(stderr, "Cannot find host: %s\n", server_proxy_hostname);
    close(target_fd);
    close(fd);
    exit(ENXIO);
  }

  char* dns_address = target_dns_entry->h_addr_list[0];

  // Connect to the proxy target.
  memcpy(&target_address.sin_addr, dns_address, sizeof(target_address.sin_addr));
  int connection_status =
      connect(target_fd, (struct sockaddr*)&target_address, sizeof(target_address));

  if (connection_status < 0) {
    /* Dummy request parsing, just to be compliant. */
    http_request_parse(fd);

    http_start_response(fd, 502);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
    close(target_fd);
    close(fd);
    return;
  }

  /* TODO: PART 4 */
  /* PART 4 BEGIN */
  pthread_t client_target;
  pthread_t target_client;
  func_arg_t client_target_args = {fd, target_fd, &target_client};
  func_arg_t target_client_args = {target_fd, fd, &client_target};
  pthread_create(&client_target, NULL, &forward, &client_target_args);
  pthread_create(&target_client, NULL, &forward, &target_client_args);
  pthread_join(client_target, NULL);
  pthread_join(target_client, NULL);
  close(target_fd);
  printf("Serving finishes. Socket[%d] closed by proxy server\n", target_fd);
  close(fd);
  printf("Serving finishes. Socket[%d] closed by proxy server\n", fd);
  /* PART 4 END */
}

#ifdef POOLSERVER
/*
 * All worker threads will run this function until the server shutsdown.
 * Each thread should block until a new request has been received.
 * When the server accepts a new connection, a thread should be dispatched
 * to send a response to the client.
 */
void* handle_clients(void* void_request_handler) {
  void (*request_handler)(int) = (void (*)(int))void_request_handler;
  /* (Valgrind) Detach so thread frees its memory on completion, since we won't
   * be joining on it. */
  pthread_detach(pthread_self());

  /* TODO: PART 7 */
  /* PART 7 BEGIN */
  while (1) {
    int client_fd = wq_pop(&work_queue);
    request_handler(client_fd);
  }
  /* PART 7 END */
}

/*
 * Creates `num_threads` amount of threads. Initializes the work queue.
 */
void init_thread_pool(int num_threads, void (*request_handler)(int)) {

  /* TODO: PART 7 */
  /* PART 7 BEGIN */
  wq_init(&work_queue);
  for (int i = 0; i < num_threads; ++i) {
    pthread_t thread;
    pthread_create(&thread, NULL, handle_clients, (void*)request_handler);
    printf("New worker thread created.\n");
  }
  /* PART 7 END */
}
#endif

/*Arguments for start rountine of the new thread to handle request*/
typedef struct thread_args {
  int client_socket_number;
  void (*request_handler)(int);
} thread_args_t;

/*Start rountine of the new thread to handle request*/
void* thread_request_handler(void* _args) {
  thread_args_t* args = (thread_args_t*)_args;
  args->request_handler(args->client_socket_number);
  pthread_exit(0);
}

/*
 * Opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *socket_number. For each accepted
 * connection, calls request_handler with the accepted fd number.
 */
void serve_forever(int* socket_number, void (*request_handler)(int)) {

  struct sockaddr_in server_address, client_address;
  size_t client_address_length = sizeof(client_address);
  int client_socket_number;

  // Creates a socket for IPv4 and TCP.
  *socket_number = socket(PF_INET, SOCK_STREAM, 0);
  if (*socket_number == -1) {
    perror("Failed to create a new socket");
    exit(errno);
  }

  int socket_option = 1;
  if (setsockopt(*socket_number, SOL_SOCKET, SO_REUSEADDR, &socket_option, sizeof(socket_option)) ==
      -1) {
    perror("Failed to set socket options");
    exit(errno);
  }

  // Setup arguments for bind()
  memset(&server_address, 0, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(server_port);

  /*
   * TODO: PART 1
   *
   * Given the socket created above, call bind() to give it
   * an address and a port. Then, call listen() with the socket.
   * An appropriate size of the backlog is 1024, though you may
   * play around with this value during performance testing.
   */

  /* PART 1 BEGIN */
  if (bind(*socket_number, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
    perror("Failed to bind the listening socket");
    exit(errno);
  }
  if (listen(*socket_number, 1024) < 0) {
    perror("Failed to listen the socket");
    exit(errno);
  }
  /* PART 1 END */
  printf("Listening on port %d...\n", server_port);

#ifdef POOLSERVER
  /*
   * The thread pool is initialized *before* the server
   * begins accepting client connections.
   */
  init_thread_pool(num_threads, request_handler);
#endif

  while (1) {
    client_socket_number = accept(*socket_number, (struct sockaddr*)&client_address,
                                  (socklen_t*)&client_address_length);
    if (client_socket_number < 0) {
      perror("Error accepting socket");
      continue;
    }

    printf("Accepted connection from %s on port %d\n", inet_ntoa(client_address.sin_addr),
           client_address.sin_port);

#ifdef BASICSERVER
    /*
     * This is a single-process, single-threaded HTTP server.
     * When a client connection has been accepted, the main
     * process sends a response to the client. During this
     * time, the server does not listen and accept connections.
     * Only after a response has been sent to the client can
     * the server accept a new connection.
     */
    request_handler(client_socket_number);

#elif FORKSERVER
    /*
     * TODO: PART 5
     *
     * When a client connection has been accepted, a new
     * process is spawned. This child process will send
     * a response to the client. Afterwards, the child
     * process should exit. During this time, the parent
     * process should continue listening and accepting
     * connections.
     */

    /* PART 5 BEGIN */
    pid_t pid;
    pid = fork();
    if (pid < 0) {
      perror("Fork child process for serving fails");
      exit(errno);
    } else if (pid == 0) {
      close(*socket_number);
      printf("Child process for serving creates\n");
      request_handler(client_socket_number);
      exit(EXIT_SUCCESS);
    } else {
      close(client_socket_number);
    }
    /* PART 5 END */

#elif THREADSERVER
    /*
     * TODO: PART 6
     *
     * When a client connection has been accepted, a new
     * thread is created. This thread will send a response
     * to the client. The main thread should continue
     * listening and accepting connections. The main
     * thread will NOT be joining with the new thread.
     */

    /* PART 6 BEGIN */
    pthread_t server_thread;
    //Here args->client_number is a copy of client_socket_number
    //So the modification of client_socket_number in main thread will not influce args->client_socket_number
    //Otherwise we need to malloc new memory for client_socket_number
    thread_args_t args = {client_socket_number, request_handler};
    pthread_create(&server_thread, NULL, thread_request_handler, (void*)&args);
    /* PART 6 END */
#elif POOLSERVER
    /*
     * TODO: PART 7
     *
     * When a client connection has been accepted, add the
     * client's socket number to the work queue. A thread
     * in the thread pool will send a response to the client.
     */

    /* PART 7 BEGIN */
    wq_push(&work_queue, client_socket_number);
    /* PART 7 END */
#endif
  }

  shutdown(*socket_number, SHUT_RDWR);
  close(*socket_number);
}

int server_fd;
void signal_callback_handler(int signum) {
  printf("Caught signal %d: %s\n", signum, strsignal(signum));
  printf("Closing socket %d\n", server_fd);
  if (close(server_fd) < 0)
    perror("Failed to close server_fd (ignoring)\n");
  exit(0);
}

char* USAGE =
    "Usage: ./httpserver --files some_directory/ [--port 8000 --num-threads 5]\n"
    "       ./httpserver --proxy inst.eecs.berkeley.edu:80 [--port 8000 --num-threads 5]\n";

void exit_with_usage() {
  fprintf(stderr, "%s", USAGE);
  exit(EXIT_SUCCESS);
}

int main(int argc, char** argv) {
  signal(SIGINT, signal_callback_handler);
  signal(SIGPIPE, SIG_IGN);

  /* Default settings */
  server_port = 8000;
  void (*request_handler)(int) = NULL;

  int i;
  for (i = 1; i < argc; i++) {
    if (strcmp("--files", argv[i]) == 0) {
      request_handler = handle_files_request;
      server_files_directory = argv[++i];
      if (!server_files_directory) {
        fprintf(stderr, "Expected argument after --files\n");
        exit_with_usage();
      }
    } else if (strcmp("--proxy", argv[i]) == 0) {
      request_handler = handle_proxy_request;

      char* proxy_target = argv[++i];
      if (!proxy_target) {
        fprintf(stderr, "Expected argument after --proxy\n");
        exit_with_usage();
      }

      char* colon_pointer = strchr(proxy_target, ':');
      if (colon_pointer != NULL) {
        *colon_pointer = '\0';
        server_proxy_hostname = proxy_target;
        server_proxy_port = atoi(colon_pointer + 1);
      } else {
        server_proxy_hostname = proxy_target;
        server_proxy_port = 80;
      }
    } else if (strcmp("--port", argv[i]) == 0) {
      char* server_port_string = argv[++i];
      if (!server_port_string) {
        fprintf(stderr, "Expected argument after --port\n");
        exit_with_usage();
      }
      server_port = atoi(server_port_string);
    } else if (strcmp("--num-threads", argv[i]) == 0) {
      char* num_threads_str = argv[++i];
      if (!num_threads_str || (num_threads = atoi(num_threads_str)) < 1) {
        fprintf(stderr, "Expected positive integer after --num-threads\n");
        exit_with_usage();
      }
    } else if (strcmp("--help", argv[i]) == 0) {
      exit_with_usage();
    } else {
      fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
      exit_with_usage();
    }
  }

  if (server_files_directory == NULL && server_proxy_hostname == NULL) {
    fprintf(stderr, "Please specify either \"--files [DIRECTORY]\" or \n"
                    "                      \"--proxy [HOSTNAME:PORT]\"\n");
    exit_with_usage();
  }

#ifdef POOLSERVER
  if (num_threads < 1) {
    fprintf(stderr, "Please specify \"--num-threads [N]\"\n");
    exit_with_usage();
  }
#endif

  chdir(server_files_directory);
  serve_forever(&server_fd, request_handler);

  return EXIT_SUCCESS;
}
