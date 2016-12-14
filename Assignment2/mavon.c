// The MIT License (MIT)
// 
// Copyright (c) 2016 Trevor Bakker 
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "train.h"

void trainArrives( uint32_t train_id, enum TRAIN_DIRECTION train_direction );
void trainCross  ( uint32_t train_id, enum TRAIN_DIRECTION train_direction );
void trainLeaves ( uint32_t train_id, enum TRAIN_DIRECTION train_direction );

// Current time of day in seconds since midnight
int32_t  current_time;
uint32_t clock_tick;

// Current train in the intersection
uint32_t in_intersection;

int isIntersectionEmpty(){
  return in_intersection == INTERSECTION_EMPTY;
}

struct train_struct{
  int id;
  int direction;
};

#define DIRECTION 5
int trainCount[DIRECTION];

int consecCount[DIRECTION];

struct train_struct *ts;

pthread_cond_t north_cond;
pthread_cond_t south_cond;
pthread_cond_t east_cond;
pthread_cond_t west_cond;

pthread_mutex_t north_mutex;
pthread_mutex_t south_mutex;
pthread_mutex_t east_mutex;
pthread_mutex_t west_mutex;

pthread_mutex_t intersection_mutex;

void * trainLogic( void * val )
{
  struct train_struct *ts = val;
  int id = ts->id;
  int direction = ts->direction;

  // Swich to set cond_wait for the direction of the thread.
  switch(direction){
    case 1:
      pthread_cond_wait(&north_cond,&north_mutex);
      break;
    case 2:
      pthread_cond_wait(&east_cond,&east_mutex);
      break;
    case 3:
      pthread_cond_wait(&south_cond,&south_mutex);
      break;
    case 4:
      pthread_cond_wait(&west_cond,&west_mutex);
      break;
  }

  // Intersection lock so that no other string crosses at the same time.
  pthread_mutex_lock( &intersection_mutex );
  // Unlock the direction mutex.
  switch(direction){
    case 1:
      pthread_mutex_unlock(&north_mutex);
      break;
    case 2:
      pthread_mutex_unlock(&east_mutex);
      break;
    case 3:
      pthread_mutex_unlock(&south_mutex);
      break;
    case 4:
      pthread_mutex_unlock(&west_mutex);
      break;
  }
  trainCross(id,direction);
  pthread_mutex_unlock( &intersection_mutex );

  free (ts);
}

void trainLeaves( uint32_t train_id, enum TRAIN_DIRECTION train_direction )
{
  fprintf( stdout, "Current time: %d MAV %d heading %s leaving the intersection\n",
           current_time, train_id, directionAsString[ train_direction ] );

  in_intersection = INTERSECTION_EMPTY;

  // Reduce this value as train has exited.
  trainCount[train_direction]--;

  // TODO: Handle any cleanup 
}

void trainCross( uint32_t train_id, enum TRAIN_DIRECTION train_direction )
{
  // TODO: Handle any crossing logic

  fprintf( stdout, "Current time: %d MAV %d heading %s entering the intersection\n",
           current_time, train_id, directionAsString[ train_direction ] );

  if( in_intersection == INTERSECTION_EMPTY )
  {
    in_intersection = train_id;
  }
  else
  {
    fprintf( stderr, "CRASH: Train %d collided with train %d\n",
                      train_id, in_intersection );
    exit( EXIT_FAILURE );
  }

  // This switch statement is counting the number of consecutive Trains in a row.
  // These values are later used to check for starvation case.
  switch(train_direction){
    case 1:
      consecCount[1]++;
      consecCount[2]=0;
      consecCount[3]=0;
      consecCount[4]=0;
      break;
    case 2:
      consecCount[1]=0;
      consecCount[2]++;
      consecCount[3]=0;
      consecCount[4]=0;
      break;
    case 3:
      consecCount[1]=0;
      consecCount[2]=0;
      consecCount[3]++;
      consecCount[4]=0;
      break;
    case 4:
      consecCount[1]=0;
      consecCount[2]=0;
      consecCount[3]=0;
      consecCount[4]++;
      break;
  }


  // Sleep for 10 microseconds to simulate crossing
  // the intersection
  usleep( 10 * 1000000 / clock_tick );

// Leave the intersection
  trainLeaves( train_id, train_direction );

  return;
}

void trainArrives( uint32_t train_id, enum TRAIN_DIRECTION train_direction )
{
  fprintf( stdout, "Current time: %d MAV %d heading %s arrived at the intersection\n",
           current_time, train_id, directionAsString[ train_direction ] );

  // This keeps tracks of number of trains scheduled.
  trainCount[train_direction]++;
  pthread_t tid;
  ts = (struct train_struct*) malloc( sizeof( struct train_struct ) );
  ts->id = train_id;
  ts->direction = train_direction;
  // Pthread Created and it is passed trainLogic and trainstruct.
  pthread_create(&tid, NULL,trainLogic,(void *)ts);

  // TODO: Handle the intersection logic

  return;
}

void mediate( )
{
  if(in_intersection != INTERSECTION_EMPTY){
    return;
  }

  // This takes care of starvation cases.
  if(consecCount[1]==5 & trainCount[2]!=0){
    consecCount[1]=0;
    pthread_cond_signal(&east_cond);
    return;
  }

  if(consecCount[2]==5 & trainCount[3]!=0){
    consecCount[2]=0;
    pthread_cond_signal(&south_cond);
    return;
  }

  if(consecCount[3]==5 & trainCount[4]!=0){
    consecCount[3]=0;
    pthread_cond_signal(&west_cond);
    return;
  }

  if(consecCount[4]==5 & trainCount[1]!=0){
    consecCount[4]=0;
    pthread_cond_signal(&north_cond);
    return;
  }

  // Checks for N-S and E-W cases.
  if(trainCount[NORTH]!=0 && trainCount[SOUTH]!=0 && trainCount[EAST]==0 && trainCount[WEST]==0){
    pthread_cond_signal(&north_cond);
    return;
  }
  if(trainCount[NORTH]==0 && trainCount[SOUTH]==0 && trainCount[EAST]!=0 && trainCount[WEST]!=0){
    pthread_cond_signal(&east_cond);
    return;
                                                                                                                             245,3         58%
  // Check for 4-way intersection.
  if(trainCount[NORTH]!=0 && trainCount[SOUTH]!=0 && trainCount[EAST]!=0 && trainCount[WEST]!=0){
    pthread_cond_signal(&north_cond);
    return;
  }

  // Right of way conditions.
  if(trainCount[NORTH]!=0 && trainCount[WEST]==0){
    pthread_cond_signal(&north_cond);
    return;
  }
  if(trainCount[WEST]!=0 && trainCount[SOUTH]==0){
    pthread_cond_signal(&west_cond);
    return;
  }
  if(trainCount[SOUTH]!=0 && trainCount[EAST]==0){
    pthread_cond_signal(&south_cond);
    return;
  }
  if(trainCount[EAST]!=0 && trainCount[NORTH]==0){
    pthread_cond_signal(&east_cond);
    return;
  }
}

void init( )
{
  // TODO: Any code you need called in the initialization of the application
  // init for scheduled train Count and consecutive train count.
  for(int i =0; i<5 ;i++){
    trainCount[i]=0;
    consecCount[i]=0;
  }

  // Initialize conds and mutex for directions.
  pthread_cond_init(&north_cond,NULL);
  pthread_cond_init(&south_cond,NULL);
  pthread_cond_init(&east_cond,NULL);
  pthread_cond_init(&west_cond,NULL);

  pthread_mutex_init(&north_mutex,NULL);
  pthread_mutex_init(&south_mutex,NULL);
  pthread_mutex_init(&east_mutex,NULL);
  pthread_mutex_init(&west_mutex, NULL);

}

/*
 *
 *
 *  DO NOT MODIFY CODE BELOW THIS LINE
 *
 *
 *
 */

int process( )
{
  // If there are no more scheduled train arrivals
  // then return and exit
  if( scheduleEmpty() ) return 0;

  // If we're done with a day's worth of schedule then
  // we're done.
  if( current_time > SECONDS_IN_A_DAY ) return 0;

  // Check for deadlocks
  mediate( );

  // While we still have scheduled train arrivals and it's time
  // to handle an event
  while( !scheduleEmpty() && current_time >= scheduleFront().arrival_time )
  {

#ifdef DEBUG
                                                                                                                             285,3         81%
    fprintf( stdout, "Dispatching schedule event: time: %d train: %d direction: %s\n",
                      scheduleFront().arrival_time, scheduleFront().train_id,
                      directionAsString[ scheduleFront().train_direction ] );
#endif

    trainArrives( scheduleFront().train_id, scheduleFront().train_direction );

    // Remove the event from the schedule since it's done
    schedulePop( );

  }

  // Sleep for a simulated second. Depending on clock_tick this
  // may equate to 1 real world second down to 1 microsecond
  usleep( 1 * 1000000 / clock_tick );

  current_time ++;

  return 1;
}

int main ( int argc, char * argv[] )
{

  // Initialize time of day to be midnight
  current_time = 0;
  clock_tick   = 1;

  // Verify the user provided a data file name.  If not then
                                                                                                                             321,3         89%
  // print an error and exit the program
  if( argc < 2 )
  {
    fprintf( stderr, "ERROR: You must provide a train schedule data file.\n");
    exit(EXIT_FAILURE);
  }

  // See if there's a second parameter which specifies the clock
  // tick rate.  
  if( argc == 3 )
  {
    int32_t tick = atoi( argv[2] );

    if( tick <= 0 )
    {
      fprintf( stderr, "ERROR: tick rate must be positive.\n");
      exit(EXIT_FAILURE);
    }
    else
    {
      clock_tick = tick;
    }
  }

  buildTrainSchedule( argv[1] );

  // Initialize the intersection to be empty
  in_intersection = INTERSECTION_EMPTY;

  // Call user specific initialization
  init( );

  // Start running the MAV manager
  while( process() );

  return 0;
}

