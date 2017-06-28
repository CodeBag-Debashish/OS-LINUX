// sem_utility.h
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <sys/shm.h>
union semun {
    int val;                    
    struct semid_ds *buf;       
    unsigned short int *array;  
    struct seminfo *__buf;      
};

static int set_semvalue(int,int);
static void del_semvalue(void);
static int semaphore_p(int);
static int semaphore_v(int);
static int sem_id;

static int set_semvalue(int sem_no,int ini_val) {
    union semun sem_union;
    sem_union.val = ini_val;
    if (semctl(sem_id, sem_no, SETVAL, sem_union) == -1) return(0);
    return(1);
}
static void del_semvalue(void) {
    union semun sem_union;
    if (semctl(sem_id, 0, IPC_RMID, sem_union) == -1)
        fprintf(stderr, "Failed to delete semaphore\n");
}
static int semaphore_p(int sem_no) {
    struct sembuf sem_b;
    sem_b.sem_num = sem_no;
    sem_b.sem_op = -1;
    sem_b.sem_flg = SEM_UNDO;
    if(semop(sem_id, &sem_b, 1) == -1) {
        fprintf(stderr, "semaphore_p failed\n");
        return(0);
    }
    //printf("%s\n", );
    //printf("dropped\n");
    return(1);
}
static int semaphore_v(int sem_no) {
    struct sembuf sem_b;
    sem_b.sem_num = sem_no;
    sem_b.sem_op = 1;
    sem_b.sem_flg = SEM_UNDO;
    if (semop(sem_id, &sem_b, 1) == -1) {
        fprintf(stderr, "semaphore_v failed\n");
        return(0);
    }
    return(1);
}


/*...............................................................*/

// resources.h
#define SIZE 1024
#define BUFSIZE     10
#define N_SEM       3
#define MUTEX       0
#define FULL        1
#define EMPTY       2
struct shared_mem {
	int buffer[SIZE];
	int in,out;
};


/*...............................................................*/
//prog.c
#include "sem_utility.h"
#include "resources.h"
int sem_ini_val[N_SEM] = {1,0,BUFSIZE};
int main(int argc, char *argv[]) {
    int sem_number;
    char * cc[] = {NULL};
    srand(time(NULL));
    void *shared_memory = (void *)0;
    struct shared_mem *ptr;
    int shm_id;
    shm_id = shmget((key_t)1111,sizeof(struct shared_mem),0666|IPC_CREAT);
    if(shm_id == -1) {
        fprintf(stderr, "shmget failed\n");
        exit(EXIT_FAILURE);
    }
    shared_memory = shmat(shm_id,(void*)0,0);
    if(shared_memory == (void*)-1) {
        fprintf(stderr, "shmat failed\n");
        exit(EXIT_FAILURE);
    }
    ptr = (struct shared_mem *)shared_memory;
    ptr->in = 0;
    ptr->out = 0;
    sem_id = semget((key_t)3333, N_SEM, 0666 | IPC_CREAT);
    for(sem_number=0;sem_number<N_SEM;sem_number++)
        set_semvalue(sem_number,sem_ini_val[sem_number]);
    pid_t producer_pid, consumer_pid;
    consumer_pid = producer_pid = getpid();
    consumer_pid = fork();
    if(consumer_pid > 0) producer_pid = fork();
    if(producer_pid == 0) { 
        execvp("producer",cc);
    }
    if(consumer_pid == 0) execvp("consumer",cc);
    if(consumer_pid > 0 && producer_pid > 0) {
        int res1,res2;
        waitpid(consumer_pid,&res1,0);
        waitpid(producer_pid,&res2,0);
    }
    sleep(2);
    del_semvalue();
    if(shmdt(shared_memory) == -1) {
        fprintf(stderr, "shmdt failed\n");
        exit(EXIT_FAILURE);
    }
    if(shmctl(shm_id,IPC_RMID,0) == -1) {
        fprintf(stderr, "shmctl failed\n");
        exit(EXIT_FAILURE);
    }
    printf("main programm exited\n");
    exit(EXIT_SUCCESS);
}


/*...............................................................*/

// cons.c
#include "sem_utility.h"
#include "resources.h"
int main(int argc, char *argv[]) {
    printf("I am consumer\n");
    srand(time(NULL));
    void *shared_memory = (void *)0;
    struct shared_mem *ptr;
    int shm_id;
    shm_id = shmget((key_t)1111,sizeof(struct shared_mem),0666|IPC_CREAT);
    if(shm_id == -1) {
        fprintf(stderr, "shmget failed\n");
        exit(EXIT_FAILURE);
    }
    shared_memory = shmat(shm_id,(void*)0,0);
    if(shared_memory == (void*)-1) {
        fprintf(stderr, "shmat failed\n");
        exit(EXIT_FAILURE);
    }
    sem_id = semget((key_t)3333, 1, 0666 | IPC_CREAT);
    ptr = (struct shared_mem *)shared_memory;
    while(1) {
        int pause_time = rand()%3;
        sleep(pause_time);

        semaphore_p(FULL);
        semaphore_p(MUTEX);

        int data = ptr->buffer[ptr->out];
        ptr->out = (ptr->out + 1) % BUFSIZE;
        printf("Consumed: %d    out = %d\n", data,ptr->out);

        semaphore_v(MUTEX);
        semaphore_v(EMPTY);
    }
    if(shmdt(shared_memory) == -1) {
        fprintf(stderr, "shmdt failed\n");
        exit(EXIT_FAILURE);
    } 
    printf("consumer exited\n");
    exit(EXIT_SUCCESS); 
}


/*...............................................................*/

// prod.c
#include "sem_utility.h"
#include "resources.h"
int main(int argc, char *argv[]) {
    printf("I am producer\n");
    srand(time(NULL));
    void *shared_memory = (void *)0;
    struct shared_mem *ptr;
    int shm_id;
    shm_id = shmget((key_t)1111,sizeof(struct shared_mem),0666|IPC_CREAT);
    if(shm_id == -1) {
        fprintf(stderr, "shmget failed\n");
        exit(EXIT_FAILURE);
    }
    shared_memory = shmat(shm_id,(void*)0,0);
    if(shared_memory == (void*)-1) {
        fprintf(stderr, "shmat failed\n");
        exit(EXIT_FAILURE);
    }
    sem_id = semget((key_t)3333, 1, 0666 | IPC_CREAT);
    ptr = (struct shared_mem *)shared_memory;
    int i = 20;
    while(1) {
        int pause_time = rand()%3;
        sleep(pause_time);
        
        semaphore_p(EMPTY);
        semaphore_p(MUTEX);
        
        int data = rand()%1000;
        ptr->buffer[ptr->in] = data;
        ptr->in = (ptr->in + 1) % BUFSIZE;
        printf("Produced: %d    in = %d\n", data,ptr->in);
        

        semaphore_v(MUTEX);
        semaphore_v(FULL);

    }
    if(shmdt(shared_memory) == -1) {
        fprintf(stderr, "shmdt failed\n");
        exit(EXIT_FAILURE);
    }
    printf("producer exited\n");
    exit(EXIT_SUCCESS);
}
