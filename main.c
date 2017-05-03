#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define DAY_IN_SECONDS 1

typedef struct Reservation {
  int reservation_id; // reservation id
  int seatnumber; //last seat number
  int tour_id;
  int passenger_id;
  int agent_id;
} Reservation;

typedef struct Seat{
  int seatnumber;
  int tour_id;
  int reservation_id;
  int passenger_id;
  char status;
} Seat;

typedef struct Passenger {
  int passenger_id;
  Reservation *reservations;
  Seat *seats;
} Passenger;

typedef struct Agent {
  int agent_id;

} Agent;

typedef struct PidArgs{
  int pid;
} PidArgs;

typedef struct AidArgs{
  int aid;
} AidArgs;

typedef struct Log{
  time_t ptime;
  int passenger_id;
  int agent_id;
  char operation;
  int seat_number;
  int tour_id;
} Log;

pthread_mutex_t *mutexs; //her seat için ayrı mutex!?
pthread_mutex_t reservation_counter_mutex;
pthread_mutex_t log_mutex;
Seat *seats; //a(vailable), r(eserved), b(ought)
Log *logs;

int simulation_time;
int num_of_passengers;
int num_of_agents;
int num_of_tours = 1;
int num_of_seats;
int random_seed;
int reservation_unique_counter = 1000;
int log_counter = 0;

//Passenger starting functions
void* makeReservation(int passenger_id, int tour_id, int agent_id, int seatnumber){
  time_t now;
  int index = num_of_seats * (tour_id - 1) + seatnumber -1;
  pthread_mutex_lock(&mutexs[index]);

  if (seats[index].status == 'A'){
      seats[index].status = 'R';
      now = time(0);
      seats[index].passenger_id = passenger_id;

      pthread_mutex_lock(&reservation_counter_mutex);
      reservation_unique_counter += 1;
      seats[index].reservation_id = reservation_unique_counter;
      seats[index].tour_id = tour_id;
      pthread_mutex_unlock(&reservation_counter_mutex);

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
      //printf("The seat %d is already reserved.\n", seatnumber);
  }

  pthread_mutex_unlock(&mutexs[index]);
}

void* cancelReservation(int passenger_id, int tour_id, int agent_id, int seatnumber){
    time_t now;
    int index = num_of_seats * (tour_id - 1) + seatnumber -1;
    pthread_mutex_lock(&mutexs[index]);

    if (seats[index].passenger_id == passenger_id && seats[index].tour_id == tour_id){

        if (seats[index].status == 'R'){
          seats[index].status = 'A';
          now = time(0);
          seats[index].passenger_id = NULL;
          seats[index].reservation_id = NULL;
          seats[index].tour_id = NULL;

          pthread_mutex_lock(&log_mutex);
          logs[log_counter].ptime = now;
          logs[log_counter].passenger_id = passenger_id;
          logs[log_counter].agent_id = agent_id;
          logs[log_counter].operation = 'C';
          logs[log_counter].seat_number = seatnumber;
          logs[log_counter].tour_id = tour_id;
          log_counter += 1;
          pthread_mutex_unlock(&log_mutex);
        }
        else{
          printf("This seat is not reserved!");
        }
    }
    else{
        printf("This reservation does not belong to given passenger_id!\n");
    }

    pthread_mutex_unlock(&mutexs[index]);
}

void* buyTicket(int passenger_id, int tour_id, int agent_id, int seatnumber){
  time_t now;
  int index = num_of_seats * (tour_id - 1) + seatnumber -1;
  pthread_mutex_lock(&mutexs[index]);

  if (seats[index].status == 'A' || (seats[index].status == 'R' && seats[index].passenger_id == passenger_id)){
      seats[index].status = 'B';
      now = time(0);
      seats[index].passenger_id = passenger_id;
      seats[index].tour_id = tour_id;
      seats[index].reservation_id = NULL;

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
      printf("The seat is not available or not reserved by this passenger.");
  }
  pthread_mutex_unlock(&mutexs[index]);
}

void* doRandomAgentActions(void* arg){


}

void* doRandomPassengerActions(void* arg){
    PidArgs *parg = arg;
    int passenger_id = parg->pid;
    int rseat, rtour;

    long int start = (long int) time(0);
    float r;
    srand(random_seed);
    //printf("%li\n",start);
    while (time(0) < start + simulation_time * DAY_IN_SECONDS){
        r = (rand() % 10000) / 10000.0;
        if (r < 0.4){
          rseat = (int)(rand() % num_of_seats + 1);
          rtour = (int)(rand() % num_of_tours + 1);
          makeReservation(passenger_id, rtour, 0, rseat);
        }
        else if (r < 0.6){
          //cancelReservation(passenger_id, , 0, );
        }
        else if (r < 0.8){
          //printf("view the reserved ticket for %d\n", passenger_id);
        }
        else{
          //buyTicket(passenger_id, , 0, 0);
          //printf("buy the reserved ticket for %d\n", passenger_id);
        }
    }
}

int main(int argc, char *argv[]){
  int i,j,k,d,ind,j2,k2, m;
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
  seats = malloc(sizeof(Seat) * num_of_seats * num_of_tours);
  logs =  malloc(sizeof(Log) * 10000);

  for (m = 0; m < num_of_tours; m++){
    for (i= 0; i < num_of_seats; i++){
        int index = m * num_of_seats + i;
        seats[index].status = 'A';
        seats[index].seatnumber = i + 1;
        seats[index].tour_id = m + 1;
        pthread_mutex_init(&mutexs[index], NULL);
    }
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
    pargs[j2].pid = j2;
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
  free(logs);
  free(seats);
}
