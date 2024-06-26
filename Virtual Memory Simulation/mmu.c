#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <fcntl.h>

#define P(s) semop(s, &pop, 1)
#define V(s) semop(s, &vop, 1)

typedef struct SM1
{
    int pid;         // process id
    int mi;          // number of required pages
    int fi;          // number of frames allocated
    int pagetable[10000][3]; // page table
    int totalpagefaults;
    int totalillegalaccess;
} SM1;
typedef struct message2
{
    long type;
    int pid;
} message2;

typedef struct message3
{
    long type;
    int pageorframe;
    int pid;
} message3;



int main(int argc, char *argv[])
{
    int timestamp = 0;

    struct sembuf pop = {0, -1, 0};
    struct sembuf vop = {0, 1, 0};

    if (argc != 5)
    {
        printf("Usage: %s <Message Queue 2 ID> <Message Queue 3 ID> <Shared Memory 1 ID> <Shared Memory 2 ID>\n", argv[0]);
        exit(1);
    }

    int msgid2 = atoi(argv[1]);
    int msgid3 = atoi(argv[2]);
    int shmid1 = atoi(argv[3]);
    int shmid2 = atoi(argv[4]);

    // Print the values
    // printf("msgid2: %d\n", msgid2);
    // printf("msgid3: %d\n", msgid3);
    // printf("shmid1: %d\n", shmid1);
    // printf("shmid2: %d\n", shmid2);

    char *outputFile = "result.txt";

    // Open output file
    int fd = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd == -1)
    {
        perror("Failed to open output file");
        exit(EXIT_FAILURE);
    }

    message2 msg2;
    message3 msg3;

    SM1 *sm1 = (SM1 *)shmat(shmid1, NULL, 0);
    int *sm2 = (int *)shmat(shmid2, NULL, 0);

    // semaphore 3 for Memory Management Unit
    key_t key = ftok(".", 'F');
    int semid3 = semget(key, 1, IPC_CREAT | 0666);

    while (1)
    {
        // wait for process to come
        msgrcv(msgid3, (void *)&msg3, sizeof(message3), 0, 0);
        timestamp++;
        
        V(semid3);

        printf("Global Ordering - (Timestamp %d, Process %d, Page %d)\n", timestamp, msg3.pid, msg3.pageorframe);
        dprintf(fd, "Global ordering - (Timestamp %d, Process %d, Page %d)\n", timestamp, msg3.pid, msg3.pageorframe);
        
        // check if the requested page is in the page table of the process with that pid
        int i = 0;
        while (sm1[i].pid != msg3.pid)
        {
            i++;
        }
        // printf("i: %d\n",i);

        int page = msg3.pageorframe;
        // printf("page: %d\n", page);
        // Iterate over each page in the page table
        // for (int j = 0; j < sm1[i].mi; j++) {
        //     printf("Page %d: Frame: %d, Valid: %d, Timestamp: %d\n", j, sm1[i].pagetable[j][0], sm1[i].pagetable[j][1], sm1[i].pagetable[j][2]);
        // }

        // printf("%d %d\n", sm1[i].pagetable[page][0], sm1[i].pagetable[page][1]);

        
        if (page == -9)
        {
            // printf("sent11\n");
            // fflush(stdout);
            // process is done
            // free the frames
            for (int j = 0; j < sm1[i].mi; j++)
            {
                if (sm1[i].pagetable[j][0] != -1)
                {
                    sm2[sm1[i].pagetable[j][0]] = 1;
                    sm1[i].pagetable[j][0] = -1;
                    sm1[i].pagetable[j][1] = 0;
                    sm1[i].pagetable[j][2] = INT_MAX;
                }
            }
            msg2.type = 2;
            msg2.pid = msg3.pid;
            msgsnd(msgid2, (void *)&msg2, sizeof(message2), 0);
            // printf("sent1\n");
        }
        else if (sm1[i].pagetable[page][0] != -1 && sm1[i].pagetable[page][1] == 1)
        {
            // printf("sent21\n");
            // fflush(stdout);
            // page there in memory and valid, return frame number
            sm1[i].pagetable[page][2] = timestamp;
            msg3.pageorframe = sm1[i].pagetable[page][0];
            sm2[sm1[i].pagetable[page][0]] = 0; //
            msgsnd(msgid3, (void *)&msg3, sizeof(message3), 0);
            // printf("sent2\n");
        }
        else if (page >= sm1[i].mi)
        {
            // printf("sent31\n");
            // fflush(stdout);
            // illegal page number
            // ask process to kill themselves
            msg3.pageorframe = -2;
            msgsnd(msgid3, (void *)&msg3, sizeof(message3), 0);

            // update total illegal access
            sm1[i].totalillegalaccess++;

            printf("Invalid Page Reference - (Process %d, Page %d)\n", i + 1, page);
            dprintf(fd, "Invalid Page Reference - (Process %d, Page %d)\n", i + 1, page);

            // free the frames
            for (int j = 0; j < sm1[i].mi; j++)
            {
                if (sm1[i].pagetable[j][0] != -1)
                {
                    sm2[sm1[i].pagetable[j][0]] = 1;
                    sm1[i].pagetable[j][0] = -1;
                    sm1[i].pagetable[j][1] = 0;
                    sm1[i].pagetable[j][2] = INT_MAX;
                }
            }
            msg2.type = 2;
            msg2.pid = msg3.pid;
            msgsnd(msgid2, (void *)&msg2, sizeof(message2), 0);
            // printf("sent3\n");
        }
        else
        {
            // printf("sent41\n");
            // fflush(stdout);
            // page fault
            // ask process to wait
            msg3.pageorframe = -1;
            msgsnd(msgid3, (void *)&msg3, sizeof(message3), 0);


            printf("Page fault sequence - (Process %d, Page %d)\n", i + 1, page);
            dprintf(fd, "Page fault sequence - (Process %d, Page %d)\n", i + 1, page);

            // Page Fault Handler (PFH)
            // check if there is a free frame in sm2
            int j = 0;
            while (sm2[j] != -1)
            {
                if (sm2[j] == 1)
                {
                    sm2[j] = 0;
                    break;
                }
                j++;
            }

            if (sm2[j] == -1)
            {
                // no free frame
                // find the page with the least timestamp
                int min = INT_MAX;
                int minpage = -1;
                for (int k = 0; k < sm1[i].mi; k++)
                {
                    if (sm1[i].pagetable[k][2] < min)
                    {
                        min = sm1[i].pagetable[k][2];
                        minpage = k;
                    }
                }

                
                sm1[i].pagetable[minpage][1] = 0;
                sm1[i].pagetable[page][0] = sm1[i].pagetable[minpage][0];
                sm1[i].pagetable[page][1] = 1;
                sm1[i].pagetable[page][2] = timestamp;
                sm1[i].pagetable[minpage][2] = INT_MAX;
                
                sm2[sm1[i].pagetable[page][0]] = 0; //

                msg2.type = 1;
                msg2.pid = msg3.pid;
                msgsnd(msgid2, (void *)&msg2, sizeof(message2), 0);
            }

            else
            {
                // free frame found
                sm1[i].pagetable[page][0] = j;
                sm1[i].pagetable[page][1] = 1;
                sm1[i].pagetable[page][2] = timestamp;

                sm2[sm1[i].pagetable[page][0]] = 0; //

                msg2.type = 1;
                msg2.pid = msg3.pid;
                msgsnd(msgid2, (void *)&msg2, sizeof(message2), 0);
            }
            // printf("sent4\n");
        }

    }
    
}