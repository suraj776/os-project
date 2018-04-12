
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

#define N_BOWLS 2          
int N_CATS;           
int N_MICE;            

int CAT_WAIT;             
int CAT_EAT; 
int CAT_N_EAT;     
int MOUSE_WAIT;           
int MOUSE_EAT;         
int MOUSE_N_EAT; 

void input()
{
	printf("Enter the number of cats = \n");
	scanf("%d",&N_CATS);
	printf("Enter the number of mice  = \n");
	scanf("%d",&N_MICE);
	printf("Enter the max time in seconds a cat sleeps = \n");
	scanf("%d",&CAT_WAIT);
	printf("Enter how long in seconds a cat is eating = \n");
	scanf("%d",&CAT_EAT);
	printf("Enter how many times a cat wants to eat = \n");
	scanf("%d",&CAT_N_EAT);
	printf("Enter max time in seconds a mouse sleeps = \n");
	scanf("%d",&MOUSE_WAIT);
	printf("Enter how long in seconds a mouse is eating= \n");
	scanf("%d",&MOUSE_EAT);
	printf("Enter how many times a mouse wants to eat= \n");
	scanf("%d",&MOUSE_N_EAT);
}
typedef struct bowl {
    int free_bowles;    
    int cats_eating;      
    int mice_eating;            
    int cats_waiting;           
    enum {
        none_eating,
        cat_eating,
        mouse_eating
    } status[N_BOWLS];         
    pthread_mutex_t mutex;     
    pthread_cond_t free_cv;
    pthread_cond_t cat_cv;      
} bowl_t;

static void
dump_bowl(const char *name, pthread_t pet, const char *what,
          bowl_t *bowl, int my_bowl)
{
    int i;
    for (i = 0; i < N_BOWLS; i++) {
        if (i) printf(":");
        switch (bowl->status[i]) {
        case none_eating:
            printf("-");
            break;
        case cat_eating:
            printf("c");
            break;
        case mouse_eating:
            printf("m");
            break;
        }
    }
    printf("] %s (id %x) %s eating from bowl %d\n", name, pet, what, my_bowl);
}

static void* 
cat(void *arg)
{
    bowl_t *bowl = (bowl_t *) arg;
    int n = CAT_N_EAT;
    int my_bowl = -1;
    int i;

    for (n = CAT_N_EAT; n > 0; n--) {

        pthread_mutex_lock(&bowl->mutex);
        pthread_cond_broadcast(&bowl->cat_cv);
        bowl->cats_waiting++;
        while (bowl->free_bowles <= 0 || bowl->mice_eating > 0) {
            pthread_cond_wait(&bowl->free_cv, &bowl->mutex);
        }
        bowl->cats_waiting--;

        assert(bowl->free_bowles > 0);
        bowl->free_bowles--;
        assert(bowl->cats_eating < N_CATS);
        bowl->cats_eating++;
        
        for (i = 0; i < N_BOWLS && bowl->status[i] != none_eating; i++) ;
        my_bowl = i;
        assert(bowl->status[my_bowl] == none_eating);
        bowl->status[my_bowl] = cat_eating;
        dump_bowl("cat", pthread_self(), "started", bowl, my_bowl);
        pthread_mutex_unlock(&bowl->mutex);
        sleep(CAT_EAT);
        
        pthread_mutex_lock(&bowl->mutex);
        assert(bowl->free_bowles < N_BOWLS);
        bowl->free_bowles++;
        assert(bowl->cats_eating > 0);
        bowl->cats_eating--;
        bowl->status[my_bowl] = none_eating;

        pthread_cond_broadcast(&bowl->free_cv);
        dump_bowl("cat", pthread_self(), "finished", bowl, my_bowl);
        pthread_mutex_unlock(&bowl->mutex);

        sleep(rand() % CAT_WAIT);
    }

    return NULL;
}

static void* 
mouse(void *arg)
{
    bowl_t *bowl = (bowl_t *) arg;
    int n = MOUSE_N_EAT;
    struct timespec ts;
    struct timeval tp;
    int my_bowl;
    int i;

    for (n = MOUSE_N_EAT; n > 0; n--) {

        pthread_mutex_lock(&bowl->mutex);
        while (bowl->free_bowles <= 0 || bowl->cats_eating > 0
               || bowl->cats_waiting > 0) {
            pthread_cond_wait(&bowl->free_cv, &bowl->mutex);
        }

     
        assert(bowl->free_bowles > 0);
        bowl->free_bowles--;
        assert(bowl->cats_eating == 0);
        assert(bowl->mice_eating < N_MICE);
        bowl->mice_eating++;

        for (i = 0; i < N_BOWLS && bowl->status[i] != none_eating; i++) ;
        my_bowl = i;
        assert(bowl->status[my_bowl] == none_eating);
        bowl->status[my_bowl] = mouse_eating;
        dump_bowl("mouse", pthread_self(), "started", bowl, my_bowl);
        pthread_mutex_unlock(&bowl->mutex);
        
        gettimeofday(&tp,NULL);
        ts.tv_sec  = tp.tv_sec;
        ts.tv_nsec = tp.tv_usec * 1000;
        ts.tv_sec += MOUSE_EAT;
        pthread_mutex_lock(&bowl->mutex);
        pthread_cond_timedwait(&bowl->cat_cv, &bowl->mutex, &ts);
        pthread_mutex_unlock(&bowl->mutex);
        
        pthread_mutex_lock(&bowl->mutex);
        assert(bowl->free_bowles < N_BOWLS);
        bowl->free_bowles++;
        assert(bowl->cats_eating == 0);
        assert(bowl->mice_eating > 0);
        bowl->mice_eating--;
        bowl->status[my_bowl]=none_eating;

        pthread_cond_broadcast(&bowl->free_cv);
        dump_bowl("mouse", pthread_self(), "finished", bowl, my_bowl);
        pthread_mutex_unlock(&bowl->mutex);

        sleep(rand() % MOUSE_WAIT);
    }

    return NULL;
}

int
main(int argc, char *argv[])
{
	input();
    int i, err;
    bowl_t _bowl, *bowl;
    pthread_t cats[N_CATS];
    pthread_t mice[N_MICE];

    srand(time(NULL));

    bowl = &_bowl;
    memset(bowl, 0, sizeof(bowl_t));
    bowl->free_bowles = N_BOWLS;
    pthread_mutex_init(&bowl->mutex, NULL);
    pthread_cond_init(&bowl->free_cv, NULL);
    pthread_cond_init(&bowl->cat_cv, NULL);
    
    for (i = 0; i < N_CATS; i++) {
    pthread_create(&cats[i], NULL, cat, bowl);
    
    }

    for (i = 0; i < N_MICE; i++) {
    pthread_create(&mice[i], NULL, mouse, bowl);
     
    }

    for (i = 0; i < N_CATS; i++) {
        (void) pthread_join(cats[i], NULL);
    }
    for (i = 0; i < N_MICE; i++) {
        (void) pthread_join(mice[i], NULL);
    }
    
    pthread_mutex_destroy(&bowl->mutex);
    pthread_cond_destroy(&bowl->free_cv);
    pthread_cond_destroy(&bowl->cat_cv);
    
    return 0;
}
