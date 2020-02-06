#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "utils.h"


// true - cart, false - passenger
void print_stamp (bool type, int id)
{
  struct timeval tm;
  gettimeofday(&tm, NULL);
  if (type)
    printf("%2d cart:      %ld us: ", id, tm.tv_usec);
  else
    printf("%2d passenger: %ld us: ", id, tm.tv_usec);
}


void step_out (passenger_data *passenger, cart_data *cart)
{
  if (passenger->state == WAITING)
  {
    printf("Error: waiting passenger can't get out!\n");
    exit(1);
  }

  passenger->state = WAITING;
  passenger->curr_cart = NULL;
  print_stamp(false, passenger->ID);
  printf("Getting out of car %d. Passengers left: %d/%d.\n", cart->ID, cart->curr_passengers_number, cart->max_passengers_number);
}


void empty_car (cart_data *cart, rollercoaster_struct *rc)
{
  for (int i = 0; i < rc->total_passengers_number; i++)
    if (rc->passengers[i].curr_cart == cart)
    {
      cart->curr_passengers_number -= 1;
      step_out(&rc->passengers[i], cart);
    }

  cart->curr_passengers_number = 0;
}

