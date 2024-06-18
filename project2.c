#include <stdio.h>
#include <pthread.h>
#include <dispatch/dispatch.h> 
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

#define MAX_CAPACITY 15
#define MAX_DOCTORS 3
#define MAX_QUEUE_SIZE 100

// Define the structure of the queue
typedef struct {
    int data[MAX_QUEUE_SIZE]; // Array to store elements of the queue
    int front; // Index of the front element of the queue
    int rear; // Index of the rear element of the queue
} Queue;

// Initialize the queue
void initQueue(Queue *q) {
    q->front = -1; // Indicates the queue is empty
    q->rear = -1; // Indicates the queue is empty
}

// Check if the queue is empty
int isEmpty(Queue *q) {
    return (q->front == -1 && q->rear == -1);
}

// Check if the queue is full
int isFull(Queue *q) {
    return (q->rear == MAX_QUEUE_SIZE - 1);
}

// Add an element to the queue (enqueue operation)
void enqueue(Queue *q, int item) {
    if (isFull(q)) {
        printf("Queue is full. Cannot enqueue.\n");
        return;
    }
    if (isEmpty(q)) {
        q->front = 0; // Initialize front and rear if the queue is empty
    }
    q->rear++; // Move rear one position back
    q->data[q->rear] = item; // Store the data at the rear position
}

// Remove and return an element from the queue (dequeue operation)
int dequeue(Queue *q, int *item) {
    if (isEmpty(q)) {
        printf("Queue is empty. Cannot dequeue.\n");
        return -1;
    }
    *item = q->data[q->front];
    if (q->front == q->rear) {
        q->front = -1;
        q->rear = -1;
    } else {
        q->front++;
    }
    return 0;
}

// Global variables
int patient_count = -1;
int doctor_count;
int total_patients;
Queue queue1, queue2, queue3, queue4;
Queue doctor_queues[MAX_DOCTORS];

// Dispatch semaphores
dispatch_semaphore_t mutex1, mutex2, mutex3, mutex4, mutex5, receptionist_ready, receptionist_call, nurse_call, patient_exit;
dispatch_semaphore_t patient_waitingroom[MAX_CAPACITY], patient_office[MAX_CAPACITY];
dispatch_semaphore_t receptionist_register[MAX_CAPACITY], office[MAX_DOCTORS];
dispatch_semaphore_t nurse_ready[MAX_DOCTORS], nurse_directs[MAX_CAPACITY];
dispatch_semaphore_t doctor_ready[MAX_DOCTORS], doctor_notified[MAX_DOCTORS], doctor_advice[MAX_CAPACITY];

// Patient
void* patient(void* arg) {
    int custnr, officenr; 

    // Enter waiting room, count patient
    dispatch_semaphore_wait(mutex1, DISPATCH_TIME_FOREVER);
    patient_count++;
    custnr = patient_count;
    dispatch_semaphore_signal(mutex1);
    printf("Patient %d enters waiting room, waits for receptionist\n", custnr);

    // Give patient number to receptionist
    dispatch_semaphore_wait(mutex2, DISPATCH_TIME_FOREVER);
    enqueue(&queue1, custnr);
    dispatch_semaphore_signal(receptionist_call);
    dispatch_semaphore_signal(mutex2);

    // Register with receptionist
    dispatch_semaphore_wait(receptionist_register[custnr], DISPATCH_TIME_FOREVER);
    printf("Patient %d leaves receptionist and sits in waiting room\n", custnr);
    dispatch_semaphore_signal(patient_waitingroom[custnr]);

    // Enter doctor's office
    dispatch_semaphore_wait(nurse_directs[custnr], DISPATCH_TIME_FOREVER);
    dispatch_semaphore_wait(mutex5, DISPATCH_TIME_FOREVER);
    dequeue(&queue4, &officenr);
    dispatch_semaphore_signal(mutex5);
    printf("Patient %d enters doctor %d's office\n", custnr, officenr);
    dispatch_semaphore_signal(patient_office[custnr]);

    // Receive advice from doctor
    dispatch_semaphore_wait(doctor_advice[custnr], DISPATCH_TIME_FOREVER);
    printf("Patient %d receives advice from doctor %d\n", custnr, officenr);
    dispatch_semaphore_signal(office[officenr]);

    //leave
    dispatch_semaphore_wait(patient_exit, DISPATCH_TIME_FOREVER);
    printf("Patient %d leaves\n", custnr);
    total_patients--;
    if (total_patients == 0) {
        printf("Simulation complete\n");
        exit(0);
    }
    dispatch_semaphore_signal(patient_exit);

    return NULL;
}

// Receptionist
void* receptionist(void* arg) {
    int i;
    while(1) {
        dispatch_semaphore_wait(receptionist_ready, DISPATCH_TIME_FOREVER); // Receptionist goes one by one
        dispatch_semaphore_wait(receptionist_call, DISPATCH_TIME_FOREVER);  // Wait for patient to call
        dispatch_semaphore_wait(mutex2, DISPATCH_TIME_FOREVER);
        dequeue(&queue1, &i);   // Get Patient number
        dispatch_semaphore_signal(mutex2);

        dispatch_semaphore_signal(receptionist_register[i]);    // Register patient
        dispatch_semaphore_wait(mutex3, DISPATCH_TIME_FOREVER);
        enqueue(&queue2, i);    // Give Patient number to nurse
        dispatch_semaphore_signal(nurse_call);  // Notify nurse that patient is waiting in waiting room
        dispatch_semaphore_signal(mutex3);
        dispatch_semaphore_signal(receptionist_ready);
    }
    return NULL;
}

// Nurse
void* nurse(void* arg) {
    int n_id = *(int*)arg;
    int j;

    while(1) {
        dispatch_semaphore_wait(nurse_ready[n_id], DISPATCH_TIME_FOREVER);  // Nurse is ready
        dispatch_semaphore_wait(nurse_call, DISPATCH_TIME_FOREVER); // Wait for receptionist to call
        dispatch_semaphore_wait(mutex3, DISPATCH_TIME_FOREVER);
        dequeue(&queue2, &j);   // Get patient number
        dispatch_semaphore_signal(mutex3);

        dispatch_semaphore_wait(patient_waitingroom[j], DISPATCH_TIME_FOREVER); // Wait for patient to enter waiting room
        dispatch_semaphore_wait(office[n_id], DISPATCH_TIME_FOREVER);   // Wait until office is free

        dispatch_semaphore_wait(mutex5, DISPATCH_TIME_FOREVER);
        enqueue(&queue4, n_id); // Give doctor number to patient
        printf("Nurse %d directs patient %d to doctor's office\n", n_id, j);
        dispatch_semaphore_signal(nurse_directs[j]);    // Direct patient to doctor's office
        dispatch_semaphore_signal(mutex5);

        
        dispatch_semaphore_wait(mutex4, DISPATCH_TIME_FOREVER);
        enqueue(&doctor_queues[n_id], j);    // Give patient number to doctor
        dispatch_semaphore_signal(doctor_notified[n_id]);   // Notify doctor that patient is waiting
        dispatch_semaphore_signal(mutex4);
        dispatch_semaphore_signal(nurse_ready[n_id]);
    }
    return NULL;
}

// Doctor
void* doctor(void* arg) {
    int d_id = *(int*)arg;
    int k;

    while(1) {
        dispatch_semaphore_wait(doctor_ready[d_id], DISPATCH_TIME_FOREVER); // Doctor is ready
        dispatch_semaphore_wait(doctor_notified[d_id], DISPATCH_TIME_FOREVER);  // Wait for nurse to notify
        dispatch_semaphore_wait(mutex4, DISPATCH_TIME_FOREVER);
        dequeue(&doctor_queues[d_id], &k);   // Get patient number
        dispatch_semaphore_signal(mutex4);

        dispatch_semaphore_wait(patient_office[k], DISPATCH_TIME_FOREVER);  // Wait for patient to enter office
        printf("Doctor %d listens to patient %d's symptoms\n", d_id, k);
        dispatch_semaphore_signal(doctor_advice[k]);    // Give advice to patient
        dispatch_semaphore_signal(doctor_ready[d_id]);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <number of patients> <number of doctors>\n", argv[0]);
        return 1;
    }

    total_patients = atoi(argv[1]);
    doctor_count = atoi(argv[2]);

    if (total_patients > 15 || total_patients <= 0) {
        printf("Invalid number of patients. Maximum number allowed is %d\n", 15);
        return 1;
    }

    if (doctor_count > 3 || doctor_count <= 0) {
        printf("Invalid number of doctors. Maximum number allowed is %d\n", 3);
        return 1;
    }

    printf("Run with %d patient, %d nurses, %d doctors\n", total_patients, doctor_count, doctor_count);
    printf("\n");

    initQueue(&queue1);
    initQueue(&queue2);
    initQueue(&queue3);
    initQueue(&queue4);
    for (int i = 0; i < doctor_count; i++) {
        initQueue(&doctor_queues[i]);
    }

    // Initialize dispatch semaphores
    mutex1 = dispatch_semaphore_create(1);
    mutex2 = dispatch_semaphore_create(1);
    mutex3 = dispatch_semaphore_create(1);
    mutex4 = dispatch_semaphore_create(1);
    mutex5 = dispatch_semaphore_create(1);
    patient_exit = dispatch_semaphore_create(1);
    receptionist_ready = dispatch_semaphore_create(1);
    receptionist_call = dispatch_semaphore_create(0);
    nurse_call = dispatch_semaphore_create(0);

    for (int i = 0; i < MAX_CAPACITY; i++) {
        patient_waitingroom[i] = dispatch_semaphore_create(0);
        patient_office[i] = dispatch_semaphore_create(0);
        receptionist_register[i] = dispatch_semaphore_create(0);
        nurse_directs[i] = dispatch_semaphore_create(0);
        doctor_advice[i] = dispatch_semaphore_create(0);
    }

    for (int i = 0; i < MAX_DOCTORS; i++) {
        nurse_ready[i] = dispatch_semaphore_create(1);
        office[i] = dispatch_semaphore_create(1);
        doctor_ready[i] = dispatch_semaphore_create(1);
        doctor_notified[i] = dispatch_semaphore_create(0);
    }

    // Create threads
    pthread_t patients[total_patients];
    pthread_t receptionist_thread;
    pthread_t nurse_threads[doctor_count];
    pthread_t doctor_threads[doctor_count];

    // Start receptionist thread
    pthread_create(&receptionist_thread, NULL, receptionist, NULL);

    // Start nurse threads
    for (int i = 0; i < doctor_count; i++) {        
        int *arg = malloc(sizeof(*arg));
        *arg = i;
        pthread_create(&nurse_threads[i], NULL, nurse, arg);
    }

    // Start doctor threads
    for (int i = 0; i < doctor_count; i++) {
        int *arg = malloc(sizeof(*arg));
        *arg = i;
        pthread_create(&doctor_threads[i], NULL, doctor, arg);
    }

    // Start patient threads
    for (int i = 0; i < total_patients; i++) {
        pthread_create(&patients[i], NULL, patient, NULL);
    }

    // Join all threads
    pthread_join(receptionist_thread, NULL);
    for (int i = 0; i < doctor_count; i++) {
        pthread_join(nurse_threads[i], NULL);
        pthread_join(doctor_threads[i], NULL);
    }
    for (int i = 0; i < total_patients; i++) {
        pthread_join(patients[i], NULL);
    }

    // Release dispatch semaphores
    dispatch_release(mutex1);
    dispatch_release(mutex2);
    dispatch_release(mutex3);
    dispatch_release(mutex4);
    dispatch_release(mutex5);
    dispatch_release(patient_exit);
    dispatch_release(receptionist_ready);
    dispatch_release(receptionist_call);
    dispatch_release(nurse_call);

    for (int i = 0; i < MAX_CAPACITY; i++) {
        dispatch_release(patient_waitingroom[i]);
        dispatch_release(patient_office[i]);
        dispatch_release(receptionist_register[i]);
        dispatch_release(nurse_ready[i]);
        dispatch_release(nurse_directs[i]);
        dispatch_release(office[i]);
        dispatch_release(doctor_ready[i]);
        dispatch_release(doctor_notified[i]);
        dispatch_release(doctor_advice[i]);
    }

    return 0;
}

