#include <errno.h>
#include <memory.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "utils.h"

pthread_mutex_t rollercoaster_mutex = PTHREAD_MUTEX_INITIALIZER;
rollercoaster_struct *rollercoaster;

static void *cart_thread_fun (void *arg)
{
  int my_ID = *((int *) arg);
  int running = true;

  while (running)
  {
    if (pthread_mutex_lock(&rollercoaster_mutex) != 0)
    {
      perror("Error: could not lock mutex");
      exit(1);
    }

    // at this point this is the only thread with access to the rollercoaster

    cart_data *this = &rollercoaster->carts[my_ID];
    if (rollercoaster->carts[rollercoaster->curr_cart].state == CART_FINISHED)
    {
      rollercoaster->curr_cart += 1;
      if (rollercoaster->curr_cart == rollercoaster->total_carts_number)
        rollercoaster->curr_cart = 0;
      pthread_mutex_unlock(&rollercoaster_mutex);
      continue;
    }

    // waiting for its turn
    if (rollercoaster->curr_cart != my_ID)
    {
      pthread_mutex_unlock(&rollercoaster_mutex);
      continue;
    }

    // arriving at station, opening doors
    if (!this->door_open && (this->state == CART_RIDING || this->state == CART_WAITING))
    {
      this->state = CART_WAITING;
      this->door_open = true;
      print_stamp(true, this->ID);
      printf("Cart arrived at station, opening doors.\n");
      if (this->curr_passengers_number)
        empty_car(this, rollercoaster);
    }

    // all rounds done, finishing work
    if (this->rounds >= this->total_rounds)
    {               
      running = false;
      this->state = CART_FINISHED;
      print_stamp(true, this->ID);
      printf("Cart finished running\n");
      pthread_mutex_unlock(&rollercoaster_mutex);
      continue;
    }

    // button pressed, can close doors and go
    if (this->state == CART_BUTTON_PRESSED)
    {
      this->rounds += 1;
      this->door_open = false;
      this->state = CART_RIDING;
      for (int i = 0; i < rollercoaster->total_passengers_number; i++)
      {
        if (rollercoaster->passengers[i].curr_cart && rollercoaster->passengers[i].curr_cart == this)
          rollercoaster->passengers[i].state = RIDING;
      }

      print_stamp(true, this->ID);
      printf("Button pressed, closing doors.\n");
      rollercoaster->curr_cart++;
      if (rollercoaster->curr_cart == rollercoaster->total_carts_number)
        rollercoaster->curr_cart = 0;

      print_stamp(true, this->ID);
      printf("Cart is going for round %d/%d\n", this->rounds, this->total_rounds);
      pthread_mutex_unlock(&rollercoaster_mutex);

      useconds_t sleep_time = 100 + 100 * (rand() % 10);
      usleep(sleep_time);
      continue;
    }

    // cart is full and ready to go, waiting for button
    if (this->curr_passengers_number == this->max_passengers_number && this->state == CART_WAITING) 
    {
      print_stamp(true, this->ID);
      printf("Cart is full, waiting for button.\n");
      this->state = CART_BUTTON_WAIT;

      int r = rand() % this->max_passengers_number;
      int t = 0;
      for (int i = 0; i < rollercoaster->total_passengers_number; i++)
      {
        if (rollercoaster->passengers[i].curr_cart && rollercoaster->passengers[i].curr_cart == this)
        {	
          t++;
          if (t > r)
          {
            this->passenger_to_push = rollercoaster->passengers[i].ID;
            break;
          }
        }
      }

      pthread_mutex_unlock(&rollercoaster_mutex);
      continue;
    }

    pthread_mutex_unlock(&rollercoaster_mutex);
  }

  pthread_exit(NULL);
}

static void *passenger_thread_fun (void *arg)
{
  int my_ID = *((int *) arg);
  int running = true;

  while (running)
  {
    if (pthread_mutex_lock(&rollercoaster_mutex))
    {
      perror("Error: could not lock mutex");
      exit(1);
    }

    passenger_data *this = &rollercoaster->passengers[my_ID];

    if (this->state == RIDING)
    {
      pthread_mutex_unlock(&rollercoaster_mutex);
      continue;
    }

    // check if passenger is in a cart waiting for departure
    if (this->curr_cart && (this->curr_cart->state == CART_BUTTON_WAIT))
    {
      if (this->curr_cart->passenger_to_push == this->ID)
      {
        this->curr_cart->state = CART_BUTTON_PRESSED;
        print_stamp(false, this->ID);
        printf("Passenger pressed button.\n");
      }

      pthread_mutex_unlock(&rollercoaster_mutex);
      continue;
    }

    if (this->state == IN_CART)
    {
      pthread_mutex_unlock(&rollercoaster_mutex);
      continue;
    }

    // We're waiting in line
    // First we need to find a cart that is open
    cart_data *cart_info = NULL;
    for (int i = 0; i < rollercoaster->total_carts_number; i++)
    {
      if (rollercoaster->carts[i].door_open && rollercoaster->carts[i].state != CART_FINISHED)
      {
        cart_info = &rollercoaster->carts[i];
        break;
      }
    }

    // No cart is currently open, waiting
    if (cart_info == NULL)
    {
      pthread_mutex_unlock(&rollercoaster_mutex);
      continue;
    }

    // We need to make sure the cart is not overpopulated
    if (cart_info->curr_passengers_number == cart_info->max_passengers_number)
    {
      pthread_mutex_unlock(&rollercoaster_mutex);
      continue;
    }

    // Now we can safely board the cart
    cart_info->curr_passengers_number++;
    this->state = IN_CART;
    this->curr_cart = cart_info;
    print_stamp(false, this->ID);
    printf("Passenger is boarding cart %d, current passengers: %d/%d\n", cart_info->ID, cart_info->curr_passengers_number, cart_info->max_passengers_number);
    pthread_mutex_unlock(&rollercoaster_mutex);
  }

  pthread_exit(NULL);
}

int main (int argc, char **argv)
{
  // change stdout to unbuffered, so all threads can easily print
  setvbuf(stdout, NULL, _IONBF, 0);
  srand(time(NULL));

  // parse arguments
  if (argc != 5)
  {
    printf("Error: wrong number of arguments! There should be:\n- passengers number\n- carts number\n- cart capacity\n- number of rides per cart\n");
    exit(1);
  }

  int passengers_number = atoi(argv[1]);
  if (passengers_number <= 0)
  {
    printf("Error: passengers number must be a positive natural number!\n");
    exit(1);
  }

  int total_carts_number = atoi(argv[2]);
  if (total_carts_number <= 0)
  {
    printf("Error: total carts number must be a positive natural number!\n");
    exit(1);
  }

  int cart_capacity = atoi(argv[3]);
  if (cart_capacity <= 0)
  {
    printf("Error: cart capacity must be a positive natural number!\n");
    exit(1);
  }

  int total_rounds = atoi(argv[4]);
  if (total_rounds <= 0)
  {
    printf("Error: number of total rounds per cart must be a positive natural number!\n");
    exit(1);
  }

  rollercoaster = malloc(sizeof(rollercoaster_struct));
  if (rollercoaster == NULL)
  {
    perror("Error: could not properly allocate memory for rollercoaster");
    exit(1);
  }

  // initialize rollercoaster state
  rollercoaster->curr_cart = 0;
  rollercoaster->curr_riding = 0;
  rollercoaster->total_carts_number = total_carts_number;
  rollercoaster->total_passengers_number = passengers_number;
  rollercoaster->passengers_in_line = 0;
  rollercoaster->carts = malloc(total_carts_number * sizeof(cart_data));
  rollercoaster->passengers = malloc(passengers_number * sizeof(passenger_data));

  if (rollercoaster->carts == NULL || rollercoaster->passengers == NULL)
  {
    perror("Error: could not properly allocate memory for carts or passengers");
    exit(1);
  }

  // initialize passengers' and carts' states
  for (int i = 0; i < total_carts_number; i++)
  {
    rollercoaster->carts[i].ID = i;
    rollercoaster->carts[i].state = CART_WAITING;
    rollercoaster->carts[i].rounds = 0;
    rollercoaster->carts[i].total_rounds = total_rounds;
    rollercoaster->carts[i].curr_passengers_number = 0;
    rollercoaster->carts[i].max_passengers_number = cart_capacity;
    rollercoaster->carts[i].passenger_to_push = 0;
    rollercoaster->carts[i].door_open = false;
  }

  for (int i = 0; i < passengers_number; i++)
  {
    rollercoaster->passengers[i].ID = i;
    rollercoaster->passengers[i].state = WAITING;
    rollercoaster->passengers[i].curr_cart = NULL;
  }

  pthread_t *thread_ID_array = malloc((total_carts_number + passengers_number) * sizeof(pthread_t));
  for (int i = 0; i < total_carts_number; i++)
  {
    int *ID = malloc(sizeof(int));
    *ID = i;
    pthread_create(thread_ID_array + i, NULL, &cart_thread_fun, (void *) ID);
  }

  for (int i = 0; i < passengers_number; i++)
  {
    int *ID = malloc(sizeof(int));
    *ID = i;
    pthread_create(thread_ID_array + total_carts_number + i, NULL, &passenger_thread_fun, (void *) ID);
  }

  for (int i = 0; i < total_carts_number; i++)
    pthread_join(thread_ID_array[i], NULL);

  for (int i = total_carts_number; i < (total_carts_number + passengers_number); i++)
    pthread_cancel(thread_ID_array[i]);

  return 0;
}
