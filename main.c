#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <semaphore.h>

#define DAY_IN_SECONDS 1

typedef struct Reservation {
  int reservation_id; // reservation id
  int seatnumber; //last seat number
  int tour_id;
  int passenger_id;
  int agent_id;
} Reservation;


// special array implementation from:
// http://stackoverflow.com/questions/3536153/c-dynamically-growing-array

typedef struct {
  int *array;
  size_t used;
  size_t size;
} Array;

void initArray(Array *a, size_t initialSize) {
  a->array = (int *)malloc(initialSize * sizeof(int));
  a->used = 0;
  a->size = initialSize;
}

void insertArray(Array *a, int element) {
  // a->used is the number of used entries, because a->array[a->used++] updates a->used only *after* the array has been accessed.
  // Therefore a->used can go up to a->size
  if (a->used == a->size) {
    a->size *= 2;
    a->array = (int *)realloc(a->array, a->size * sizeof(int));
  }
  a->array[a->used++] = element;
}

void freeArray(Array *a) {
  free(a->array);
  a->array = NULL;
  a->used = a->size = 0;
}

//used
typedef struct Seat{
  int seatnumber;
  int tour_id;
  int reservation_id;
  int passenger_id;
  char status;
} Seat;

//used
typedef struct Passenger {
  int passenger_id;
  int *rid1;
  int *rid2;
  Array seats;
} Passenger;

typedef struct Agent {
  int agent_id;

} Agent;

//args used
typedef struct PidArgs{
  int pid;
} PidArgs;

typedef struct AidArgs{
  int aid;
} AidArgs;

// used
typedef struct Log{
  time_t ptime;
  int passenger_id;
  int agent_id;
  char operation;
  int seat_number;
  int tour_id;
} Log;

pthread_mutex_t *mutexs;
pthread_mutex_t *mutexs_passenger;

pthread_mutex_t reservation_counter_mutex;
pthread_mutex_t log_mutex;
Seat *seats; //a(vailable), r(eserved), b(ought)
Log *logs;
Passenger *passengers;

int simulation_time;
int num_of_passengers;
int num_of_agents;
int num_of_tours = 1;
int num_of_seats;
int random_seed;
int reservation_unique_counter = 1000;
int log_counter = 0;
sem_t semaphore;

//Passenger starting functions
int makeReservation(int passenger_id, int tour_id, int agent_id, int seatnumber, int day){
  time_t now;
  int index = num_of_seats * (tour_id - 1) + seatnumber -1;
  int result = 1;
  //sem_wait(&semaphore);

  pthread_mutex_lock(&mutexs[index]);
  pthread_mutex_lock(&mutexs_passenger[passenger_id]);

  if (passengers[passenger_id].rid1[day] == -1 || passengers[passenger_id].rid2[day] == -1){
    if (seats[index].status == 'A'){
        seats[index].status = 'R';
        now = time(0);
        seats[index].passenger_id = passenger_id;

        pthread_mutex_lock(&reservation_counter_mutex);
        reservation_unique_counter += 1;
        seats[index].reservation_id = reservation_unique_counter;
        seats[index].tour_id = tour_id;
        pthread_mutex_unlock(&reservation_counter_mutex);

        if (passengers[passenger_id].rid1[day] == -1){
            passengers[passenger_id].rid1[day] = index;
        } else if (passengers[passenger_id].rid2[day] == -1) {
            passengers[passenger_id].rid2[day] = index;
        } else{
          printf("UNEXPECTED RID1 RID2 ERROR!\n");
        }

        pthread_mutex_lock(&log_mutex);
        logs[log_counter].ptime = now;
        logs[log_counter].passenger_id = passenger_id;
        logs[log_counter].agent_id = agent_id;
        logs[log_counter].operation = 'R';
        logs[log_counter].seat_number = seatnumber;
        logs[log_counter].tour_id = tour_id;
        log_counter += 1;
        pthread_mutex_unlock(&log_mutex);
    }
    else{
        result = 0;
        //printf("The seat %d is already reserved.\n", seatnumber);
    }
  }
  else{
      result = 0;
    //printf("You cannot reserve more than 2 seats in 1 day\n");
  }
  pthread_mutex_unlock(&mutexs_passenger[passenger_id]);
  pthread_mutex_unlock(&mutexs[index]);

  return result;
}

void* cancelReservation(int passenger_id, int tour_id, int agent_id, int seatnumber, int day){
    time_t now;
    int index = num_of_seats * (tour_id - 1) + seatnumber -1;

    pthread_mutex_lock(&mutexs[index]);
    pthread_mutex_lock(&mutexs_passenger[passenger_id]);

    if (seats[index].passenger_id == passenger_id){

        if (seats[index].status == 'R'){
          seats[index].status = 'A';
          now = time(0);
          seats[index].passenger_id = NULL;
          seats[index].reservation_id = NULL;
          seats[index].tour_id = NULL;

          if (passengers[passenger_id].rid1[day] == index){
              passengers[passenger_id].rid1[day] = -1;
          } else if (passengers[passenger_id].rid2[day] == index){
              passengers[passenger_id].rid2[day] = -1;
          } else {
            printf("ERROR! reservation records could not be found in passenger\n");
          }

          pthread_mutex_lock(&log_mutex);
          logs[log_counter].ptime = now;
          logs[log_counter].passenger_id = passenger_id;
          logs[log_counter].agent_id = agent_id;
          logs[log_counter].operation = 'C';
          logs[log_counter].seat_number = seatnumber;
          logs[log_counter].tour_id = tour_id;
          log_counter += 1;
          pthread_mutex_unlock(&log_mutex);

          sem_post(&semaphore); // wake up other waiting process
        }
        else{
          printf("This seat is not reserved!\n");
        }
    }
    else{
        printf("This reservation does not belong to given passenger_id!\n");
    }

    pthread_mutex_unlock(&mutexs_passenger[passenger_id]);
    pthread_mutex_unlock(&mutexs[index]);
}

void* buyTicket(int passenger_id, int tour_id, int agent_id, int seatnumber, int day){
  time_t now;
  int index = num_of_seats * (tour_id - 1) + seatnumber -1;
  pthread_mutex_lock(&mutexs[index]);
  pthread_mutex_lock(&mutexs_passenger[passenger_id]);

  if (seats[index].status == 'A' || (seats[index].status == 'R' && seats[index].passenger_id == passenger_id)){
      seats[index].status = 'B';
      now = time(0);
      seats[index].passenger_id = passenger_id;
      seats[index].tour_id = tour_id;
      seats[index].reservation_id = NULL;

      insertArray(&passengers[passenger_id].seats, index);

      if (passengers[passenger_id].rid1[day] == index){
          passengers[passenger_id].rid1[day] = -1;
      } else if (passengers[passenger_id].rid2[day] == index){
          passengers[passenger_id].rid2[day] = -1;
      } else {
        printf("ERROR! reservation records could not be found in passenger\n");
      }

      pthread_mutex_lock(&log_mutex);
      logs[log_counter].ptime = now;
      logs[log_counter].passenger_id = passenger_id;
      logs[log_counter].agent_id = agent_id;
      logs[log_counter].operation = 'B';
      logs[log_counter].seat_number = seatnumber;
      logs[log_counter].tour_id = tour_id;
      log_counter += 1;
      pthread_mutex_unlock(&log_mutex);
  }
  else{
      printf("The seat %d %d is not available or not reserved by this passenger.\n",tour_id, seatnumber);
  }

  pthread_mutex_unlock(&mutexs_passenger[passenger_id]);
  pthread_mutex_unlock(&mutexs[index]);
}

void* doRandomAgentActions(void* arg){
    AidArgs *aarg = arg;
    int agent_id = aarg->aid;
    int rseat, rtour;
    int i;
    long int start;
    float r;
    int rres;
    int temp_index;
    int rcount = 0;
    int rindices[4];
    int rdays[4];
    int passenger_id;
    srand(random_seed);

    for (i= 0; i < simulation_time; i++){
        start = (long int) time(0);
        while (time(0) < start + DAY_IN_SECONDS){
            r = (rand() % 10000) / 10000.0;
            rcount = 0;
            passenger_id = (int) (rand() % num_of_passengers);

            if (r < 0.4){
              rseat = (int)(rand() % num_of_seats + 1);
              rtour = (int)(rand() % num_of_tours + 1);
              makeReservation(passenger_id, rtour, agent_id, rseat,i);
            }
            else if (r < 0.6){
              if (i > 0){
                if (passengers[passenger_id].rid1[i-1] != -1){
                    rindices[rcount] = passengers[passenger_id].rid1[i-1];
                    rdays[rcount] = i-1;
                    rcount += 1;
                }
                if (passengers[passenger_id].rid2[i-1] != -1){
                    rindices[rcount] = passengers[passenger_id].rid2[i-1];
                    rdays[rcount] = i-1;
                    rcount += 1;
                }
              }

              if (passengers[passenger_id].rid2[i] != -1){
                  rindices[rcount] = passengers[passenger_id].rid2[i];
                  rdays[rcount] = i;
                  rcount += 1;
              }

              if (passengers[passenger_id].rid1[i] != -1){
                  rindices[rcount] = passengers[passenger_id].rid1[i];
                  rdays[rcount] = i;
                  rcount += 1;
              }

              if (rcount > 1){
                  rres = (int) (rand() % rcount);
                  temp_index = rindices[rres];
                  rtour = (temp_index / num_of_seats) + 1;
                  rseat = (temp_index % num_of_seats) + 1;
                  printf("AGENT CANCEL REQUEST from random %d %d %d\n",rtour,rseat,rdays[rres]);
                  cancelReservation(passenger_id, rtour, agent_id, rseat, rdays[rres]);

              } else if (rcount == 1){
                temp_index = rindices[0];
                rtour = (temp_index / num_of_seats) + 1;
                rseat = (temp_index % num_of_seats) + 1;
                printf("AGENT CANCEL REQUEST from one selection %d %d %d\n",rtour,rseat,rdays[0]);
                cancelReservation(passenger_id, rtour, agent_id, rseat, rdays[0]);
              } else {
                  //printf("No reservation to cancel!\n");
              }
            }
            else if (r < 0.8){
              //printf("view the reserved ticket for %d\n", passenger_id);
            }
            else{
              if (i > 0){
                if (passengers[passenger_id].rid1[i-1] != -1){
                    rindices[rcount] = passengers[passenger_id].rid1[i-1];
                    rdays[rcount] = i-1;
                    rcount += 1;
                }
                if (passengers[passenger_id].rid2[i-1] != -1){
                    rindices[rcount] = passengers[passenger_id].rid2[i-1];
                    rdays[rcount] = i-1;
                    rcount += 1;
                }
              }

              if (passengers[passenger_id].rid2[i] != -1){
                  rindices[rcount] = passengers[passenger_id].rid2[i];
                  rdays[rcount] = i;
                  rcount += 1;
              }

              if (passengers[passenger_id].rid1[i] != -1){
                  rindices[rcount] = passengers[passenger_id].rid1[i];
                  rdays[rcount] = i;
                  rcount += 1;
              }

              if (rcount > 1){
                  rres = (int) (rand() % rcount);
                  temp_index = rindices[rres];
                  rtour = (temp_index / num_of_seats) + 1;
                  rseat = (temp_index % num_of_seats) + 1;
                  printf("AGENT BUY REQUEST: from random %d %d %d\n",rtour,rseat,rdays[rres]);
                  buyTicket(passenger_id, rtour, agent_id, rseat, rdays[rres]);

              } else if (rcount == 1){
                temp_index = rindices[0];
                rtour = (temp_index / num_of_seats) + 1;
                rseat = (temp_index % num_of_seats) + 1;
                printf("AGENT BUY REQUEST: from one selection %d %d %d\n",rtour,rseat,rdays[0]);
                buyTicket(passenger_id, rtour, agent_id, rseat, rdays[0]);
              } else {
                  //printf("No reservation to buy!\n");
              }
            }
        }
        // ONE DAY PASSED
    }
}

void* doRandomPassengerActions(void* arg){
    PidArgs *parg = arg;
    int passenger_id = parg->pid;
    int rseat, rtour;
    int i;
    long int start;
    float r;
    int rres;
    int temp_index;
    int rcount = 0;
    int rindices[4];
    int rdays[4];
    int resResult;
    int resperdayCount = 0;
    srand(random_seed);

    for (i= 0; i < simulation_time; i++){
        start = (long int) time(0);
        while (time(0) < start + DAY_IN_SECONDS){
            r = (rand() % 10000) / 10000.0;
            rcount = 0;

            if (r < 0.4){
              if (resperdayCount < 2){
                rseat = (int)(rand() % num_of_seats + 1);
                rtour = (int)(rand() % num_of_tours + 1);
                resResult = makeReservation(passenger_id, rtour, 0, rseat,i);
                if (resResult == 1){
                    resperdayCount += 1;
                }
              }
            }
            else if (r < 0.6){
              if (i > 0){
                if (passengers[passenger_id].rid1[i-1] != -1){
                    rindices[rcount] = passengers[passenger_id].rid1[i-1];
                    rdays[rcount] = i-1;
                    rcount += 1;
                }
                if (passengers[passenger_id].rid2[i-1] != -1){
                    rindices[rcount] = passengers[passenger_id].rid2[i-1];
                    rdays[rcount] = i-1;
                    rcount += 1;
                }
              }

              if (passengers[passenger_id].rid2[i] != -1){
                  rindices[rcount] = passengers[passenger_id].rid2[i];
                  rdays[rcount] = i;
                  rcount += 1;
              }

              if (passengers[passenger_id].rid1[i] != -1){
                  rindices[rcount] = passengers[passenger_id].rid1[i];
                  rdays[rcount] = i;
                  rcount += 1;
              }

              if (rcount > 1){
                  rres = (int) (rand() % rcount);
                  temp_index = rindices[rres];
                  rtour = (temp_index / num_of_seats) + 1;
                  rseat = (temp_index % num_of_seats) + 1;
                  printf("PASSENGER CANCEL REQUEST from random %d %d %d\n",rtour,rseat,rdays[rres]);
                  cancelReservation(passenger_id, rtour, 0, rseat, rdays[rres]);

              } else if (rcount == 1){
                temp_index = rindices[0];
                rtour = (temp_index / num_of_seats) + 1;
                rseat = (temp_index % num_of_seats) + 1;
                printf("PASSENGER CANCEL REQUEST from one selection %d %d %d\n",rtour,rseat,rdays[0]);
                cancelReservation(passenger_id, rtour, 0, rseat, rdays[0]);
              } else {
                  //printf("No reservation to cancel!\n");
              }
            }
            else if (r < 0.8){
              //printf("view the reserved ticket for %d\n", passenger_id);
            }
            else{
              if (i > 0){
                if (passengers[passenger_id].rid1[i-1] != -1){
                    rindices[rcount] = passengers[passenger_id].rid1[i-1];
                    rdays[rcount] = i-1;
                    rcount += 1;
                }
                if (passengers[passenger_id].rid2[i-1] != -1){
                    rindices[rcount] = passengers[passenger_id].rid2[i-1];
                    rdays[rcount] = i-1;
                    rcount += 1;
                }
              }

              if (passengers[passenger_id].rid2[i] != -1){
                  rindices[rcount] = passengers[passenger_id].rid2[i];
                  rdays[rcount] = i;
                  rcount += 1;
              }

              if (passengers[passenger_id].rid1[i] != -1){
                  rindices[rcount] = passengers[passenger_id].rid1[i];
                  rdays[rcount] = i;
                  rcount += 1;
              }

              if (rcount > 1){
                  rres = (int) (rand() % rcount);
                  temp_index = rindices[rres];
                  rtour = (temp_index / num_of_seats) + 1;
                  rseat = (temp_index % num_of_seats) + 1;
                  printf("PASSENGER BUY REQUEST: from random %d %d %d\n",rtour,rseat,rdays[rres]);
                  buyTicket(passenger_id, rtour, 0, rseat, rdays[rres]);

              } else if (rcount == 1){
                temp_index = rindices[0];
                rtour = (temp_index / num_of_seats) + 1;
                rseat = (temp_index % num_of_seats) + 1;
                printf("PASSENGER BUY REQUEST: from one selection %d %d %d\n",rtour,rseat,rdays[0]);
                buyTicket(passenger_id, rtour, 0, rseat, rdays[0]);
              } else {
                  //printf("No reservation to buy!\n");
              }
            }
        }
        resperdayCount = 0;
        // ONE DAY PASSED
    }
}

int main(int argc, char *argv[]){
  int i,j,k,d,ind,j2,k2, m,g;
  pthread_t *passengerThreads;
  pthread_t *agentThreads;
  PidArgs *pargs;
  AidArgs *aargs;

  for (i = 0; i< argc; i++){
    if (argv[i][0] == '-'){
      if (argv[i][1] == 'd'){
          simulation_time = atoi(argv[i+1]);
      }
      else if (argv[i][1] == 'p'){
        num_of_passengers = atoi(argv[i+1]);
      }
      else if (argv[i][1] == 'a'){
        num_of_agents = atoi(argv[i+1]);
      }
      else if (argv[i][1] == 't'){
        num_of_tours = atoi(argv[i+1]);
      }
      else if (argv[i][1] == 's'){
        num_of_seats = atoi(argv[i+1]);
      }
      else if (argv[i][1] == 'r'){
        random_seed = atoi(argv[i+1]);
      }
      else{
        printf("Unknown argument %c.\n", argv[i][1]);
      }
    }
  }

  mutexs = malloc(sizeof(pthread_mutex_t) * num_of_seats * num_of_tours);
  mutexs_passenger = malloc(sizeof(pthread_mutex_t) * num_of_passengers);
  seats = malloc(sizeof(Seat) * num_of_seats * num_of_tours);
  logs =  malloc(sizeof(Log) * 10000);
  passengers = malloc(sizeof(Passenger) * num_of_passengers);

  sem_init(&semaphore,0, num_of_tours * num_of_seats);

  for (m = 0; m < num_of_tours; m++){
    for (i= 0; i < num_of_seats; i++){
        int index = m * num_of_seats + i;
        seats[index].status = 'A';
        seats[index].seatnumber = i + 1;
        seats[index].tour_id = m + 1;
        pthread_mutex_init(&mutexs[index], NULL);
    }
  }

  for(m = 0; m < num_of_passengers; m++){
      passengers[m].passenger_id = m;
      passengers[m].rid1 = malloc(sizeof(int) * simulation_time);
      passengers[m].rid2 = malloc(sizeof(int) * simulation_time);
      for (g = 0; g < simulation_time; g++){
        passengers[m].rid1[g] = -1;
        passengers[m].rid2[g] = -1;
      }
      initArray(&passengers[m].seats, 5);
      pthread_mutex_init(&mutexs_passenger[m], NULL);
  }

  passengerThreads = malloc(sizeof(pthread_t) * num_of_passengers);
  pargs = malloc(sizeof(PidArgs) * num_of_passengers);
  agentThreads = malloc(sizeof(pthread_t) * num_of_agents);
  aargs = malloc(sizeof(AidArgs) * num_of_agents);

  pthread_mutex_init(&reservation_counter_mutex, NULL);
  pthread_mutex_init(&log_mutex, NULL);

  for(j = 0; j< num_of_passengers; j++){
    pargs[j].pid = j;
    pthread_create(&passengerThreads[j],NULL, doRandomPassengerActions, &pargs[j]);
  }

  for(j2 = 0; j2 < num_of_agents; j2++){
    aargs[j2].aid = j2 + 1;
    pthread_create(&agentThreads[j2],NULL, doRandomAgentActions, &aargs[j2]);
  }

  for (k=0; k< num_of_passengers; k++){
      pthread_join(passengerThreads[k],NULL);
  }

  for(k2 = 0; k2 < num_of_agents; k2++){
      pthread_join(agentThreads[k2],NULL);
  }

  //printf("%-10s\t%-5s\t%-9s\t%-7s\t%-7s","Time","P_ID","A_ID","Operation","Seat No", "Tour No");
  printf("Time     \tP_ID\tA_ID\tOperation\tSeat No \tTour No\n");

  for(ind = 0; ind < log_counter; ind++){
      time_t t = logs[ind].ptime;
      struct tm *time_info;
      time_info = localtime(&t);
      printf("%-2d:%-2d:%-2d\t%-5d\t%-5d\t%-9c\t%-7d \t%-7d\n", time_info->tm_hour, time_info->tm_min, time_info->tm_sec, logs[ind].passenger_id, logs[ind].agent_id, logs[ind].operation, logs[ind].seat_number, logs[ind].tour_id);
  }

  for (d = 0; d < num_of_seats * num_of_tours; d++){
    pthread_mutex_destroy(&mutexs[d]);
  }

  pthread_mutex_destroy(&reservation_counter_mutex);
  pthread_mutex_destroy(&log_mutex);
  free(pargs);
  free(aargs);
  free(passengerThreads);
  free(agentThreads);
  free(mutexs);
  free(mutexs_passenger);
  free(logs);
  free(seats);
  free(passengers);
}
