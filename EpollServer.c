#define _XOPEN_SOURCE
#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <signal.h>
#define MAX_LINE 500
#define WORKER_NUM 4

typedef unsigned char bool;
static const bool False = 0;
static const bool True = 1;

struct thread_pool;
struct work;
typedef struct thread_pool tpool_t;
typedef struct work work_t;
typedef void (*work_func)(void *);
typedef void* (*worker_func)(void *);

struct thread_pool{
    work_t *first;
    work_t *last;
    pthread_mutex_t  work_mutex;
    pthread_cond_t   work_cond;
    pthread_cond_t   working_cond;
    size_t           working_cnt;
    size_t           thread_cnt;
    bool             stop;
};

struct work{
    work_t* next;
    void* args;
    work_func func;
};


typedef struct{
    int fd;
    char filename[MAX_LINE];
}Params;

tpool_t *tpool_create(worker_func work);
void tpool_destory(tpool_t *tp);
bool tpool_add_work(tpool_t *tp, work_func func, void *arg);
void thread_wait(tpool_t* tp);
tpool_t *tp;

static work_t* work_create(work_func func, void* args){
    work_t *wk = (work_t*)malloc(sizeof(work_t));
    wk->func = func;
    wk->args = args;
    wk->next = NULL;
    return wk;
}

static void tpool_work_destroy(work_t *wk)
{
    if (wk == NULL)
        return;
    free(wk);
}

static work_t* work_get(tpool_t* tp){
    if(tp->first!=NULL){
        work_t *t = tp->first;
        tp->first = tp->first->next;
        if(tp->first == NULL){
            tp->last = NULL;
        }
        return t;
    }
    return NULL;
}

bool tpool_add_work(tpool_t *tp, work_func func, void *arg){
    work_t *wk = work_create(func, arg);
    if(tp->working_cnt == WORKER_NUM){
        printf("Thread pool full\n");
        return False;
    }else{
        if(tp->first == NULL){
            tp->first = wk;
            tp->last = wk;
        }else{
            tp->last->next = wk;
            tp->last = wk;
        }
        return True;
    }
}
void* worker(void *arg){
    printf("Thread %ld create\n", pthread_self());
    tpool_t *tp = arg;
    work_t *wk;
    while(1){
        pthread_mutex_lock(&(tp->work_mutex));
        printf("Thread %ld controling\n", pthread_self());
        if(tp->stop){
            printf("Thread %ld exit\n", pthread_self());
            break;
        }
        if(tp->first == NULL){
            printf("Thread %ld waiting for source\n", pthread_self());
            pthread_cond_wait(&(tp->work_cond), &(tp->work_mutex));
            printf("Thread %ld get source\n", pthread_self());
        }
        wk = work_get(tp);
        tp->working_cnt++;
        pthread_mutex_unlock(&(tp->work_mutex));

        if(wk){
            wk->func(wk->args);
        }
        pthread_mutex_lock(&(tp->work_mutex));
        tpool_work_destroy(wk);
        tp->working_cnt--;
        if (!tp->stop && tp->working_cnt == 0 && tp->first == NULL)
            pthread_cond_signal(&(tp->working_cond));
        pthread_mutex_unlock(&(tp->work_mutex));
    }
    tp->thread_cnt--;
    pthread_cond_signal(&(tp->working_cond));
    pthread_mutex_unlock(&(tp->work_mutex));
    return NULL;
}

void tpool_wait(tpool_t *tp)
{
    if (tp == NULL)
        return;

    pthread_mutex_lock(&(tp->work_mutex));
    while (1) {
        if ((!tp->stop && tp->working_cnt != 0) || (tp->stop && tp->thread_cnt != 0)) {
            printf("working_cnt = %ld, tp->stop = %d waiting threads to exit\n", tp->working_cnt, tp->stop);
            pthread_cond_wait(&(tp->working_cond), &(tp->work_mutex));
        } else {
            break;
        }
    }
    pthread_mutex_unlock(&(tp->work_mutex));
}

tpool_t *tpool_create(worker_func work){
    pthread_t tid;
    tpool_t *tp = (tpool_t*)malloc(sizeof(tpool_t));
    tp->first = NULL;
    tp->last = NULL;
    tp->thread_cnt = WORKER_NUM;
    tp->working_cnt = 0;
    pthread_mutex_init(&(tp->work_mutex), NULL);
    pthread_cond_init(&(tp->work_cond), NULL);
    pthread_cond_init(&(tp->working_cond), NULL);
    for(int i=0;i<tp->thread_cnt;i++){
        pthread_create(&tid, NULL, worker, tp);
        pthread_detach(tid);
    }
    return tp;
}
void tpool_destory(tpool_t *tp){
    printf("Begin to destory thread pool\n");
    work_t *work;
    work_t *work2;

    if (tp == NULL)
        return;

    pthread_mutex_lock(&(tp->work_mutex));
    work = tp->first;
    while (work != NULL) {
        work2 = work->next;
        tpool_work_destroy(work);
        work = work2;
    }
    tp->stop = True;
    pthread_cond_broadcast(&(tp->work_cond));
    pthread_mutex_unlock(&(tp->work_mutex));

    tpool_wait(tp);

    pthread_mutex_destroy(&(tp->work_mutex));
    pthread_cond_destroy(&(tp->work_cond));
    pthread_cond_destroy(&(tp->working_cond));

    free(tp);
    printf("Destory thread pool success\n");
}

void err_sys(const char * err_msg){
    perror(err_msg);
    exit(1);
}

void file_transfer(void* params){
    pthread_t tid = pthread_self();
    printf("Thread %ld begin\n", tid);
    Params *p = (Params*) params;
    FILE *fp;
    printf("%s\n", p->filename);
    fp = fopen(p->filename, "rb");
    unsigned char buffer[MAX_LINE];
    printf("\n");
    while(1){
        memset(buffer, '\0', sizeof(buffer));
        fread(buffer, 1, MAX_LINE-1,fp);
        printf("%s", buffer);
        write(p->fd, buffer, strlen(buffer));
        if(feof(fp)){
            break;
        }
    }
    close(p->fd);
    fclose(fp);
}

static void tpool_handle(int sig){
    printf("Recv SIGINT signal and destory the thread pool.\n");
    tpool_destory(tp);
    printf("Server exit\n");
    exit(0);
}

int main(int argc, char** argv){
    int listenfd, connfd;
    struct sockaddr_in server_addr;
    struct epoll_event events[10];
    struct sigaction action;
    action.sa_handler = tpool_handle;
    action.sa_flags = 0;
    if(argc!=2)
        err_sys("usage a.out ${Port}");
    
    if((listenfd = socket(AF_INET, SOCK_STREAM, 0))<0)
        err_sys("socket error");
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[1]));
    server_addr.sin_addr.s_addr = htonl(0);
    
    bind(listenfd, (const struct sockaddr *)&server_addr, sizeof(server_addr));
    listen(listenfd, 10);

    int epfd = epoll_create(1024);
    struct epoll_event ev;
    ev.data.fd = listenfd;
    ev.events = EPOLLIN|EPOLLET;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);
    tp = tpool_create(worker);
    printf("Server begin to listen %d port\n", atoi(argv[1]));
    if(sigaction(SIGINT, &action, NULL)<0){
        perror("Signal bind failed.\n");
        exit(1);
    }
    for(;tp;){
        int fds = epoll_wait(epfd, events, 10, -1);
        for(int i =0;i< fds; i++){
            if(events[i].data.fd == listenfd){
                connfd = accept(listenfd, (struct sockaddr *)NULL, NULL);
                struct epoll_event ev;
                ev.data.fd = connfd;
                ev.events = EPOLLIN | EPOLLET;
                epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev);
            }else{
                Params *p = (Params*)malloc(sizeof(Params));
                p->fd = events[i].data.fd; 
                int n = read(p->fd, p->filename, MAX_LINE);
                if(strcmp(p->filename, "EOF") == 0){
                    tpool_destory(tp);
                    tp = NULL;
                    break;
                }
                if(access(p->filename, F_OK)!=-1){
                    tpool_add_work(tp, file_transfer, p);
                    printf("working fd %d\n", p->fd);
                    pthread_cond_signal(&(tp->work_cond));
                }else{
                    close(p->fd);
                }
            }
        }
        
    }
    exit(0);
}