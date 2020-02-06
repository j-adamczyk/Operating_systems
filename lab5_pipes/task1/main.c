#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wait.h>


typedef struct command
{
  char *command_name;
  char **arguments;
} command;


typedef struct command_array
{
  command **commands;
  int number_of_commands;
} command_array;


command_array *parse_line (char *line)
{
  command_array *result = malloc (sizeof (struct command_array));

  int command_number = 1; // = pipe number + 1, min 1
  for (int i=0; i<strlen(line); i++)
    if (line[i] == '|')
      command_number += 1;

  result->number_of_commands = command_number;

  command **commands = malloc (command_number * sizeof (command*));

  // holds parts between |
  char **commands_and_args = malloc (command_number * sizeof (char*));

  char *command_and_args = strtok (line, "|\n");
  int i = 0;
  while (command_and_args != NULL)
  {
    commands_and_args[i] = strdup (command_and_args);
    i += 1;
    command_and_args = strtok (NULL, "|\n");
  }

  for (command_number = 0; command_number < result->number_of_commands; command_number++)
  {
    char *command_and_args = commands_and_args[command_number];

    // count number of arguments of given command
    char *command_and_args_copy = strdup (command_and_args);
    int args_number = -1; // because it will also count command, making it 0
    char *word;
    word = strtok (command_and_args_copy, " ");
    while (word != NULL)
    {
      args_number += 1;
      word = strtok (NULL, " ");
    }

    // create and fill command struct
    command *command = malloc (sizeof (struct command));

    // fill command name
    word = strtok (command_and_args, " ");
    command->command_name = malloc ((strlen (word) + 1) * sizeof (char));
    strcpy (command->command_name, word);

    // fill args
    if (args_number == 0)
    {
      command->arguments = malloc (2 * sizeof (char*));
    }
    else
    {
      command->arguments = malloc ((args_number + 2) * sizeof (char*)); // +1 for command name as first argument, +1 for ending NULL pointer

      word = strtok (NULL, " ");
      int arg_counter = 1;
      while (word != NULL)
      {
        command->arguments[arg_counter] = malloc ((strlen (word) + 1) * sizeof (char));
        strcpy (command->arguments[arg_counter], word);
        arg_counter += 1;
        word = strtok (NULL, " ");
      }
    }

    command->arguments[0] = command->command_name;
    command->arguments[args_number + 1] = NULL;
    commands[command_number] = command;
  }

  free (commands_and_args);
  result->commands = commands;

  return result;
}


void wait_for_processes()
{
  pid_t wpid;
  int status;
  while ((wpid = wait (&status)) > 0)
  {
    if (WIFEXITED (status))
      if (WEXITSTATUS (status) != 0)
        printf ("Warning: process with pid %d did not exit with success.\n", wpid);
  }
}


int main (int argc, char **argv)
{
  if (argc != 2)
  {
    perror ("Error: wrong number of arguments! There should be file_name!");
    exit (1);
  }

  if (strcmp (argv[1], "") == 0)
  {
    perror ("Error: file name cannot be empty!");
    exit (1);
  }

  FILE *file = fopen (argv[1], "r");
  if (file == NULL)
  {
    perror ("Error: could not open provided file!");
    exit (1);
  }

  size_t size = 512;
  char *line = malloc (size * sizeof (char));
  while (getline (&line, &size, file) != -1)
  {
    if (strcmp (line, "\n") == 0)
      continue;

    char *line_copy = malloc ((strlen (line) + 1) * sizeof (char));
    strcpy (line_copy, line);

    command_array *parsed_line = parse_line (line_copy);

    int prev_cmd_out = 0;
    int pipefd[2] = {0,0};
    for (int i=0; i < parsed_line->number_of_commands; i++)
    {
      if (i < parsed_line->number_of_commands - 1)
        pipe (pipefd);

      pid_t child_PID = fork();
      if (child_PID == 0)
      {
        if (prev_cmd_out != 0)
          dup2 (prev_cmd_out, STDIN_FILENO);

        if (i < parsed_line->number_of_commands - 1)
        {
          dup2 (pipefd[1], STDOUT_FILENO);
          close (pipefd[0]);
        }

        char *command_name = parsed_line->commands[i]->command_name;
        char **arguments = parsed_line->commands[i]->arguments;

        execvp (command_name, arguments);

        perror ("Error: got after exec, couldn't start child process properly!");
        exit (1);
      }
      else
      {
        if (prev_cmd_out != 0)
          close (prev_cmd_out);
      }

      if (i < parsed_line->number_of_commands - 1)
      {
        close (pipefd[1]);
        prev_cmd_out = pipefd[0];
      }
    }

    wait_for_processes();
  }

  free (line);
  return 0;
}
