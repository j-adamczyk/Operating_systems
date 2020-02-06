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
char* name;

void initialize(char*, char*, char*);
void sigint_handler(int);
void close_client(void);

sock_msg_t get_msg(void);
void delete_msg(sock_msg_t);

void send_msg(sock_msg_t*);
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
    char* unix_path = address;

    if (strlen(unix_path) < 1 || strlen(unix_path) > UNIX_PATH_MAX)
      die("Error: invalid Unix socket path!\n");

    struct sockaddr_un un_addr;
    un_addr.sun_family = AF_UNIX;
    snprintf(un_addr.sun_path, UNIX_PATH_MAX, "%s", unix_path);

    socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socket_fd == -1)
      die_errno();

    if (connect(socket_fd, (const struct sockaddr *) &un_addr, sizeof(un_addr)) == -1)
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

    struct sockaddr_in web_addr;
    memset(&web_addr, 0, sizeof(struct sockaddr_in));

    web_addr.sin_family = AF_INET;
    web_addr.sin_addr.s_addr = in_addr;
    web_addr.sin_port = htons(port_num);

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1)
      die_errno();

    if (connect(socket_fd, (const struct sockaddr *) &web_addr, sizeof(web_addr)) == -1)
      die_errno();
  }
  else
    die("Error: unknown socket type!\n");

  send_empty(REGISTER);
}

void send_msg(sock_msg_t* msg)
{
  write(socket_fd, &msg->type, sizeof(msg->type));
  write(socket_fd, &msg->size, sizeof(msg->size));
  write(socket_fd, &msg->name_size, sizeof(msg->name_size));
  write(socket_fd, &msg->id, sizeof(msg->id));

  if (msg->size > 0)
    write(socket_fd, msg->content, msg->size);

  if (msg->name_size > 0)
    write(socket_fd, msg->name, msg->name_size);
}


void send_empty(sock_msg_type_t type)
{
  sock_msg_t msg =
  {
    type,
    0,
    strlen(name) + 1,
    0,
    NULL,
    name
  };

  send_msg(&msg);
};


void send_done(int id, char *content)
{
  sock_msg_t msg =
  {
    WORK_DONE,
    strlen(content) + 1,
    strlen(name) + 1,
    id,
    content,
    name
  };

  send_msg(&msg);
}

sock_msg_t get_msg(void)
{
  sock_msg_t msg;

  if (read(socket_fd, &msg.type, sizeof(msg.type)) != sizeof(msg.type))
    die("Error: unknown message from server!\n");

  if (read(socket_fd, &msg.size, sizeof(msg.size)) != sizeof(msg.size))
    die("Error: unknown message from server!\n");

  if (read(socket_fd, &msg.name_size, sizeof(msg.name_size)) != sizeof(msg.name_size))
    die("Error: unknown message from server!\n");

  if (read(socket_fd, &msg.id, sizeof(msg.id)) != sizeof(msg.id))
    die("Error: unknown message from server!\n");

  if (msg.size > 0)
  {
    msg.content = malloc(msg.size + 1);

    if (msg.content == NULL)
      die_errno();

    if (read(socket_fd, msg.content, msg.size) != msg.size)
      die("Error: unknown message from server!\n");
  }
  else
    msg.content = NULL;

  if (msg.name_size > 0)
  {
    msg.name = malloc(msg.name_size + 1);

    if (msg.name == NULL)
      die_errno();

    if (read(socket_fd, msg.name, msg.name_size) != msg.name_size)
      die("Error: something wrong with message from server!\n");

  }
  else
    msg.name = NULL;

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
  shutdown(socket_fd, SHUT_RDWR);
  close(socket_fd);
}
