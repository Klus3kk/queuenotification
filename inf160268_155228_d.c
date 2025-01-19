#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>

#define MSG_BUFFER_SIZE 512
#define TYPE_PRODUCER 100
#define TYPE_CONSUMER 200
#define ACTION_SUBSCRIBE 300
#define ACTION_UNSUBSCRIBE 400
#define ACTION_NOTIFY 500

struct msg_packet {
    long type;
    char body[MSG_BUFFER_SIZE];
    int sender_id;
    int msg_category;
};

struct subscriber {
    int id;
    int msg_category;
    int queue_id;
    struct subscriber *next;
};

struct subscriber *subscriber_list = NULL;

void register_subscriber(int id, int msg_category, int queue_id) {
    struct subscriber *new_sub = (struct subscriber *)malloc(sizeof(struct subscriber));
    if (!new_sub) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }
    new_sub->id = id;
    new_sub->msg_category = msg_category;
    new_sub->queue_id = queue_id;
    new_sub->next = subscriber_list;
    subscriber_list = new_sub;
    printf("Subscriber %d added for category %d with queue ID %d\n", id, msg_category, queue_id);
}

// Remove a subscriber from the list
void unregister_subscriber(int id, int msg_category) {
    struct subscriber **current = &subscriber_list;
    while (*current) {
        if ((*current)->id == id && (*current)->msg_category == msg_category) {
            struct subscriber *to_delete = *current;
            *current = (*current)->next;
            free(to_delete);
            printf("Subscriber %d removed from category %d\n", id, msg_category);
            return;
        }
        current = &((*current)->next);
    }
    printf("No matching subscriber found for ID %d and category %d\n", id, msg_category);
}

void distribute_notification(int msg_category, const char *notification) {
    struct subscriber *current = subscriber_list;
    int count = 0;
    while (current) {
        if (current->msg_category == msg_category) {
            struct msg_packet packet;
            packet.type = ACTION_NOTIFY;
            snprintf(packet.body, MSG_BUFFER_SIZE, "%s", notification);
            packet.sender_id = 0; // Dyspozytor jako nadawca
            packet.msg_category = msg_category;

            if (msgsnd(current->queue_id, &packet, sizeof(packet) - sizeof(long), 0) == -1) {
                perror("Error sending message to client");
            } else {
                printf("Delivered message to subscriber %d via queue ID %d\n", current->id, current->queue_id);
                count++;
            }
        }
        current = current->next;
    }
    if (count == 0) {
        printf("No subscribers found for category %d\n", msg_category);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <key_file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    key_t ipc_key;
    int producer_queue_id;

    if ((ipc_key = ftok(argv[1], 42)) == -1) {
        perror("Error generating IPC key");
        exit(EXIT_FAILURE);
    }

    if ((producer_queue_id = msgget(ipc_key, 0666 | IPC_CREAT)) == -1) {
        perror("Error creating producer-dispatcher queue");
        exit(EXIT_FAILURE);
    }

    struct msg_packet packet;
    while (1) {
        if (msgrcv(producer_queue_id, &packet, sizeof(packet) - sizeof(long), 0, 0) == -1) {
            perror("Error receiving message");
            exit(EXIT_FAILURE);
        }

        switch (packet.type) {
        case TYPE_PRODUCER:
            printf("Producer registered: ID %d, Category %d\n", packet.sender_id, packet.msg_category);
            break;
        case TYPE_CONSUMER: {
            key_t client_key = ftok(argv[1], packet.sender_id);
            int client_queue_id;
            if ((client_queue_id = msgget(client_key, 0666 | IPC_CREAT)) == -1) {
                perror("Error creating client queue");
                break;
            }
            printf("Consumer registered: ID %d with queue ID %d\n", packet.sender_id, client_queue_id);
            break;
        }
        case ACTION_SUBSCRIBE:
            printf("Consumer %d subscribed to category %d\n", packet.sender_id, packet.msg_category);
            register_subscriber(packet.sender_id, packet.msg_category, packet.sender_id);
            break;
        case ACTION_UNSUBSCRIBE:
            printf("Consumer %d unsubscribed from category %d\n", packet.sender_id, packet.msg_category);
            unregister_subscriber(packet.sender_id, packet.msg_category);
            break;
        case ACTION_NOTIFY:
            printf("Notification received from producer %d for category %d: %s\n", packet.sender_id, packet.msg_category, packet.body);
            distribute_notification(packet.msg_category, packet.body);
            break;
        default:
            printf("Unrecognized message type: %ld\n", packet.type);
        }
    }

    if (msgctl(producer_queue_id, IPC_RMID, NULL) == -1) {
        perror("Error removing producer-dispatcher queue");
        exit(EXIT_FAILURE);
    }

    return 0;
}
