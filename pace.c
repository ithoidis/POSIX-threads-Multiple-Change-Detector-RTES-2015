/*
  Multiple Change Detector
  RTES 2015

  Nikos P. Pitsianis 
  AUTh 2015

  Iordanis P. Thoidis
  Student
 */

#include <pthread.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>

long udelay = 1000000;
float *det_time;
int changed = 0, detected = 0;
int N, NUM_THREADS;

void exitfunc(int sig)
{
  int i;
  float max = det_time[0], min = det_time[0];
  float mean = 0, sdev = 0, range;
  printf("\n%d Threads & %d Signals & Change frequency %f s\n", NUM_THREADS, N, (float) udelay/1000000);
	printf("Changed: %d detected: %d \n", changed, detected);
	
  for (i = 0; i < detected; ++i)
  {
    mean += det_time[i];
    if (det_time[i] > max)
      max = det_time[i];
    if (det_time[i] < min)
      min = det_time[i];
  }
  printf("Real time Detection time: %.0f μs \n", max*1000000);
  mean /= detected;
	for (i = 0; i < detected; ++i){
    sdev += (det_time[i]-mean)*(det_time[i]-mean);
  }
  sdev = sqrt(sdev/detected);
  range = max - min;
  printf("Mean Detection Time: %.0f μs and Standard deviation: %.0f μs\n", mean*1000000, sdev*1000000); 
  printf("Value Range: %.0f μs\n", 1000000*range);

	_exit(0);
}

volatile int *signalArray;
volatile int flag=1;
struct timeval *timeStamp;
pthread_mutex_t mutexdet;

void *SensorSignalReader (void *args);
void *ChangeDetector (void *args);

int main(int argc, char **argv)
{

  NUM_THREADS = sysconf(_SC_NPROCESSORS_ONLN);
  
  printf("Number of Processor Cores = %d\n", NUM_THREADS);

  long i;

  if (argc != 3) {
    printf("Usage: %s N NUM_THREADS \n"
           " where\n"
           " N    : number of signals to monitor\n"
           " NUM_THREADS    : number of threads to execute\n"
	   , argv[0]);
    return (1);
  }
  
  NUM_THREADS = atoi(argv[2]);

  N = atoi(argv[1]);
  
  printf("%d Threads & %d Signals \n", NUM_THREADS, N);

  // set a timed signal to terminate the program
  signal(SIGALRM, exitfunc);
  alarm(20); // after 20 sec

  // Allocate signal, time-stamp arrays and thread handles
  signalArray = (int *) malloc(N*sizeof(int));
  timeStamp = (struct timeval *) malloc(N*sizeof(struct timeval));
  det_time = (float*) malloc(20000*sizeof(float));

  pthread_t sigGen;
  pthread_t sigDet[NUM_THREADS];
  
  pthread_mutex_init(&mutexdet, NULL);
  pthread_attr_t pthread_custom_attr;
  pthread_attr_init(&pthread_custom_attr);
  pthread_attr_setdetachstate(&pthread_custom_attr, PTHREAD_CREATE_JOINABLE);
  
  for (i=0; i<N; i++) {
    signalArray[i] = 0;
  }

  for(i=0; i<NUM_THREADS; i++){
      pthread_create(&sigDet[i],&pthread_custom_attr, ChangeDetector, (void *) i );
  }

  sleep(2);
  pthread_create (&sigGen, NULL, SensorSignalReader, NULL);
  
  for(i=0; i<NUM_THREADS; i++){
    pthread_join(sigDet[i], NULL);
  }
  return 0;
}


void *SensorSignalReader (void *arg)
{

  char buffer[30];
  struct timeval tv; 
  time_t curtime;

  srand(time(NULL));

  while (1) {
    int t = rand() % 10 + 1; // wait up to 1 sec in 10ths
    // Set the signal frequency
    usleep(t*udelay/10);
    
    int r = rand() % N;
    signalArray[r] ^= 1;

    if (signalArray[r]) {
      changed++;
      gettimeofday(&tv, NULL);
      timeStamp[r] = tv;
      curtime = tv.tv_sec;
      strftime(buffer,30,"%d-%m-%Y  %T.",localtime(&curtime));
      printf("Changed %5d at Time %s%d\n",r,buffer,tv.tv_usec);
    }
  }
}

void *ChangeDetector (void *arg1)
{ 
  long tid;
  long i;
  tid = (long) arg1;
  if (tid >= N)
    pthread_exit(NULL);
  long Num_cells;
  Num_cells = N / NUM_THREADS;
  if (N%NUM_THREADS!=0)  Num_cells++;
  long start = tid*Num_cells;
  long end = (start+Num_cells-1 < N) ? start+Num_cells-1 : N-1;
  if (tid == NUM_THREADS-1)
    end = N-1;
  if(start>end)
      pthread_exit(NULL);
  printf("Thread %ld started. [%ld,%ld]\n", tid, start, end);
  char buffer[30];
  struct timeval tv; 
  time_t curtime;

  int comp_Array[Num_cells];
  for (i = 0; i < Num_cells; i++)
      comp_Array[i] = signalArray[start+i];

 while (1) {
    for (i =0; i < Num_cells; ++i)
    {
      if (signalArray[start+i]!=comp_Array[i]) {
          if  (signalArray[start+i]==1){
          	gettimeofday(&tv, NULL); 
          	curtime = tv.tv_sec;
          	strftime(buffer,30,"%d-%m-%Y  %T.",localtime(&curtime));
          	printf("Detcted %5ld at Time %s%d after %ld.%06d sec // Thread %ld \n",start+i,buffer,tv.tv_usec,
          	tv.tv_sec - timeStamp[start+i].tv_sec,
          	tv.tv_usec - timeStamp[start+i].tv_usec, tid);            
            //mutex atomic operation increase detected++ value and add a new det_time
            //Realloc and save times for the statistics
            pthread_mutex_lock (&mutexdet);            
            //det_time =  (float*) realloc(det_time, detected * sizeof(float));
            det_time[detected] = (float) (tv.tv_sec - timeStamp[start+i].tv_sec) +  0.000001 * (tv.tv_usec - timeStamp[start+i].tv_usec);
            detected++;
            pthread_mutex_unlock (&mutexdet);

          }
          comp_Array[i] = signalArray[start+i];
      }
    } 
  }

}

