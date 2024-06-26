#include "foothread.h"

void foothread_attr_setjointype(foothread_attr_t *attr, int i)
{
    attr->join_type = FOOTHREAD_JOINABLE;
    nojthreads++;
}
void foothread_attr_setstacksize(foothread_attr_t *attr, int i)
{
    attr->stack_size = i;
}

void foothread_create(foothread_t *thread, foothread_attr_t *attr, int (*start_routine)(void *), void *arg)
{
    size_t stack_size = FOOTHREAD_DEFAULT_STACK_SIZE;
    if (attr != NULL)
    {
        if (attr->stack_size > 0)
            stack_size = attr->stack_size;
    }

    void *stack = malloc(stack_size);
    if (stack == NULL)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    if (semmade == 0)
    {
        // Initialize the semaphore
        if (sem_init(&semid, 0, 0) == -1)
        {
            perror("sem_init");
            exit(EXIT_FAILURE);
        }

        semmade = 1;
    }

    if (attr == NULL)
    {
        attr->join_type = FOOTHREAD_DETACHED;
        attr->stack_size = FOOTHREAD_DEFAULT_STACK_SIZE;
    }

    pid_t tid = clone(start_routine, stack + stack_size, CLONE_VM | SIGCHLD, arg);
    if (tid == -1)
    {
        perror("clone");
        exit(EXIT_FAILURE);
    }

    if (thread != NULL)
    {
        // Store thread data
        // ...
        nojthreads++;
    }
}

void foothread_exit()
{
    // if leader then
    // while(joinablethds){
    //     p();
    //     joinablethds--;
    // }
    if (gettid() == getpid())
    {
        while (nojthreads > 0)
        {
            /* code */
            sem_wait(&semid);
            nojthreads--;
        }
    }
    else
    {
        sem_post(&semid);
    }
    exit(0);
}

static int get_thread_id()
{
    return syscall(SYS_gettid);
}

void foothread_mutex_init(foothread_mutex_t *mutex)
{
    sem_init(&mutex->lock, 0, 1);      // Initially unlocked
    sem_init(&mutex->wait_lock, 0, 1); // Controls access
    mutex->is_locked = 0;
    mutex->lock_count = 0;
}

void foothread_mutex_lock(foothread_mutex_t *mutex)
{
    sem_wait(&mutex->wait_lock); // Ensure exclusive access to lock
    if (mutex->lock_count == 0)
    {
        sem_wait(&mutex->lock); // Lock the mutex if it's the first lock call
    }
    mutex->lock_count++; // Increment lock count
    mutex->is_locked = 1;
    sem_post(&mutex->wait_lock); // Release exclusive access
}

void foothread_mutex_unlock(foothread_mutex_t *mutex)
{
    sem_wait(&mutex->wait_lock); // Ensure exclusive access to unlock
    mutex->lock_count--;         // Decrement lock count
    if (mutex->lock_count == 0)
    {
        mutex->is_locked = 0;
        sem_post(&mutex->lock); // Unlock the mutex if it's the last unlock call
    }
    sem_post(&mutex->wait_lock); // Release exclusive access
}

void foothread_mutex_destroy(foothread_mutex_t *mutex)
{
    sem_destroy(&mutex->lock);
    sem_destroy(&mutex->wait_lock);
    // Note: Make sure the mutex is fully unlocked and not in use before destroying
}

void foothread_barrier_init(foothread_barrier_t *barrier, int count)
{
    barrier->count = count;
    barrier->max = count;
    sem_init(&barrier->barrier_sem, 0, 0); // Initially locked, to be unlocked when count reaches 0
    sem_init(&barrier->wait_sem, 0, 1);    // Controls access to the count variable
}

void foothread_barrier_wait(foothread_barrier_t *barrier)
{
    sem_wait(&barrier->wait_sem); // Lock to decrement count safely
    if (--barrier->count == 0)
    {
        // Last thread to reach the barrier, unlock barrier_sem count times to release all threads
        for (int i = 0; i < barrier->max; ++i)
        {
            sem_post(&barrier->barrier_sem);
        }
    }
    sem_post(&barrier->wait_sem);    // Release lock on count
    sem_wait(&barrier->barrier_sem); // Wait until barrier_sem is unlocked
}

void foothread_barrier_destroy(foothread_barrier_t *barrier)
{
    sem_destroy(&barrier->barrier_sem);
    sem_destroy(&barrier->wait_sem);
}