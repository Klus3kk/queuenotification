#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>

#define MSG_BUFFER_SIZE 512
#define TYPE_CONSUMER 200
#define ACTION_SUBSCRIBE 500
#define ACTION_SUBSCRIBE_LIST 550
#define ACTION_NOTIFY 600
#define ACTION_NACK 400

// Struktura wiadomości dla kolejki
struct msg_packet {
    long type;
    char body[MSG_BUFFER_SIZE];
    int sender_id;
    int msg_category;
    int queue_id; // ID kolejki odbiorcy
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <key_file> <client_id>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    key_t ipc_key, client_key;
    int dispatcher_queue_id, client_queue_id;
    int client_id = atoi(argv[2]);

    // Generowanie kluczy IPC
    if ((ipc_key = ftok(argv[1], 42)) == -1) {
        perror("Error generating IPC key for dispatcher");
        exit(EXIT_FAILURE);
    }

    if ((client_key = ftok(argv[1], client_id)) == -1) {
        perror("Error generating IPC key for client");
        exit(EXIT_FAILURE);
    }

    // Połączenie z kolejką dyspozytora
    if ((dispatcher_queue_id = msgget(ipc_key, 0666)) == -1) {
        perror("Error connecting to dispatcher queue");
        exit(EXIT_FAILURE);
    }

    // Tworzenie kolejki klienta
    if ((client_queue_id = msgget(client_key, 0666 | IPC_CREAT)) == -1) {
        perror("Error creating client queue");
        exit(EXIT_FAILURE);
    }

    // Rejestracja klienta
    struct msg_packet registration_packet;
    registration_packet.type = TYPE_CONSUMER;
    registration_packet.sender_id = client_id;
    registration_packet.msg_category = 0;
    registration_packet.queue_id = client_queue_id;

    if (msgsnd(dispatcher_queue_id, &registration_packet, sizeof(registration_packet) - sizeof(long), 0) == -1) {
        perror("Error registering client");
        exit(EXIT_FAILURE);
    }

    printf("Client %d registered successfully.\n", client_id);

    // Żądanie listy powiadomień
    struct msg_packet request_packet;
    request_packet.type = ACTION_SUBSCRIBE_LIST;
    request_packet.sender_id = client_id;
    request_packet.queue_id = client_queue_id;

    if (msgsnd(dispatcher_queue_id, &request_packet, sizeof(request_packet) - sizeof(long), 0) == -1) {
        perror("Error requesting subscription list");
        exit(EXIT_FAILURE);
    }

    // Odbieranie listy powiadomień
    struct msg_packet response_packet;
    if (msgrcv(client_queue_id, &response_packet, sizeof(response_packet) - sizeof(long), 0, 0) == -1) {
        perror("Error receiving subscription list");
        exit(EXIT_FAILURE);
    }

    if (response_packet.type == ACTION_NACK) {
        fprintf(stderr, "Dyspozytor zwrócił komunikat: %s\n", response_packet.body);
        exit(EXIT_FAILURE);
    } else if (response_packet.type != ACTION_SUBSCRIBE_LIST) {
        fprintf(stderr, "Unexpected response type: %ld\n", response_packet.type);
        exit(EXIT_FAILURE);
    }

    printf("Available notifications:\n%s\n", response_packet.body);

    // Subskrybowanie kategorii
    printf("Enter category to subscribe to: ");
    int category;
    if (scanf("%d", &category) != 1) {
        fprintf(stderr, "Invalid input. Exiting.\n");
        exit(EXIT_FAILURE);
    }

    struct msg_packet subscribe_packet;
    subscribe_packet.type = ACTION_SUBSCRIBE;
    subscribe_packet.sender_id = client_id;
    subscribe_packet.msg_category = category;
    subscribe_packet.queue_id = client_queue_id;

    if (msgsnd(dispatcher_queue_id, &subscribe_packet, sizeof(subscribe_packet) - sizeof(long), 0) == -1) {
        perror("Error sending subscription request");
        exit(EXIT_FAILURE);
    }

    // Oczekiwanie na potwierdzenie subskrypcji
    if (msgrcv(client_queue_id, &response_packet, sizeof(response_packet) - sizeof(long), 0, 0) == -1) {
        perror("Error receiving subscription acknowledgment");
        exit(EXIT_FAILURE);
    }

    if (response_packet.type == ACTION_NACK) {
        fprintf(stderr, "Subscription request rejected by dispatcher. Exiting.\n");
        exit(EXIT_FAILURE);
    } else if (response_packet.type != ACTION_SUBSCRIBE) {
        fprintf(stderr, "Unexpected response type: %ld\n", response_packet.type);
        exit(EXIT_FAILURE);
    }

    printf("Subscribed to category %d. Waiting for notifications...\n", category);

    // Oczekiwanie na powiadomienia
    while (1) {
        struct msg_packet notification_packet;
        if (msgrcv(client_queue_id, &notification_packet, sizeof(notification_packet) - sizeof(long), 0, 0) == -1) {
            if (errno == EIDRM) {
                printf("Client queue has been removed. Exiting.\n");
                break;
            }
            perror("Error receiving notification");
            exit(EXIT_FAILURE);
        }

        if (notification_packet.type == ACTION_NOTIFY) {
            printf("Notification received: %s\n", notification_packet.body);
        }
    }

    return 0;
}
