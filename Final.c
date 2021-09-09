#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>


char macpool[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
int elementsAdded = 0;
int elementsLeft = 1000;
char filename[] = "Mac_Addresses.txt";

typedef struct
{
    char *mac;
    void *(*generate)(void *);
    struct timeval insertionTime;
    bool isClose, old;
} macAddress;

typedef struct
{
    macAddress *buf[1000];
    long head, tail;
    int full, empty;
    pthread_mutex_t *mut;
    pthread_cond_t *notFull, *notEmpty;
} queue;

typedef struct
{
    int* addresses;
    queue* q;
    queue* close;
    struct timeval fourteen; 

} lstruct;

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

void macAddressGenerator(char *mac)
{
    int ran;
    for (int i = 0; i < 17; i++)
    {
        if ((i + 1) % 3 == 0)
            mac[i] = ':';
        else
        {
            ran = rand() % 15;
            mac[i] = macpool[ran];
        }
    }
    mac[17] = '\0';
}

void createStarterAddresses(int *addresses)
{

    FILE *fp = fopen(filename, "w");
    char *mac = (char *)malloc(18);
    for (int i = 0; i < *addresses; i++)
    {
        macAddressGenerator(mac);
        fwrite(mac, 18, 1, fp);
        fwrite("\n", 1, 1, fp);
    }
    fclose(fp);
    free(mac);
}

void saveTimestamp(double timestamp){

    FILE *fp = fopen("Timestamps.bin", "ab");
    fprintf(fp, "%f\n",timestamp);
    fclose(fp);
}

void addMac(char *inputAddress, int *addresses)
{

    FILE *fp = fopen(filename, "a");
    fwrite(inputAddress, 18, 1, fp);
    fwrite("\n", 1, 1, fp);
    fclose(fp);
    (*addresses)++;
}

void readMac(int place, char *inputAddress)
{

    FILE *fp = fopen(filename, "r");
    fseek(fp, place * (19), SEEK_SET);
    fread(inputAddress, 18, 1, fp);
    fclose(fp);
}

void BTNearMe(char *inputAddress, int *addresses)
{
    readMac(rand() % *addresses, inputAddress);
}

void uploadContacts(queue *q)
{
    time_t t;
    time(&t); 
    char *filenameClose = (char *)malloc(strlen("CloseContacts.txt") + 40);
    sprintf(filenameClose, "CloseContacts_%s.txt", ctime(&t));
    FILE *fp = fopen(filenameClose, "w");
    char *temp = (char *)malloc(18);
    if (q->head > q->tail)
    {
        for (int i = q->tail; i < q->head; i++)
        {
            temp = q->buf[i]->mac;
            fwrite(temp, 17, 1, fp);
            fwrite("\n", 1, 1, fp);
        }
    }
    else
    {
        for (int i = q->head; i < q->tail; i++)
        {
            temp = q->buf[i]->mac;
            fwrite(temp, 17, 1, fp);
            fwrite("\n", 1, 1, fp);
        }
    }
    fclose(fp);
}

bool findMac(macAddress *target, queue *q)
{
    macAddress *temp;
    if (q->head > q->tail)
    {
        for (int i = q->tail; i < q->head; i++)
        {
            temp = q->buf[i];
            if (strcmp(target->mac, temp->mac) == 0)
                return true;
            
        }
    }
    else
    {
        for (int i = q->head; i < q->tail; i++)
        {
            temp = q->buf[i];
            if (strcmp(target->mac, temp->mac) == 0)
                return true;
            
        }
    }
    return false;
}

bool isClose(macAddress *target, queue *q)
{
    if (findMac(target, q))
    {
        if (toc(target->insertionTime) > 2.4 * 0 && toc(target->insertionTime) < 12)
            return true;
    }
    return false;
}

macAddress *createMac(char *inputAddress, macAddress *macAddress)
{
    macAddress->insertionTime = tic();
    macAddress->isClose = false;
    macAddress->mac = inputAddress;
    return macAddress;
}

bool isInFile(char *inputAddress, int *addresses)
{
    char *temp = (char *)malloc(18);
    for (int i = 0; i < *addresses; i++)
    {
        readMac(i, temp);
        if (strcmp(temp, inputAddress) == 0)
        {
            free(temp);
            return true;
        }
    }
    free(temp);
    return false;
}

bool testCOVID()
{
    srand(time(NULL));
    return (rand() & 1);
}

queue *initializeQueue(void)
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

void resetQueue(queue * q){
    q->empty = 1;
    q->full = 0;
    q->head = 0;
    q->tail = 0;
}

void deleteQueue(queue *q)
{
    pthread_mutex_destroy(q->mut);
    free(q->mut);
    pthread_cond_destroy(q->notFull);
    free(q->notFull);
    pthread_cond_destroy(q->notEmpty);
    free(q->notEmpty);
    free(q);
}

void queueAppend(queue *q, macAddress *in)
{
    q->buf[q->tail] = in;
    q->tail++;
    if (q->tail == 1000)
        q->tail = 0;
    if (q->tail == q->head)
        q->full = 1;
    q->empty = 0;

    return;
}

void queueDelete(queue *q, macAddress *out)
{
    *out = *q->buf[q->head];
    q->head++;
    if (q->head == 1000)
        q->head = 0;
    if (q->head == q->tail)
        q->empty = 1;
    q->full = 0;

    return;
}

bool deleteOld(queue *q)
{
    macAddress *temp = q->buf[q->head];
    if (toc(temp->insertionTime) > 12){
        queueDelete(q, temp);
        return true;
    }
    return false;
}

void *loadThread(void *argument)
{
    lstruct *args;
    args = (lstruct *)argument;
    queue *q = args->q;
    queue *close = args->close;
    struct timeval start10 = tic();
    double end10;
    struct timeval timer = tic();
    double time = 0;
    while (1)
    {

        end10 = toc(start10);
        time = toc(timer);
        if (end10 > 0.1)
        {
            saveTimestamp(time);
            start10 = tic();
            char *mac = (char *)malloc(18);
            macAddress *temp = (macAddress *)malloc(sizeof(macAddress));
            while (q->full)
            {
                pthread_cond_wait(q->notFull, q->mut);
            }
            BTNearMe(mac, args->addresses);
            createMac(mac, temp);
            temp->insertionTime = tic();
            temp->isClose = false;
            temp->old = false;
            if (isClose(temp, q))
            {
                if (!findMac(temp, close))
                {
                    queueAppend(close, temp);
                    args->fourteen = tic();
                }
            }
            else{
                queueAppend(q, temp);
                pthread_cond_signal(q->notEmpty);
            }
        }
    }
    return (NULL);
}

void *unloadThread(void *argument)
{
    lstruct *args;
    args = (lstruct *)argument;
    queue *q = args->q;
    queue *close = args->close;
    pid_t tid;
    tid = syscall(SYS_gettid);
    struct timeval start10 = tic();
    double end10;
    struct timeval start4h = tic();
    double end4h;
    double end14d;
    while (1)
    {
        end10 = toc(start10);

        if (end10 >  0.1)
        {
            start10 = tic();
            while (q->empty)
            {
                pthread_cond_wait(q->notEmpty, q->mut);
            }
            if(deleteOld(q))
                pthread_cond_signal(q->notFull);
            end4h = toc(start4h);
            if (end4h > 144)
            {
                start4h = tic();
                end14d = toc(args->fourteen);
                bool test = testCOVID();
                time_t t;
                t = time(NULL);
                struct tm tm;
                tm = *localtime(&t);
                if (test)
                {
                    uploadContacts(close);
                }
                if (end14d >  12096)
                    resetQueue(close);
            }
        }
    }
    return (NULL);
}

void main()
{
    srand(time(NULL));
    int *addresses = (int *)malloc(sizeof(int));
    *addresses= 1000;
    createStarterAddresses(addresses);
    FILE *fp = fopen("Timestamps.bin", "wb");
    struct timeval fourteen;
    double end14d;
    queue *q, *close;
    q = initializeQueue();
    close = initializeQueue();
    if (close == NULL || q == NULL)
        exit(1);
    lstruct strct;
    strct.q = q;
    strct.close = close;
    strct.addresses = addresses;
    strct.fourteen = fourteen;
    pthread_t lthread, uthread;
    pthread_create(&lthread, NULL, loadThread, &strct);
    pthread_create(&uthread, NULL, unloadThread, &strct);
    pthread_join(lthread, NULL);
    pthread_join(uthread, NULL);
    deleteQueue(q);
    deleteQueue(close);
}