#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "util.h"

typedef struct client
{
  int client_ID;
  int client_queue_ID;
  pid_t client_PID;
  int friend_count;
  struct client **friends;
} client;

int running = true;

// Protocol:
// Request: CLIENT_ID COMMAND_BODY   <QMSG_LEN bytes in total>
// Response: COMMAND_BODY      <QMSG_LEN bytes in total>


key_t get_server_key()
{
    return ftok(FTOK_PATH, FTOK_ID);
}

void send_command(int queue_ID, command cmd, char *msg)
{
  // msg must be exactly QMSG_LEN bytes long, padded with '\0'
  msgbuf message_buffer;
  message_buffer.mtype = cmd;
  memset(message_buffer.mtext, '\0', QMSG_LEN);
  snprintf(message_buffer.mtext, QMSG_LEN, "%s", msg);
  if (msgsnd(queue_ID, &message_buffer, QMSG_LEN, 0) == -1)
    printf("Warning: command \"%s\" could not be send.\n", msg);
}

ssize_t receive_command(int queue_ID, msgbuf *message_buffer, long msgtype)
{
  ssize_t received = msgrcv(queue_ID, message_buffer, QMSG_LEN, msgtype, 0);
  if (received == -1)
  {
    if (errno == EINTR) // During waiting for a message SIGINT occurred
       return -1;
    printf("Warning: message not received properly.\n");
  }
  return received;
}


static void sigint_handler(int signum)
{
  if (signum == SIGINT)
  {
    printf ("SIGINT received by server, ending work.\n");
    running = false;
  }
}

void add_client(int client_queue_ID, pid_t client_PID, int new_ID, client *clients)
{
  if (new_ID > MAX_CLIENTS)
  {
    printf("Warning: could not add another clients, limit achieved. New client terminated. To add more clients, remove some of the existing ones.\n");
    kill(client_PID, SIGINT);
    return;
  }
  clients[new_ID].client_ID = new_ID;
  clients[new_ID].client_queue_ID = client_queue_ID;
  clients[new_ID].client_PID = client_PID;
  clients[new_ID].friend_count = 0;
  clients[new_ID].friends = calloc(0, MAX_CLIENTS * sizeof(client*));
}

void add_friend(client *cl, client *friend)
{
  if (cl->friend_count >= MAX_CLIENTS)
  {
    printf("Warning: could not another friend, all current clients are already friends.\n");
    return;
  }
  cl->friends[cl->friend_count] = friend;
  cl->friend_count++;
}

void delete_friend(client *cl, client *friend)
{
  for (int i = 0; i < cl->friend_count; i++)
    if (cl->friends[i] && cl->friends[i] == friend)
      cl->friends[i] = NULL;
}

void remove_client(client *clients, int client_ID, int clients_number)
{
  for (int i = 0; i < clients_number; i++)
    if (clients[i].client_ID == client_ID) // client_ID == 0 means slot is free
    {
      clients[i].client_ID = 0;
      free(clients[i].friends);
    }
}

client *get_client_from_client_ID(client *clients, int client_ID_to_find, int last)
{
  for (int i = 0; i < last; i++)
    if (clients[i].client_ID == client_ID_to_find)
      return (clients + i);
  return NULL;
}

int main()
{
  // turn off stdout buffering so messages are visible immediately
  if (setvbuf(stdout, NULL, _IONBF, 0) != 0)
  {
    printf("Error: buffering mode could not be changed!\n");
    exit(1);
  }

  key_t server_key = get_server_key();
  int server_queue_ID = msgget(server_key, IPC_CREAT | 0666);
  if (server_queue_ID == -1)
  {
    printf("Error: could not greate server queue!\n");
    exit(1);
  }
  printf("Created server queue with ID %d.\n", server_queue_ID);

  client clients[MAX_CLIENTS + 1]; // +1, since proper client_ID can't be 0 and numeration begins from 1
  memset(clients, 0, (MAX_CLIENTS+1) * sizeof(client));
  int curr_slot = 1;

  struct sigaction act;
  act.sa_handler = sigint_handler;
  sigemptyset(&act.sa_mask);
  sigaddset(&act.sa_mask, SIGINT);
  act.sa_flags = 0;
  sigaction(SIGINT, &act, NULL);

  msgbuf request;
  char response[QMSG_LEN];
  while (running)
  {
    memset(request.mtext, '\0', QMSG_LEN);
    memset(response, '\0', QMSG_LEN);

    // perform series on non-blocking calls to receive top priority commands
    // if no message of desired type is waiting, then errno is set to ENOMSG
    ssize_t received = msgrcv(server_queue_ID, &request, QMSG_LEN, STOP, MSG_NOERROR | IPC_NOWAIT);
    if (received == -1)
    {
      if (errno == ENOMSG)
      {
        received = msgrcv(server_queue_ID, &request, QMSG_LEN, LIST, MSG_NOERROR | IPC_NOWAIT);
        if (received == -1)
        {
          if (errno == ENOMSG)
          {
            received = msgrcv(server_queue_ID, &request, QMSG_LEN, FRIENDS, MSG_NOERROR | IPC_NOWAIT);
            if (received == -1)
            {
              if (errno == ENOMSG)
              {
                // no non-blocking calls, check blocking call
                receive_command(server_queue_ID, &request, 0);
                if (!running)
                  break;
              }
              else
              {
                printf("Warning: some kind of problem encountered while receiving message.\n");
              }
            }
          }
          else
          {
            printf("Warning: some kind of problem encountered while receiving message.\n");
          }
        }
      }
      else
      {
        printf("Warning: some kind of problem encountered while receiving message.\n");
      }
    }

    int client_ID = (int) strtol(strtok(request.mtext, " "), NULL, 10);
    char *command_body = strtok(NULL, "\n");
    client *cl;

    cl = get_client_from_client_ID(clients, client_ID, curr_slot);
    if (cl == NULL && request.mtype != INIT)
    {
      printf("Warning: wrong client ID %d received.\n", client_ID);
      continue;
    }

    if (request.mtype == STOP)
    {
      if (client_ID < curr_slot)
      {
        remove_client(clients, client_ID, curr_slot);
        printf("Client with ID %d stopped and removed.\n", client_ID);
      }
      else
        printf("Warning: client with ID %d could not be stopped and removed, this ID is greater then highest current client ID.\n", client_ID);
    }
    else if (request.mtype == INIT)
    {
      pid_t client_PID = (pid_t) strtol(strtok(command_body, " "), NULL, 10);
      int client_queue_ID = (int) strtol(strtok(NULL, "\0"), NULL, 10);
      if (client_queue_ID == -1)
      {
        printf("Warning: could not open client queue, key was invalid.\n");
        continue;
      }
      add_client(client_queue_ID, client_PID, curr_slot, clients);
      snprintf(response, QMSG_LEN, "%d", curr_slot);
      curr_slot++;
      send_command(client_queue_ID, NEW_CLIENT, response);
      printf("New client with ID %d and queue ID %d recognized.\n", curr_slot - 1, client_queue_ID);
    }
    else if (request.mtype == ECHO)
    {
      snprintf(response, QMSG_LEN, "%s", command_body);
      send_command(cl->client_queue_ID, ECHO, response);
      kill(cl->client_PID, SIGUSR1);
      printf("Echoing message: \"%s\" to client with ID %d.\n", response, cl->client_ID);
    }
    else if (request.mtype == LIST)
    {
      int written = 0;
      for (int i = 1; i < curr_slot; i++)
      {
        if (clients[i].client_ID != 0)
        {
          if (written > QMSG_LEN)
            break;
          written = snprintf(response + written,
                             (size_t) QMSG_LEN - written,
                             "Client_ID: %d, client_queue_ID: %d\n", clients[i].client_ID, clients[i].client_queue_ID);
        }
      }
      send_command(cl->client_queue_ID, LIST, response);
      kill(cl->client_PID, SIGUSR1);
      printf("Listing clients for client with ID %d.\n", client_ID);
    }
    else if (request.mtype == FRIENDS || request.mtype == ADD)
    {
      if (request.mtype == FRIENDS)
      {
        memset(cl->friends, 0, MAX_CLIENTS * sizeof(client *));
        cl->friend_count = 0;
      }
      char *str_ID = strtok(command_body, " ");
      while (str_ID != NULL)
      {
        int friend_ID = (int) strtol(str_ID, NULL, 10);
        client *friend = get_client_from_client_ID(clients, friend_ID, curr_slot);
        if (friend == NULL)
        {
          printf("Warning: could not add non-existent friend.\n");
          continue;
        }
        add_friend(cl, friend);
        str_ID = strtok(NULL, " ");
      }
      printf("Adding friends for client with ID %d.\n", client_ID);
    }
    else if (request.mtype == TOALL)
    {
      snprintf(response, QMSG_LEN, "[%d] %s", cl->client_ID, command_body);
      for (int i = 1; i < curr_slot; i++)
        if (clients[i].client_ID != 0 && clients[i].client_ID != cl->client_ID)
        {
          send_command(clients[i].client_queue_ID, TOALL, response);
          kill(clients[i].client_PID, SIGUSR1);
        }
      printf("Forwarding message to all clients from client with ID %d.\n", client_ID);
    }
    else if (request.mtype == TOFRIENDS)
    {
      snprintf(response, QMSG_LEN, "[%d] %s", cl->client_ID, command_body);
      for (int i = 0; i < cl->friend_count; i++)
        if (cl->friends[i] && cl->friends[i]->client_ID != 0)
        {
          send_command(cl->friends[i]->client_queue_ID, TOFRIENDS, response);
          kill(cl->friends[i]->client_PID, SIGUSR1);
        }
      printf("Forwarding message to friends from client with ID %d.\n", client_ID);
    }
    else if (request.mtype == TOONE)
    {
      int target_ID = (int) strtol(strtok(NULL, " "), NULL, 10);
      client *target = get_client_from_client_ID(clients, target_ID, curr_slot);
      if (target == NULL)
      {
        printf("Warning: could not send message to non-existent client.\n");
        continue;
      }
      snprintf(response, QMSG_LEN, "[%d] %s", cl->client_ID, command_body);
      send_command(target->client_queue_ID, TOONE, response);
      kill(target->client_PID, SIGUSR1);
      printf("Forwarding message to client with ID %d from client with ID %d.\n", target_ID, client_ID);
    }
    else if (request.mtype == DEL)
    {
      char *str_ID = strtok(command_body, " ");
      while (str_ID != NULL)
      {
        int friend_ID = (int) strtol(str_ID, NULL, 10);
        client *friend = get_client_from_client_ID(clients, friend_ID, curr_slot);
        if (friend == NULL)
        {
          printf("Warning: could not delete non-existent friend.\n");
          continue;
        }
        delete_friend(cl, friend);
        str_ID = strtok(NULL, " ");
      }
      printf("Deleting friends for client with ID %d.\n", client_ID);
    }
    else
      printf("Warning: command not recognized and therefore not executed.\n");
  }

  // Inform clients about exit and clean up server queue
  printf("Informing clients about server ending work.\n");
  for (int i = 0; i < curr_slot; i++)
    if (clients[i].client_ID != 0)
    {
      kill(clients[i].client_PID, SIGINT);
      receive_command(server_queue_ID, &request, STOP);
    }

  msgctl(server_queue_ID, IPC_RMID, NULL);

  return 0;
}
