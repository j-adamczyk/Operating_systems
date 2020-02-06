
#include <arpa/inet.h>
#include <netinet/in.h>
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
#include <unistd.h>

#include "utils.h"

int local_socket;
int web_socket;
int epoll_fd;
char *unix_path;

int op_num = 1;

pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;
client_t clients[MAX_CLIENTS];

pthread_t pinger;
pthread_t commander;

void initialize(char *, char *);
void sigint_handler(int);
void close_server(void);

void *commander_start_routine(void *);
void *pinger_start_routine(void *);

void handle_message(int);

sock_msg_t get_msg(int, struct sockaddr *, socklen_t *);
void delete_msg(sock_msg_t);

void send_msg(int sock, sock_msg_t msg);
void send_empty(int, sock_msg_type_t);

void unregister_client(int);

int get_client_index_from_name(char *);


int main(int argc, char **argv)
{
  if (argc != 3)
    die("Error: wrong number of arguments! There should be:\n- TCP port\n- Unix socket path\n");

  initialize(argv[1], argv[2]);

  struct epoll_event event;
  while (true)
  {
    if (epoll_wait(epoll_fd, &event, 1, -1) == -1)
      die_errno();

    handle_message(event.data.fd);
  }
}


void initialize(char *port, char *unix_path)
{
  if (atexit(close_server) == -1)
    die_errno();

  if (signal(SIGINT, sigint_handler) == SIG_ERR)
    print_errno();

  // parse port
  int port_num = atoi(port);
  if (port_num < 1024)
    die("Error: invalid port number! It must be a positive integer < 1024!\n");

  // parse unix path
  unix_path = unix_path;
  if (strlen(unix_path) < 1 || strlen(unix_path) > UNIX_PATH_MAX)
    die("Error: invalid Unix socket path!\n");

  // local socket
  struct sockaddr_un un_addr;
  un_addr.sun_family = AF_UNIX;

  snprintf(un_addr.sun_path, UNIX_PATH_MAX, "%s", unix_path);

  local_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
  if (local_socket < 0)
    die_errno();

  if (bind(local_socket, (struct sockaddr *) &un_addr, sizeof(un_addr)))
    die_errno();

  // web socket
  struct sockaddr_in web_addr;
  memset(&web_addr, 0, sizeof(struct sockaddr_in));
  web_addr.sin_family = AF_INET;
  web_addr.sin_addr.s_addr = inet_addr("0.0.0.0");
  web_addr.sin_port = htons(port_num);

  web_socket = socket(AF_INET, SOCK_DGRAM, 0);
  if (web_socket < 0)
    die_errno();

  if (bind(web_socket, (struct sockaddr *) &web_addr, sizeof(web_addr)))
    die_errno();

  // epoll
  struct epoll_event event;
  event.events = EPOLLIN | EPOLLPRI;

  epoll_fd = epoll_create1(0);
  if (epoll_fd == -1)
    die_errno();

  event.data.fd = local_socket;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, local_socket, &event) == -1)
    die_errno();

  event.data.fd = web_socket;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, web_socket, &event) == -1)
    die_errno();

  // threads
  if (pthread_create(&commander, NULL, commander_start_routine, NULL) != 0)
    die_errno();

  if (pthread_create(&pinger, NULL, pinger_start_routine, NULL) != 0)
    die_errno();

  // detach instead of join later
  if (pthread_detach(commander) != 0)
    die_errno();

  if (pthread_detach(pinger) != 0)
    die_errno();
}


void *commander_start_routine(void *params)
{
  char buffer[1024];
  while (true)
  {
    int min_i = MAX_CLIENTS;
    int min = 1000000;

    // read command
    printf(">");
    scanf("%1023s", buffer);

    // read file
    FILE *file = fopen(buffer, "r");
    if (file == NULL)
    {
      print_errno();
      continue;
    }
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0L, SEEK_SET);

    char *file_buff = malloc(size + 1);
    if (file_buff == NULL)
    {
      print_errno();
      continue;
    }

    file_buff[size] = '\0';

    if (fread(file_buff, 1, size, file) != size)
    {
      fprintf(stderr, "Error: could not read whole file!\n");
      free(file_buff);
      continue;
    }

    fclose(file);

    // send request
    pthread_mutex_lock(&client_mutex);

    // find client with least jobs
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
      if (!clients[i].fd)
        continue;

      if (min > clients[i].jobs)
      {
        min_i = i;
        min = clients[i].jobs;
      }
    }

    if (min_i < MAX_CLIENTS)
    {
      sock_msg_t msg = { WORK, strlen(file_buff) + 1, 0, ++op_num, file_buff, NULL };
      printf("Job number %d sent to client \'%s\'.\n", op_num, clients[min_i].name);
      send_msg(min_i, msg);
      clients[min_i].jobs++;
    }
    else
      fprintf(stderr, "No clients connected\n");

    pthread_mutex_unlock(&client_mutex);

    free(file_buff);
  }
}


void *pinger_start_routine(void *params)
{
  while (true)
  {
    pthread_mutex_lock(&client_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
      if (clients[i].fd == 0)
        continue;

      if (clients[i].inactive)
        unregister_client(i);
      else
      {
        clients[i].inactive = 1;
        send_empty(i, PING);
      }
    }
    pthread_mutex_unlock(&client_mutex);
    sleep(10);
  }
}


void handle_message(int sock)
{
  struct sockaddr *addr = malloc(sizeof(struct sockaddr));
  if (addr == NULL)
    die_errno();

  socklen_t addr_len = sizeof(struct sockaddr);

  sock_msg_t msg = get_msg(sock, addr, &addr_len);
  pthread_mutex_lock(&client_mutex);

  switch (msg.type)
  {
    case REGISTER:
    {
      sock_msg_type_t reply = OK;

      // check availability - name and slot
      int i = get_client_index_from_name(msg.name);
      if (i < MAX_CLIENTS)
        reply = NAME_TAKEN;

      for (i = 0; i < MAX_CLIENTS && clients[i].fd != 0; i++);

      if (i == MAX_CLIENTS)
        reply = FULL;

      if (reply != OK)
      {
        send_empty(sock, reply);
        break;
      }

      clients[i].fd = sock;
      clients[i].name = strdup(msg.name);
      clients[i].addr = addr;
      clients[i].addr_len = addr_len;
      clients[i].jobs = 0;
      clients[i].inactive = 0;

      printf("Client \'%s\' registered.\n", clients[i].name);

      send_empty(i, OK);
      break;
    }

    case UNREGISTER:
    {
      int i;
      for (i = 0; i < MAX_CLIENTS && (clients[i].fd == 0 || strcmp(clients[i].name, msg.name) != 0); i++);
      if (i == MAX_CLIENTS)
        break;

      unregister_client(i);
      break;
    }

    case WORK_DONE:
    {
      int i = get_client_index_from_name(msg.name);
      if (i < MAX_CLIENTS)
      {
        clients[i].inactive = 0;
        clients[i].jobs--;
      }

      printf("Job number %lu done by client client \'%s\'. Results:\n%s\n", msg.id, msg.name, msg.content);
      break;
    }

    case PONG:
    {
      int i = get_client_index_from_name(msg.name);
      if (i < MAX_CLIENTS)
        clients[i].inactive = 0;

      break;
    }
  }

  pthread_mutex_unlock(&client_mutex);
  delete_msg(msg);
}


void unregister_client(int i)
{
  printf("Client \'%s\' unregistered.\n", clients[i].name);

  clients[i].fd = 0;
  clients[i].name = NULL;
  clients[i].addr = NULL;
  clients[i].addr_len = 0;
  clients[i].jobs = 0;
  clients[i].inactive = 0;
}


int get_client_index_from_name(char *name)
{
  int i;
  for (i = 0; i < MAX_CLIENTS; i++)
  {
    if (clients[i].fd == 0)
      continue;

    if (strcmp(clients[i].name, name) == 0)
      break;
  }

  return i;
}


void send_msg(int i, sock_msg_t msg)
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

  sendto(clients[i].fd, buff, size, 0, clients[i].addr, clients[i].addr_len);

  free(buff);
}


void send_empty(int i, sock_msg_type_t reply)
{
  sock_msg_t msg = { reply, 0, 0, 0, NULL, NULL };
  send_msg(i, msg);
}

sock_msg_t get_msg(int sock, struct sockaddr *addr, socklen_t *addr_len)
{
  sock_msg_t msg;
  ssize_t head_size = sizeof(msg.type) + sizeof(msg.size) + sizeof(msg.name_size) + sizeof(msg.id);
  int8_t buff[head_size];
  if (recv(sock, buff, head_size, MSG_PEEK) < head_size)
    die("Error: unknown message from client!\n");

  memcpy(&msg.type, buff, sizeof(msg.type));
  memcpy(&msg.size, buff + sizeof(msg.type), sizeof(msg.size));
  memcpy(&msg.name_size, buff + sizeof(msg.type) + sizeof(msg.size), sizeof(msg.name_size));
  memcpy(&msg.id, buff + sizeof(msg.type) + sizeof(msg.size) + sizeof(msg.name_size), sizeof(msg.id));

  ssize_t size = head_size + msg.size + 1 + msg.name_size + 1;
  int8_t* buffer = malloc(size);

  if (recvfrom(sock, buffer, size, 0, addr, addr_len) < size)
    die("Error: unknown message from client!\n");

  if (msg.size > 0)
  {
    msg.content = malloc(msg.size + 1);
    if (msg.content == NULL) die_errno();
      memcpy(msg.content, buffer + head_size, msg.size + 1);
  }
  else
    msg.content = NULL;

  if (msg.name_size > 0)
  {
    msg.name = malloc(msg.name_size + 1);
    if (msg.name == NULL) die_errno();
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


void close_server(void)
{
  printf("Server ending work.\n");
  close(local_socket);
  close(web_socket);
  unlink(unix_path);
  close(epoll_fd);
}

