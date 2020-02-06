#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define min(X, Y) (((X) < (Y)) ? (X) : (Y))
#define max(X, Y) (((X) > (Y)) ? (X) : (Y))

typedef struct image_struct
{
  int height;
  int width;
  int **array;
} image_struct;

typedef struct filter_struct
{
  int size;
  double **array;
} filter_struct;


int thread_number;
char *mode;
image_struct image;
filter_struct filter;
image_struct result;


void load_image (char *image_file_name)
{
  FILE *file = fopen(image_file_name, "r");
  if (errno != 0)
  {
    perror("Error");
    exit(1);
  }

  char line[2048];
  char *elem;

  // first and second lines don't matter
  fgets(line, sizeof(line), file);
  fgets(line, sizeof(line), file);

  // third line contains width and height of image
  fgets(line, sizeof(line), file);

  elem = strtok(line, " ");
  image.width = atoi(elem);

  elem = strtok(NULL, " ");
  image.height = atoi(elem);

  // fourth line contains max pixel value
  fgets(line, sizeof(line), file);
  elem = strtok(line, " ");
  int max_pixel_val = atoi(elem);

  image.array = malloc(image.height * sizeof(int*));
  for (int i = 0; i < image.height; i++)
    image.array[i] = malloc(image.width * sizeof(int));

  // next lines are actual image
  int i = 0;
  int j = 0;
  bool done = false;
  while (fgets(line, sizeof(line), file) && !done)
  {
    elem = strtok(line, " ");
    while (elem != NULL && !done)
    {
      image.array[i][j] = min(atoi(elem), max_pixel_val);
      elem = strtok(NULL, " ");
      j += 1;
      if (j == image.width)
      {
        i += 1;
        j = 0;

        if (i == image.height)
          done = true;
      }
    }
  }

  if (i < image.height)
  {
    printf("Error: incorrect image file! It didn't contain as many pixels as declared in its height and width!\n");
    exit(1);
  }
  fclose(file);
}


void load_filter (char *filter_file_name)
{
  FILE *file = fopen(filter_file_name, "r");
  if (errno != 0)
  {
    perror("Error");
    exit(1);
  }

  char line[1024];
  char *elem;

  // first line contains filter size (it's square)
  fgets(line, sizeof(line), file);
  filter.size = atoi(line);

  filter.array = malloc(filter.size * sizeof(int*));
  for (int i = 0; i < filter.size; i++)
    filter.array[i] = malloc(filter.size * sizeof(int));

  // next lines are actual filter
  int i = 0;
  int j = 0;
  bool done = false;
  while (fgets(line, sizeof(line), file) && !done)
  {
    elem = strtok(line, " ");
    while (elem != NULL && !done)
    {
      if (j == filter.size)
      {
        i += 1;
        j = 0;

        if (i == filter.size)
        {
          done = true;
          break;
        }
      }

      filter.array[i][j] = atof(elem);
      elem = strtok(NULL, " ");
      j += 1;
    }
  }

  fclose(file);
}


int calculate_pixel (int x, int y)
{
  double pixel = 0;
  double half_filter_size = ceil(filter.size / 2);
  for (int i = 0; i < filter.size; i++)
  {
    int new_x = round(max(0, x - half_filter_size + i - 1));
    new_x = new_x < image.height ? new_x : image.height - 1;

    for (int j = 0; j < filter.size; j++)
    {
      int new_y = round(max(0, y - half_filter_size + j - 1));
      new_y = new_y < image.width ? new_y : image.width - 1;

      pixel += image.array[new_x][new_y] * filter.array[i][j];
    }
  }

  pixel = pixel >= 0 ? pixel : 0;

  return round(pixel);
}


void *filter_image (void *arg)
{
  struct timeval start_time;
  struct timeval end_time;

  gettimeofday(&start_time, NULL);

  int *my_number = arg;
  if (strcmp(mode, "block") == 0)
  {
    int col_from = (*my_number - 1) * ceil(image.width / thread_number);
    int col_to = *my_number * ceil(image.width / thread_number);

    for (int x = 0; x < image.height; x++)
      for (int y = col_from; y < col_to; y++)
        result.array[x][y] = calculate_pixel(x, y);
  }
  else if (strcmp(mode, "interleaved") == 0)
  {
    int x = 0;
    for (int i = 0; x < image.height; i++)
    {
      x = *my_number - 1 + i * thread_number;

      if (x >= image.height)
        break;

      for (int y = 0; y < image.width; y++)
        result.array[x][y] = calculate_pixel(x, y);
    }
  }

  gettimeofday(&end_time, NULL);

  struct timeval *result = malloc(sizeof(struct timeval));
  result->tv_sec = end_time.tv_sec - start_time.tv_sec;
  result->tv_usec = end_time.tv_usec - start_time.tv_usec;
  return result;
}


void save_result (char *result_time_name)
{
  FILE *file = fopen(result_time_name, "w+");
  if (errno != 0)
  {
    perror("Error");
    exit(1);
  }

  // first and second lines contain formal information
  char *image_type = "P2\n";
  fwrite(image_type, sizeof(char), strlen(image_type), file);

  char *comment = "#image after filtering\n";
  fwrite(comment, sizeof(char), strlen(comment), file);

  // third line contains width and height of image
  char width[10];
  sprintf(width, "%d ", result.width);
  fwrite(width, sizeof(char), strlen(width), file);

  char height[10];
  sprintf(height, "%d\n", result.height);
  fwrite(height, sizeof(char), strlen(height), file);

  // fourth line contains max pixel value
  char max_pixel_val[10];
  sprintf(max_pixel_val, "%d\n", 255);
  fwrite(max_pixel_val, sizeof(char), strlen(max_pixel_val), file);

  // next lines are actual image

  // seach number is max 3 digits long + we need spaces between them and signs string end
  char *line = malloc(4 * result.width + 3);
  char number[8];
  for (int i = 0; i < result.height; i++)
  {
    memset(line, 0, strlen(line));
    for (int j = 0; j < result.width; j++)
    {
      memset(number, 0, strlen(number));
      sprintf(number, "%d ", result.array[i][j]);
      line = strcat(line, number);
    }
    line = strcat(line, "\n");
    fwrite(line, sizeof(char), strlen(line), file);
  }

  fclose(file);
}


void save_time (int thread_number, char *mode, long long int time)
{
  FILE *output_file = fopen("times.txt", "a");
  if (output_file == NULL)
    printf("Warning: could not open output file, result was not saved.\n");

  char to_write[128];
  sprintf(to_write, "%d thread(s), %s mode, %lld us (real time)\n", thread_number, mode, time);

  fwrite(to_write, sizeof(char), strlen(to_write), output_file);

  fclose(output_file);
}


int main (int argc, char **argv)
{
  // process arguments
  if (argc != 6)
  {
    printf("Error: wrong number of arguments! There should be:\n- number of threads\n- thread calculation mode: \"block\" or \"interleaved\"\n- image file name\n- filter file name\n- result file name\n");
    exit(1);
  }

  thread_number = atoi(argv[1]);
  mode = argv[2];
  if (strcmp(mode, "block") != 0 && strcmp(mode, "interleaved") != 0)
  {
    printf("Error: thread calculation mode must be either \"block\" or \"interleaved\"!\n");
    exit(1);
  }

  char *image_file_name = argv[3];
  char *filter_file_name = argv[4];
  char *result_file_name = argv[5];

  // prepare all arrays
  load_image(image_file_name);
  load_filter(filter_file_name);

  result.height = image.height;
  result.width = image.width;
  result.array = malloc(image.height * sizeof(int*));
  for (int i = 0; i < image.height; i++)
    result.array[i] = calloc(image.width, sizeof(int));

  // measure time
  long long int seconds;
  long long int microseconds;
  struct timeval main_start_time;
  struct timeval main_end_time;

  // create threads and do actual work
  gettimeofday(&main_start_time, NULL);

  pthread_t thread_array[thread_number];
  for (int i = 0; i < thread_number; i++)
  {
    pthread_t new_thread;
    int *new_thread_number = malloc(sizeof(int));
    *new_thread_number = i+1;
    if (pthread_create(&new_thread, NULL, &filter_image, new_thread_number) != 0)
    {
      perror("Error:");
      exit(1);
    }

    thread_array[i] = new_thread;
  }

  // sum time
  struct timeval thread_time;
  void *result;
  for (int i = 0; i < thread_number; i++)
  {
    if (pthread_join(thread_array[i], &result) != 0)
    {
      perror("Error:");
      exit(1);
    }

    thread_time = *(struct timeval*) result;
    long long int time = thread_time.tv_sec * 1e6 + thread_time.tv_usec;
    printf("Thread number %d worked %lld us (real time).\n", i, time);

    seconds += thread_time.tv_sec;
    microseconds += thread_time.tv_usec;
  }

  gettimeofday(&main_end_time, NULL);
  seconds += (main_end_time.tv_sec - main_start_time.tv_sec);
  microseconds += (main_end_time.tv_usec - main_start_time.tv_usec);

  long long int time = seconds * 1e6 + microseconds;
  printf("Whole filtering took %lld us (real time).\n", time);

  save_time(thread_number, mode, time);

  save_result(result_file_name);

  return 0;
}
