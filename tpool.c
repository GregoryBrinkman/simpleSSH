#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include "tpool.h"

#define INIT_TASKS_PER_THREAD 4

typedef struct taskQ{
  pthread_mutex_t flagLock;
  pthread_cond_t condition;
  int flag;
  pthread_mutex_t queueLock;
  int len;
  int frontIndex;
  int backIndex;
  int* buff;
} taskQueue;

typedef struct tpool{
  int totalThreads;
  pthread_t* threads;
  void (*work)(int);
  taskQueue* queue;
}tpool_t;

static tpool_t pool;
static taskQueue queue;

int resizeQueue(){

  int i;
  pool.queue->buff = (int*)realloc(pool.queue->buff, pool.queue->len * 2 * sizeof(int));

  if(pool.queue->buff == NULL){
    pthread_mutex_unlock(&pool.queue->queueLock);
    perror("realloc failed");
    return 0;
  }

  if(pool.queue->frontIndex > pool.queue->backIndex){
    for(i = pool.queue->frontIndex; i < pool.queue->len; i++)
      pool.queue->buff[pool.queue->len + i] = pool.queue->buff[i];
    pool.queue->frontIndex += pool.queue->len;
  }

  pool.queue->len = pool.queue->len * 2;
  return 1;
}

void printQueue(){
  int i = 0;
  printf("[ ");
  while (i < (int)pool.queue->len) {
    if (((pool.queue->backIndex > pool.queue->frontIndex) && (i < pool.queue->backIndex) && (i >= pool.queue->frontIndex)) ||
        ((pool.queue->backIndex < pool.queue->frontIndex) && ((i < pool.queue->backIndex) || (i >= pool.queue->frontIndex))))
      printf(" %d ", pool.queue->buff[i]);
    else
      printf(" - ");
    i++;
  }
  printf(" ]\n\n");
}
  /* printf("\n\nfrontIndex=%d, backIndex=%d\n", pool.queue->frontIndex, pool.queue->backIndex); */
  /* printf("["); */
  /* int i; */
  /* for(i = 0; i< pool.queue->len; i++) */
  /*   printf(" %d ", pool.queue->buff[i]); */
  /* printf("]\n\n"); */


int tpool_grab_task(int num){

  int retVal;
  pthread_mutex_lock(&(pool.queue->queueLock));
  if(pool.queue->frontIndex == pool.queue->backIndex){
    fprintf(stderr, "Can't get something that isn't there");
    return 0;
  }
  /* printf("inside grab_task lock: thread %d\n", num); */
  retVal = pool.queue->buff[pool.queue->frontIndex];
  pool.queue->frontIndex = (pool.queue->frontIndex + 1) % pool.queue->len;
  pthread_mutex_unlock(&(pool.queue->queueLock));

  return retVal;

}


void* threadLooper(void * threadNum){
  int taskNum;
  int i = *(int*)threadNum;
  free(threadNum);

  while(1){

    pthread_mutex_lock(&pool.queue->flagLock);
    printf("Thread %d inside lock. flag = %d\n", i, pool.queue->flag);
    while(pool.queue->flag <= 0){
      /* printf("Thread %d is waiting\n", i); */
      /* printf("flag was %d\n", pool.queue->flag); */
      printQueue();
      pthread_cond_wait(&(pool.queue->condition), &(pool.queue->flagLock));
    }
    pool.queue->flag -= 1;
    pthread_mutex_unlock(&pool.queue->flagLock);

    if((taskNum = tpool_grab_task(i)) != 0)
      pool.work(taskNum);

    printf("thread %d going in again\n", i);
  }
}


int tpool_add_task(int newtask){
  printf("adding task %d\n", newtask);

  pthread_mutex_lock(&(pool.queue->queueLock));
  if(((pool.queue->backIndex + 1) % pool.queue->len) == pool.queue->frontIndex)
    if(resizeQueue()==0){
      perror("resizing failed");
      return 0;
    }
  pool.queue->buff[pool.queue->backIndex] = newtask;
  pool.queue->backIndex = (pool.queue->backIndex + 1) % pool.queue->len;
  pthread_mutex_unlock(&(pool.queue->queueLock));

  printQueue();

  pthread_mutex_lock(&pool.queue->flagLock);
  pool.queue->flag += 1;
  pthread_cond_signal(&(pool.queue->condition));
  pthread_mutex_unlock(&pool.queue->flagLock);
  printf("thread signaled!\n");


  return 1;
}


int tpool_init(void (*process_task)(int)){
  pool.totalThreads = (int)sysconf(_SC_NPROCESSORS_ONLN);
  pool.work = process_task;


  pool.threads = (pthread_t*) malloc(pool.totalThreads * sizeof(pthread_t));


  pool.queue = &queue;
  pthread_mutex_init(&(pool.queue->queueLock), NULL);
  pthread_mutex_init(&(pool.queue->flagLock), NULL);
  pthread_cond_init(&pool.queue->condition, NULL);
  pool.queue->flag = 0;

  pool.queue->len = INIT_TASKS_PER_THREAD;
  pool.queue->frontIndex = 0;
  pool.queue->backIndex = 0;
  pool.queue->buff = (int*) malloc(pool.queue->len * sizeof(int));

  pthread_attr_t attr;

  //set thread to start in detached mode
  if(pthread_attr_init(&attr) != 0){
    perror("pthread attr");
    exit(EXIT_FAILURE);
  }

  if(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0){
    perror("pthread attr setdetachstate");
    return 0;
  }

  int i, x, ret;
  for(i = 0; i < pool.totalThreads; i++){
    x = i+1;

    int *threadNum = (int*) malloc(sizeof(int));
    *threadNum = x;

    if((ret = pthread_create(&pool.threads[i], &attr, threadLooper, threadNum)) != 0){
      fprintf(stderr, "pthread creation failed");
      return 0;
    }
  }
  return 1;
}
