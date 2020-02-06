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

typedef struct msgbuf
{
  long mtype;
  char mtext[QMSG_LEN];
} msgbuf;

key_t get_server_key();
void send_command(int queue_ID, command cmd, char *msg);
ssize_t receive_command(int queue_ID, msgbuf *message_buffer, long msgtype);
ssize_t receive_command_noblock(int queue_ID, msgbuf *message_buffer, long msgtype);

#endif
