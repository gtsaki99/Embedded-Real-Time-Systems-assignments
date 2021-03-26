#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>

#define QUEUESIZE 10
#define LOOP 20
#define P 1
#define Q 1

int left = LOOP * QUEUESIZE, all = LOOP * QUEUESIZE, added = 0;
double durations[LOOP * QUEUESIZE];

struct timeval tic()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv;
}

double toc(struct timeval begin)
{
    struct timeval end;
    gettimeofday(&end, NULL);
    double stime = ((double)(end.tv_sec - begin.tv_sec) * 1000) +
                   ((double)(end.tv_usec - begin.tv_usec) / 1000);
    stime = stime / 1000;
    return (stime);
}

void *producer(void *args);
void *consumer(void *args);
void *theWork(void *arg);
void writeToFile(double m);

typedef struct
{
    void *(*work)(void *);
    void *arg;
} workFunction;

typedef struct
{
    int ar;
    struct timeval timer;
} argum;

typedef struct
{
    workFunction* buf[QUEUESIZE];
    long head, tail;
    int full, empty;
    pthread_mutex_t *mut;
    pthread_cond_t *notFull, *notEmpty;
} queue;

queue *queueInit(void);
void queueDelete(queue *q);
void queueAdd(queue *q, workFunction *in);
void queuePop(queue *q, workFunction *out);

int main()
{
    queue *fifo;
    pthread_t prod[P], cons[Q];

    fifo = queueInit();
    if (fifo == NULL)
    {
        fprintf(stderr, "Main: Queue Init failed.\n");
        exit(1);
    }

    for (int i = 0; i < P; i++)
        pthread_create(&prod[i], NULL, producer, fifo);
    for (int i = 0; i < Q; i++)
        pthread_create(&cons[i], NULL, consumer, fifo);
    for (int i = 0; i < P; i++)
        pthread_join(prod[i], NULL);
    for (int i = 0; i < Q; i++)
        pthread_join(cons[i], NULL);
    queueDelete(fifo);

    double sum = 0, mean = 0;
    for(int i = 0; i < all; i++)
        sum += durations[i];
    mean = (double)sum / all;
    writeToFile(mean);
    return 0;
}

void writeToFile(double m)
{
    FILE *fp;
    char fname[23] = "Experiment P=p,Q=q.txt";
    fname[13] = P + '0';
    fname[17] = Q + '0';
   fp = fopen(fname, "w+");
   fprintf(fp, "Number of producers: %d\nNumber of consumers: %d\nTotal elements added: %d\nMean time calculated: %f", P, Q, all, m);
   fclose(fp);
}

void *theWork (void *arg) { 
    int a = *((int *)arg);
    printf("Hello, I was pulled from thread #%ld(POSIX thread id) and I was given this integer: %d.\n", pthread_self(), a);
    printf("So I am going to do some random calculations using it.\n");
    double s = a;
    float temp = 0;
    for(int i = 0; i < 500; i++)
    {
        temp = tan(s);
        s += temp;
        temp = 0;
    }
    printf("Here is the result of them: %f.\n", s);
    s = 0;
}

void *producer(void *q)
{
    queue *fifo;
    int i;

    fifo = (queue *)q;

    int *argArr = (int *)malloc(all * sizeof(int));

    for (i = 0; i < all; i++)
        argArr[i] = i;

    workFunction *workFArr;
    workFArr = (workFunction *)malloc(all * sizeof(workFunction));
    
    argum *argumArr;
    argumArr = (argum *)malloc(all * sizeof(argum));

    for (i = 1; added < all; i++)
    {
        pthread_mutex_lock(fifo->mut);

        (argumArr + i)->ar = argArr[added];
        (argumArr + i)->timer = tic();

        (workFArr + i)->arg = (argumArr + i);
        (workFArr + i)->work = theWork;

        added++;

        while (fifo->full)
        {
            // printf("Producer: queue FULL.\n");
            pthread_cond_wait(fifo->notFull, fifo->mut);
        }
        queueAdd(fifo, (workFArr + i));
        pthread_mutex_unlock(fifo->mut);
        pthread_cond_signal(fifo->notEmpty);
    }
    return (NULL);
}

void *consumer(void *q)
{
    queue *fifo;
    int i;

    fifo = (queue *)q;

    while(left > Q - 1)
    {   
        pthread_mutex_lock(fifo->mut);
        
        while (fifo->empty)
        {
            printf("Consumer: queue EMPTY.\n");
            pthread_cond_wait(fifo->notEmpty, fifo->mut);
            if(left < 0)
                exit(0);
        }
        
        workFunction wf;
        queuePop(fifo, &wf);
        
        argum *a;
        a = wf.arg;
        int it = a -> ar;
        struct timeval start = a -> timer;
        double time = toc(start);

        (*wf.work)(&it);
        
        left--;
        durations[added] = time;

        pthread_mutex_unlock(fifo->mut);
        pthread_cond_signal(fifo->notFull);
    }

    return (NULL);
}

queue *queueInit(void)
{
    queue *q;

    q = (queue *)malloc(sizeof(queue));
    if (q == NULL)
        return (NULL);

    q->empty = 1;
    q->full = 0;
    q->head = 0;
    q->tail = 0;
    q->mut = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(q->mut, NULL);
    q->notFull = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
    pthread_cond_init(q->notFull, NULL);
    q->notEmpty = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
    pthread_cond_init(q->notEmpty, NULL);

    return (q);
}

void queueDelete(queue *q)
{
    pthread_mutex_destroy(q->mut);
    free(q->mut);
    pthread_cond_destroy(q->notFull);
    free(q->notFull);
    pthread_cond_destroy(q->notEmpty);
    free(q->notEmpty);
    free(q);
}

void queueAdd(queue *q, workFunction *in)
{
    q->buf[q->tail] = in;
    q->tail++;
    if (q->tail == QUEUESIZE)
        q->tail = 0;
    if (q->tail == q->head)
        q->full = 1;
    q->empty = 0;

    return;
}

void queuePop(queue *q, workFunction *out)
{
    *out = *q->buf[q->head];

    q->head++;
    if (q->head == QUEUESIZE)
        q->head = 0;
    if (q->head == q->tail)
        q->empty = 1;
    q->full = 0;

    return;
}