#include "foothread.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct node {
    int id;
    int parent_id;
    int num_children;
    int sum;
    foothread_mutex_t sum_mutex;
} node_t;

int n; // Number of nodes
node_t* nodes;

void* node_function(void* arg) {
    node_t* node = (node_t*)arg;

    if (node->num_children == 0) { // Leaf node
        int input;
        printf("Leaf node %d :: Enter a positive integer: ", node->id);
        scanf("%d", &input);

        foothread_mutex_lock(&nodes[node->parent_id].sum_mutex);
        nodes[node->parent_id].sum += input;
        printf("Internal node %d gets the partial sum %d from its children\n", node->parent_id, input);
        foothread_mutex_unlock(&nodes[node->parent_id].sum_mutex);
    }

    foothread_barrier_wait(&barrier);
    
    // Call foothread_exit() for synchronization at the end
    foothread_exit();
    return NULL;
}

int main(int argc, char* argv[]) {
    FILE* file = fopen("tree.txt", "r");
    if (!file) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    fscanf(file, "%d", &n);

    nodes = (node_t*)malloc(n * sizeof(node_t));
    int* children_counts = (int*)calloc(n, sizeof(int)); // Temporary array to count children

    for (int i = 0; i < n; i++) {
        fscanf(file, "%d %d", &nodes[i].id, &nodes[i].parent_id);
        nodes[i].sum = 0;
        foothread_mutex_init(&nodes[i].sum_mutex);
        if (i != nodes[i].parent_id) { // Avoid counting the root node as its own child
            children_counts[nodes[i].parent_id]++;
        }
    }

    // Initialize num_children for each node
    for (int i = 0; i < n; i++) {
        nodes[i].num_children = children_counts[i];
    }

    free(children_counts);
    fclose(file);

    foothread_barrier_t barrier;
    foothread_barrier_init(&barrier, n);

    foothread_t threads[n];
    for (int i = 0; i < n; i++) {
        foothread_create(&threads[i], NULL, node_function, (void*)&nodes[i]);
    }

    // Wait for all threads to finish
    for (int i = 0; i < n; i++) {
        foothread_join(threads[i], NULL);
    }

    printf("Sum at root (node %d) = %d\n", nodes[0].id, nodes[0].sum);

    // Clean up
    foothread_barrier_destroy(&barrier);
    for (int i = 0; i < n; i++) {
        foothread_mutex_destroy(&nodes[i].sum_mutex);
    }
    free(nodes);

    return 0;
}
