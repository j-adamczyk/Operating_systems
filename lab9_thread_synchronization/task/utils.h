#ifndef utils
#define utils


typedef enum cart_state
{
  CART_RIDING,
  CART_BUTTON_WAIT,
  CART_BUTTON_PRESSED,
  CART_WAITING,
  CART_FINISHED
} cart_state;


typedef enum passenger_state
{
  RIDING,
  IN_CART,
  WAITING
} passenger_state;


typedef struct cart_data
{
  int ID;
  cart_state state;  // RIDING/WAITING
  int rounds;
  int total_rounds;
  int curr_passengers_number;
  int max_passengers_number;
  int passenger_to_push;
  bool door_open;
} cart_data;


typedef struct passenger_data
{
  int ID;
  passenger_state state;
  cart_data *curr_cart;
} passenger_data;


typedef struct rollercoaster_struct
{
  int curr_cart;  // current car accepting passengers
  int curr_riding;  // current riding car

  int total_carts_number;
  int total_passengers_number;
  int passengers_in_line;

  cart_data *carts;
  passenger_data *passengers;
} rollercoaster_struct;


void print_stamp (bool, int);
void step_out (passenger_data *, cart_data *);
void empty_car (cart_data *, rollercoaster_struct *);

#endif
