#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <time.h>
#include <unistd.h>

#include "util.h"

char client_queue_path[32];
int running = true;
int client_ID = 0; // 0 means unregistered client
mqd_t server_queue_ID;
mqd_t client_queue_ID;

// Protocol:
// Request: CLIENT_ID COMMAND_BODY   <QMSG_LEN bytes in total>
// Response: COMMAND_BODY      <QMSG_LEN bytes in total>


void send_command(int queue_ID, command cmd, char *msg)
{
  // msg must be exactly QMSG_LEN bytes long, padded with '\0'
  char message_text[QMSG_LEN];
  snprintf(message_text, QMSG_LEN, "%d %s", cmd, msg);
  if (mq_send(queue_ID, message_text, QMSG_LEN, 1) == -1)
    printf("Warning: could not send message.\n");
}

ssize_t receive_command(int queue_ID, char *message_buffer)
{
  ssize_t received = mq_receive(queue_ID, message_buffer, QMSG_LEN, NULL);
  if (received == -1)
  {
    if (errno == EINTR) // During waiting for a message SIGINT occurred
       return -1;
    printf("Warning: message not received properly.\n");
  }
  return received;
}

ssize_t receive_command_nonblocking(int queue_ID, char *message_buffer)
{
  struct timespec time;
  clock_gettime(CLOCK_REALTIME, &time);
  time.tv_sec++;
  return mq_timedreceive(queue_ID, message_buffer, QMSG_LEN, NULL, &time);
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
    char response[QMSG_LEN];
    while(receive_command_nonblocking(client_queue_ID, response) != -1)
    {
      command type = strtol(strtok(response, " "), NULL, 10);
      if (type == STOP)
        running = false;
      else
        printf("%s\n", strtok(NULL, "\0"));
    }
  }
}


void await_client_ID(int client_queue_ID)
{
  char message_buffer[QMSG_LEN];
  memset(message_buffer, '\0', QMSG_LEN);
  receive_command(client_queue_ID, message_buffer);
  if (*message_buffer == '\0') // received SIGINT while waiting for response
    return;
  command type = strtol(strtok(message_buffer, " "), NULL, 10);
  if (type != NEW_CLIENT)
  {
    printf("Error: did not receive NEW_CLIENT message!\n");
    exit(1);
  }

  printf("Client ready for work.\n");
  client_ID = (int) strtol(strtok(NULL, "\n"), NULL, 10);
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

  char *server_queue_path = "/server_queue";
  server_queue_ID = mq_open(server_queue_path, O_WRONLY);
  if (server_queue_ID == -1)
  {
    fprintf(stderr, "Failed to open server queue, make sure server is running.\n");
    exit(1);
  }

  struct mq_attr queue_attributes;
  queue_attributes.mq_maxmsg = 4;
  queue_attributes.mq_msgsize = QMSG_LEN;
  memset(client_queue_path, '\0', 32);
  snprintf(client_queue_path, 32, "/%d", getpid());
  client_queue_ID = mq_open(client_queue_path, O_CREAT | O_RDONLY, 0666, &queue_attributes);
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
  snprintf(init_msg, QMSG_LEN-1, "%d %d %s", 0, getpid(), client_queue_path);
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
    snprintf(exit_msg, QMSG_LEN, "%d", client_ID);
    send_command(server_queue_ID, STOP, exit_msg);
    free(exit_msg);
    sleep(1);
  }

  // Destroy client's own queue
  mq_close(client_queue_ID);
  mq_unlink(client_queue_path);

  return 0;
}
