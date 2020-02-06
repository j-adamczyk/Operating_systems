#ifndef util_h
#define util_h

#define FTOK_PATH "/tmp"
#define FTOK_ID 42
#define MAX_CLIENTS 16
#define QMSG_LEN 128

// COMMAND_TYPE = PRIORITY
typedef enum command
{
  INIT = 1,
  NEW_CLIENT = 2,
  STOP = 3,
  LIST = 4,
  FRIENDS = 5,
  ECHO = 6,
  TOALL = 7,
  TOFRIENDS = 8,
  TOONE = 9,
  ADD = 10,
  DEL = 11
} command;


void send_command(int queue_ID, command cmd, char *msg);
ssize_t receive_command(int queue_ID, char *message_buffer);
ssize_t receive_command_nonblocking(int queue_ID, char *message_buffer);

#endif
