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
wq_t work_queue;
int num_threads;
int server_port;
char *server_files_directory;
char *server_proxy_hostname;
int server_proxy_port;

#define MAX_FILE_SIZE 128
#define BUFFER_SIZE 8192
#define MAX_STRING_BUFFER_SIZE 8192

typedef struct proxy_thread_status {
  int src_fd;
  int dst_fd;
  pthread_cond_t *cond;
  int alive;
  int *finished;
} proxy_thread_status;


void send_fd(int dst_fd, int src_fd) {
  void *buffer = malloc(BUFFER_SIZE);
  size_t size;
  while ((size = read(src_fd, buffer, BUFFER_SIZE)) > 0) {
    http_send_data(dst_fd, buffer, size);
  }
  free(buffer);
}

/*
 * Serves the contents the file stored at `path` to the client socket `fd`.
 * It is the caller's reponsibility to ensure that the file stored at `path` exists.
 * You can change these functions to anything you want.
 * 
 * ATTENTION: Be careful to optimize your code. Judge is
 *            sesnsitive to time-out errors.
 */
void serve_file(int fd, char *path, struct stat *st) {

  http_start_response(fd, 200);
  http_send_header(fd, "Content-Type", http_get_mime_type(path));

  long size = st->st_size;
  char *file_size = malloc(MAX_FILE_SIZE * sizeof(char));
  sprintf(file_size, "%ld", size);
  http_send_header(fd, "Content-Length", file_size);

  http_end_headers(fd);

  int file = open(path, O_RDONLY);
  send_fd(fd, file);
  free(file_size);
  close(file);
}

void send_index(int fd, char *path) {
  char *index_path = malloc(strlen(path) + strlen("/index.html"));
  strcpy(index_path, path);
  strcat(index_path, "/index.html");
  int index_fd = open(index_path, O_RDONLY);
  free(index_path);
  send_fd(fd, index_fd);
  close(index_fd);
}

void serve_directory(int fd, char *path) {
  http_start_response(fd, 200);
  http_send_header(fd, "Content-Type", http_get_mime_type(".html"));
  http_end_headers(fd);

  DIR *dir = opendir(path);
  struct dirent *dfile;
  char *string_buffer = malloc(MAX_STRING_BUFFER_SIZE);
  while ((dfile = readdir(dir)) != NULL)
  {
    if (strcmp(dfile->d_name, "index.html") == 0) {
      send_index(fd, path);
      free(string_buffer);
      return;
    } else {
      int buffer_size = strlen("<a href=./></a><br>\n") + strlen(dfile->d_name)*2 + 1;
      char *buffer = malloc(buffer_size);
      sprintf(buffer, "<a href=./%s>%s</a><br>\n", dfile->d_name, dfile->d_name);
      strcat(string_buffer, buffer);
      free(buffer);
    }
  }
  http_send_string(fd, string_buffer);
  free(string_buffer);
  closedir(dir);
}

void serve_404_not_found(int fd) {
  http_start_response(fd, 404);
  http_send_header(fd, "Content-Type", "text/html");
  http_end_headers(fd);
}


/*
 * Reads an HTTP request from stream (fd), and writes an HTTP response
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

  struct http_request *request = http_request_parse(fd);

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

  /* Remove beginning `./` */
  char *path = malloc(strlen(server_files_directory) + strlen(request->path));
  strcpy(path, server_files_directory);
  strcat(path, request->path);

  /* 
   * TODO: First is to serve files. If the file given by `path` exists,
   * call serve_file() on it. Else, serve a 404 Not Found error below.
   *
   * TODO: Second is to serve both files and directories. You will need to
   * determine when to call serve_file() or serve_directory() depending
   * on `path`.
   *  
   * Feel FREE to delete/modify anything on this function.
   */

  struct stat st;
  int return_value = stat(path, &st);

  if (return_value == 0) {
    if (S_ISREG(st.st_mode)) {
      serve_file(fd, path, &st);
    } else {
      serve_directory(fd, path);
    }
  } else {
    serve_404_not_found(fd);
  }

  free(path);
  close(fd);
  return;
}

void *serve_proxy_thread(void *args) {
  proxy_thread_status *status = (proxy_thread_status *) args;

  printf("%d %d\n", status->src_fd, status->dst_fd);
  //send_fd(status->dst_fd, status->src_fd);
  char *buffer = malloc(BUFFER_SIZE);
  size_t size;
  while ((size = read(status->src_fd, buffer, BUFFER_SIZE - 1)) > 0 && !(*status->finished)) {
    printf("heeeeeeeeeeeeeeeeeeeeeeey %ld\n", size);
    http_send_data(status->dst_fd, buffer, size);
  }
  free(buffer);

  printf("hey %d %d\n", status->src_fd, status->dst_fd); 

  status->alive = 0;
  *(status->finished) = 1;
  pthread_cond_signal(status->cond);
  return NULL;
}


/*
 * Opens a connection to the proxy target (hostname=server_proxy_hostname and
 * port=server_proxy_port) and relays traffic to/from the stream fd and the
 * proxy target. HTTP requests from the client (fd) should be sent to the
 * proxy target, and HTTP responses from the proxy target should be sent to
 * the client (fd).
 *
 *   +--------+     +------------+     +--------------+
 *   | client | <-> | httpserver | <-> | proxy target |
 *   +--------+     +------------+     +--------------+
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

  struct hostent *target_dns_entry = gethostbyname2(server_proxy_hostname, AF_INET);

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

  char *dns_address = target_dns_entry->h_addr_list[0];

  memcpy(&target_address.sin_addr, dns_address, sizeof(target_address.sin_addr));
  int connection_status = connect(target_fd, (struct sockaddr*) &target_address,
      sizeof(target_address));

  if (connection_status < 0) {
    /* Dummy request parsing, just to be compliant. */
    http_request_parse(fd);

    http_start_response(fd, 502);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
    http_send_string(fd, "<center><h1>502 Bad Gateway</h1><hr></center>");
    close(target_fd);
    close(fd);
    return;

  }

  /* 
  * TODO: Your solution for task 3 belongs here! 
  */
  proxy_thread_status *proxy_request = malloc(sizeof(proxy_thread_status));
  proxy_thread_status *proxy_response = malloc(sizeof(proxy_thread_status));
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
  int finished = 0;

  proxy_request->src_fd = fd;
  proxy_request->dst_fd = target_fd;
  proxy_request->cond = &cond;
  proxy_request->alive = 1;
  proxy_request->finished = &finished;

  proxy_response->src_fd = target_fd;
  proxy_response->dst_fd = fd;
  proxy_response->cond = &cond;
  proxy_response->alive = 1;
  proxy_response->finished = &finished;

  pthread_t proxy_threads[2];
  pthread_create(proxy_threads, NULL, serve_proxy_thread, proxy_request);
  pthread_create(proxy_threads + 1, NULL, serve_proxy_thread, proxy_response);

  while (!finished) {
    pthread_cond_wait(&cond, &mutex);
  }

  pthread_mutex_destroy(&mutex);
  pthread_cond_destroy(&cond);

  pthread_cancel(proxy_threads[0]);
  pthread_cancel(proxy_threads[1]);

  free(proxy_request);
  free(proxy_response);

  close(target_fd);
  close(fd);
  printf("salam\n");
}

void *serve_thread(void *args) {
  void (*request_handler)(int) = args;
  while (1) {
    int fd = wq_pop(&work_queue);
		request_handler(fd);
		close(fd);
  }
}


void init_thread_pool(int num_threads, void (*request_handler)(int)) {
  wq_init(&work_queue);
  pthread_t thread_pool[num_threads + 1];
  for (int i = 0; i < num_threads + 1; i++) {
    pthread_create(&thread_pool[i], NULL, serve_thread, request_handler);
  }
}

/*
 * Opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *socket_number. For each accepted
 * connection, calls request_handler with the accepted fd number.
 */
void serve_forever(int *socket_number, void (*request_handler)(int)) {

  struct sockaddr_in server_address, client_address;
  size_t client_address_length = sizeof(client_address);
  int client_socket_number;

  *socket_number = socket(PF_INET, SOCK_STREAM, 0);
  if (*socket_number == -1) {
    perror("Failed to create a new socket");
    exit(errno);
  }

  int socket_option = 1;
  if (setsockopt(*socket_number, SOL_SOCKET, SO_REUSEADDR, &socket_option,
        sizeof(socket_option)) == -1) {
    perror("Failed to set socket options");
    exit(errno);
  }

  memset(&server_address, 0, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(server_port);

  if (bind(*socket_number, (struct sockaddr *) &server_address,
        sizeof(server_address)) == -1) {
    perror("Failed to bind on socket");
    exit(errno);
  }

  if (listen(*socket_number, 1024) == -1) {
    perror("Failed to listen on socket");
    exit(errno);
  }

  printf("Listening on port %d...\n", server_port);

  init_thread_pool(num_threads, request_handler);

  while (1) {
    client_socket_number = accept(*socket_number,
        (struct sockaddr *) &client_address,
        (socklen_t *) &client_address_length);
    if (client_socket_number < 0) {
      perror("Error accepting socket");
      continue;
    }

    printf("Received connection from %s on port %d\n",
        inet_ntoa(client_address.sin_addr),
        client_address.sin_port);

    wq_push(&work_queue, client_socket_number);

    /*if (num_threads == 0) {
     request_handler(client_socket_number);
     close(client_socket_number);
    } else {
      wq_push(&work_queue, client_socket_number);
    }*/

  }

  shutdown(*socket_number, SHUT_RDWR);
  close(*socket_number);
}

int server_fd;
void signal_callback_handler(int signum) {
  printf("Caught signal %d: %s\n", signum, strsignal(signum));
  printf("Closing socket %d\n", server_fd);
  if (close(server_fd) < 0) perror("Failed to close server_fd (ignoring)\n");
  exit(0);
}

char *USAGE =
  "Usage: ./httpserver --files files/ [--port 8000 --num-threads 5]\n"
  "       ./httpserver --proxy ce.sharif.edu:80 [--port 8000 --num-threads 5]\n";

void exit_with_usage() {
  fprintf(stderr, "%s", USAGE);
  exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
  signal(SIGINT, signal_callback_handler);
  signal(SIGPIPE, SIG_IGN);

  /* Default settings */
  server_port = 8000;
  void (*request_handler)(int) = NULL;

  int i;
  for (i = 1; i < argc; i++) {
    if (strcmp("--files", argv[i]) == 0) {
      request_handler = handle_files_request;
      free(server_files_directory);
      server_files_directory = argv[++i];
      if (!server_files_directory) {
        fprintf(stderr, "Expected argument after --files\n");
        exit_with_usage();
      }
    } else if (strcmp("--proxy", argv[i]) == 0) {
      request_handler = handle_proxy_request;

      char *proxy_target = argv[++i];
      if (!proxy_target) {
        fprintf(stderr, "Expected argument after --proxy\n");
        exit_with_usage();
      }

      char *colon_pointer = strchr(proxy_target, ':');
      if (colon_pointer != NULL) {
        *colon_pointer = '\0';
        server_proxy_hostname = proxy_target;
        server_proxy_port = atoi(colon_pointer + 1);
      } else {
        server_proxy_hostname = proxy_target;
        server_proxy_port = 80;
      }
    } else if (strcmp("--port", argv[i]) == 0) {
      char *server_port_string = argv[++i];
      if (!server_port_string) {
        fprintf(stderr, "Expected argument after --port\n");
        exit_with_usage();
      }
      server_port = atoi(server_port_string);
    } else if (strcmp("--num-threads", argv[i]) == 0) {
      char *num_threads_str = argv[++i];
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

  serve_forever(&server_fd, request_handler);

  return EXIT_SUCCESS;
}
