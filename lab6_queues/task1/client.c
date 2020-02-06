#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>

#include "util.h"

int running = true;
int client_ID = 0;  // 0 means unregistered client
int client_queue_ID;

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


static void sig_handler (int signum)
{
  if (signum == SIGINT)
  {
    printf ("SIGINT received by client with ID %d, ending work.\n", client_ID);
    running = false;
  }
  else if (signum == SIGUSR1)
  {
    msgbuf message_buffer;
    while (msgrcv(client_queue_ID, &message_buffer, QMSG_LEN, 0, MSG_NOERROR | IPC_NOWAIT) != -1)
    {
      if (message_buffer.mtype == STOP)
        running = false;
      else
        printf("%s\n", message_buffer.mtext);
    }
  }
}


void await_client_ID(int client_queue_ID)
{
    msgbuf message_buffer;
    receive_command(client_queue_ID, &message_buffer, NEW_CLIENT);
    if (running == false) // received SIGINT while waiting for NEW_CLIENT message
        return;
    printf("Client ready for work.\n");
    client_ID = (int) strtol(message_buffer.mtext, NULL, 10);
}


// evaluates one line and sends appropriate message to the server
void evaluate (int server_queue_ID, char *msg, char *input)
{
  if (strcmp(input, "STOP\n") == 0)
    running = false;
  else if (strcmp(input, "LIST\n") == 0)
  {
    snprintf(msg, QMSG_LEN, "%d", client_ID);
    send_command(server_queue_ID, LIST, msg);
  }
  else if (strncmp(input, "ECHO ", 5) == 0)
  {
    snprintf(msg, QMSG_LEN, "%d %s", client_ID, input+5);
    send_command(server_queue_ID, ECHO, msg);
  }
  else if (strncmp(input, "FRIENDS", 7) == 0)
  {
    snprintf(msg, QMSG_LEN, "%d %s", client_ID, input+8);
    send_command(server_queue_ID, FRIENDS, msg);
  }
  else if (strncmp(input, "2ALL ", 5) == 0)
  {
    snprintf(msg, QMSG_LEN, "%d %s", client_ID, input+5);
    send_command(server_queue_ID, TOALL, msg);
  }
  else if (strncmp(input, "2FRIENDS ", 9) == 0)
  {
    snprintf(msg, QMSG_LEN, "%d %s", client_ID, input+9);
    send_command(server_queue_ID, TOFRIENDS, msg);
  }
  else if (strncmp(input, "2ONE ", 5) == 0)
  {
    snprintf(msg, QMSG_LEN, "%d %s", client_ID, input+5);
    send_command(server_queue_ID, TOONE, msg);
  }
  else if (strncmp(input, "ADD ", 4) == 0)
  {
    snprintf(msg, QMSG_LEN, "%d %s", client_ID, input+4);
    send_command(server_queue_ID, ADD, msg);
  }
  else if (strncmp(input, "DEL ", 4) == 0)
  {
    snprintf(msg, QMSG_LEN, "%d %s", client_ID, input+4);
    send_command(server_queue_ID, DEL, msg);
  }
  else if (strncmp(input, "READ ", 5) == 0)
  {
    char *rel_path = strtok(input + 5, "\n");
    char *abs_path = realpath(rel_path, NULL);
    if (abs_path == NULL)
    {
      printf("Warning: could not find command file!\n");
      return;
    }

    FILE *command_file = fopen(abs_path, "r");
    if (command_file == NULL)
    {
      printf("Warning: could not open command file\n");
      return;
    }

    ssize_t read = 0;
    size_t len = QMSG_LEN + 16; // length of input

    while (true) // will exit when EOF is read
    {
      memset(msg, '\0', QMSG_LEN);
      memset(input, '\0', len);
      read = getline (&input, &len, command_file);
      if (read == -1)
        break;
      evaluate(server_queue_ID, msg, input);
    }

    free(abs_path);
  }
  else if (strcmp(input, "\n") == 0)
    return;
  else
    printf("Warning: unknown command, nothing executed!\n");
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

  int server_queue_ID = msgget(server_key, 0);
  if (server_queue_ID == -1)
  {
    fprintf(stderr, "Failed to open server queue, make sure server is running.\n");
    exit(1);
  }

  client_queue_ID = msgget(IPC_PRIVATE, 0666);
  if (client_queue_ID == -1)
  {
    printf("Error: could not create client queue!\n");
    exit(1);
  }

  printf("Created client queue with ID %d.\n", client_queue_ID);

  struct sigaction act;
  act.sa_handler = sig_handler;
  sigemptyset(&act.sa_mask);
  sigaddset(&act.sa_mask, SIGINT);
  sigaddset(&act.sa_mask, SIGUSR1);
  act.sa_flags = 0;
  sigaction(SIGINT, &act, NULL);
  sigaction(SIGUSR1, &act, NULL);

  // inform server about new client; protocol required some client_ID, so 0 is sent
  char *init_msg = calloc('\0', QMSG_LEN);
  snprintf(init_msg, QMSG_LEN, "%d %d %d", 0, getpid(), client_queue_ID);
  send_command(server_queue_ID, INIT, init_msg);
  free(init_msg);

  await_client_ID(client_queue_ID);

  char msg[QMSG_LEN]; // message text
  char input[QMSG_LEN + 16]; // command + message text; read from terminal
  while (running)
  {
    memset(msg, '\0', QMSG_LEN);
    memset(input, '\0', QMSG_LEN + 16);

    fgets(input, QMSG_LEN+16, stdin);

    if (!running)
      break;

    if (*input != '\0')
      evaluate(server_queue_ID, msg, input);

  }

  // Inform server about client exit
  if (client_ID)
  {
    char *exit_msg = calloc('\0', QMSG_LEN);
    snprintf(exit_msg, QMSG_LEN-1, "%d", client_ID);
    send_command(server_queue_ID, STOP, exit_msg);
    free(exit_msg);
    sleep(1);
  }

  // Destroy client's own queue
  msgctl(client_queue_ID, IPC_RMID, NULL);

  return 0;
}
