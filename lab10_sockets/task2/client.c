#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "utils.h"

int socket_fd;
char *name;

void initialize(char*, char*, char*);
void sigint_handler(int);
void close_client(void);

sock_msg_t get_msg(void);
void delete_msg(sock_msg_t);

void send_msg(sock_msg_t msg);
void send_empty(sock_msg_type_t);
void send_done(int, char*);


int main(int argc, char **argv)
{
  if (argc != 4)
    die("Error: wrong number of arguments! There should be:\n- client name\n- socket type (UNIX or WEB)\n- address (filepath to socket or IP address with port)\n");

  initialize(argv[1], argv[2], argv[3]);

  while (true)
  {
    sock_msg_t msg = get_msg();

    switch (msg.type)
    {
      case OK:
        break;

      case PING:
      {
        send_empty(PONG);
        break;
      }

      case NAME_TAKEN:
        die("Error: name already taken!\n");

      case FULL:
        die("Error: server already full!\n");

      case WORK:
      {
        printf("Received job.\n");

        // simulate complex computations
        sleep(10);

        char *buffer = malloc(100 + 2 * msg.size);
        if (buffer == NULL)
          die_errno();

        sprintf(buffer, "echo '%s' | awk '{for(x=1;$x;++x)print $x}' | sort | uniq -c", msg.content);

        FILE *result = popen(buffer, "r");
        if (result == 0)
        {
          free(buffer);
          break;
        }

        int n = fread(buffer, 1, 99 + 2 * msg.size, result);
        buffer[n] = '\0';

        printf("Job done.\n");

        send_done(msg.id, buffer);
        free(buffer);
        break;
      }

      default: break;
    }

    delete_msg(msg);
  }
}


void initialize(char* n, char* variant, char* address)
{
  if (atexit(close_client) == -1)
    die_errno();

  if (signal(SIGINT, sigint_handler) == SIG_ERR)
    print_errno();

  name = n;

  // parse address
  if (strcmp("UNIX", variant) == 0)
  {
    char *unix_path = address;

    if (strlen(unix_path) < 1 || strlen(unix_path) > UNIX_PATH_MAX)
      die("Error: invalid Unix socket path!\n");

    struct sockaddr_un unix_addr;
    unix_addr.sun_family = AF_UNIX;
    snprintf(unix_addr.sun_path, UNIX_PATH_MAX, "%s", unix_path);

    struct sockaddr_un client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sun_family = AF_UNIX;
    snprintf(client_addr.sun_path, UNIX_PATH_MAX, "%s", name);

    socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (socket_fd == -1)
      die_errno();

    if (bind(socket_fd, (struct sockaddr *) &client_addr, sizeof(client_addr)) == -1)
      die_errno();

    if (connect(socket_fd, (struct sockaddr *) &unix_addr, sizeof(unix_addr)) == -1)
      die_errno();
  }
  else if (strcmp("WEB", variant) == 0)
  {
    strtok(address, ":");
    char *port = strtok(NULL, ":");

    if (port == NULL)
      die("Error: invalid port! It must be specified in IP address!\n");

    int in_addr = inet_addr(address);
    if (in_addr == INADDR_NONE)
      die("Error: invalid address!\n");

    int port_num = atoi(port);
    if (port_num < 1024)
      die("Error: invalid port number! It must be a positive integer < 1024!\n");

    socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd == -1)
      die_errno();

    struct sockaddr_in web_addr;
    memset(&web_addr, 0, sizeof(struct sockaddr_in));

    web_addr.sin_family = AF_INET;
    web_addr.sin_addr.s_addr = in_addr;
    web_addr.sin_port = htons(port_num);

    if (connect(socket_fd, (struct sockaddr *) &web_addr, sizeof(web_addr)) == -1)
      die_errno();
  }
  else
    die("Error: unknown socket type!\n");

  send_empty(REGISTER);
}


void send_msg(sock_msg_t msg)
{
  ssize_t head_size = sizeof(msg.type) + sizeof(msg.size) + sizeof(msg.name_size) + sizeof(msg.id);
  ssize_t size = head_size + msg.size + 1 + msg.name_size + 1;
  int8_t *buff = malloc(size);
  if (buff == NULL)
    die_errno();

  memcpy(buff, &msg.type, sizeof(msg.type));
  memcpy(buff + sizeof(msg.type), &msg.size, sizeof(msg.size));
  memcpy(buff + sizeof(msg.type) + sizeof(msg.size), &msg.name_size, sizeof(msg.name_size));
  memcpy(buff + sizeof(msg.type) + sizeof(msg.size) + sizeof(msg.name_size), &msg.id, sizeof(msg.id));

  if (msg.size > 0 && msg.content != NULL)
    memcpy(buff + head_size, msg.content, msg.size + 1);

  if (msg.name_size > 0 && msg.name != NULL)
    memcpy(buff + head_size + msg.size + 1, msg.name, msg.name_size + 1);

  if (write(socket_fd, buff, size) != size)
    die_errno();

  free(buff);
}


void send_empty(sock_msg_type_t type)
{
  sock_msg_t msg = { type, 0, strlen(name), 0, NULL, name };
  send_msg(msg);
};


void send_done(int id, char *content)
{
  sock_msg_t msg = { WORK_DONE, strlen(content), strlen(name), id, content, name };
  send_msg(msg);
}


sock_msg_t get_msg(void)
{
  sock_msg_t msg;
  ssize_t head_size = sizeof(msg.type) + sizeof(msg.size) + sizeof(msg.name_size) + sizeof(msg.id);
  int8_t buff[head_size];
  if (recv(socket_fd, buff, head_size, MSG_PEEK) < head_size)
    die("Error: unknown message from server!\n");

  memcpy(&msg.type, buff, sizeof(msg.type));
  memcpy(&msg.size, buff + sizeof(msg.type), sizeof(msg.size));
  memcpy(&msg.name_size, buff + sizeof(msg.type) + sizeof(msg.size), sizeof(msg.name_size));
  memcpy(&msg.id, buff + sizeof(msg.type) + sizeof(msg.size) + sizeof(msg.name_size), sizeof(msg.id));

  ssize_t size = head_size + msg.size + 1 + msg.name_size + 1;
  int8_t *buffer = malloc(size);

  if (recv(socket_fd, buffer, size, 0) < size)
    die("Error: unknown message from server!\n");

  if (msg.size > 0)
  {
    msg.content = malloc(msg.size + 1);
    if (msg.content == NULL)
      die_errno();

    memcpy(msg.content, buffer + head_size, msg.size + 1);
  }
  else
    msg.content = NULL;

  if (msg.name_size > 0)
  {
    msg.name = malloc(msg.name_size + 1);
    if (msg.name == NULL)
      die_errno();

    memcpy(msg.name, buffer + head_size + msg.size + 1, msg.name_size + 1);
  }
  else
    msg.name = NULL;

  free(buffer);

  return msg;
}


void delete_msg(sock_msg_t msg)
{
  if (msg.content != NULL)
    free(msg.content);

  if (msg.name != NULL)
    free(msg.name);
}


void sigint_handler(int signum)
{ exit(0); }


void close_client(void)
{
  printf("Client \'%s\' ending work.\n", name);
  send_empty(UNREGISTER);
  unlink(name);
  shutdown(socket_fd, SHUT_RDWR);
  close(socket_fd);
}
