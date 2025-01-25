#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>

#define MSG_BUFFER_SIZE 512
#define TYPE_PRODUCER 100
#define TYPE_CONSUMER 200
#define ACTION_ACK 300
#define ACTION_NACK 400
#define ACTION_SUBSCRIBE 500
#define ACTION_UNSUBSCRIBE 555
#define ACTION_SUBSCRIBE_LIST 550
#define ACTION_UNSUBSCRIBE_LIST 505
#define ACTION_NOTIFY 600

struct msg_packet {
    long type;
    char body[MSG_BUFFER_SIZE];
    int sender_id;
    int msg_category;
    int notification_queue_id;
    int action_queue_id;
};

struct producer {
    int id;
    int msg_category;
    struct producer *next;
};

struct subscriber {
    int id;
    int msg_category;
    int notification_queue_id;
    struct subscriber *next;
};

// Global lists
struct producer *producer_list = NULL;
struct subscriber *subscriber_list = NULL;

// Helper functions
void register_producer(int id, int msg_category);
void register_subscriber(int id, int msg_category, int notification_queue_id);
void generate_producer_list(char *buffer);
void generate_subscribed_list(char *buffer, int id);
void unregister_subscriber(int id, int msg_category);
int category_exists(int msg_category);
int client_is_subscriber(int id);
void notify_clients_about_new_category(int msg_category);
void distribute_notification(int msg_category, const char *message);

// Register a producer
void register_producer(int id, int msg_category) {
    struct producer *new_prod = (struct producer *)malloc(sizeof(struct producer));
    if (!new_prod) {
        perror("Memory allocation error for producer");
        exit(EXIT_FAILURE);
    }
    new_prod->id = id;
    new_prod->msg_category = msg_category;
    new_prod->next = producer_list;
    producer_list = new_prod;
    printf("Registered producer: ID %d, category %d\n", id, msg_category);
}

// Register a subscriber
void register_subscriber(int id, int msg_category, int notification_queue_id) {
    struct subscriber *new_sub = (struct subscriber *)malloc(sizeof(struct subscriber));
    if (!new_sub) {
        perror("Memory allocation error for subscriber");
        exit(EXIT_FAILURE);
    }
    new_sub->id = id;
    new_sub->msg_category = msg_category;
    new_sub->notification_queue_id = notification_queue_id;
    new_sub->next = subscriber_list;
    subscriber_list = new_sub;
    printf("Registered subscriber: ID %d, category %d, queue %d\n", id, msg_category, notification_queue_id);
}

// Check if category exists
int category_exists(int msg_category) {
    struct producer *current = producer_list;
    while (current) {
        if (current->msg_category == msg_category) {
            return 1;
        }
        current = current->next;
    }
    return 0;
}

// Notify clients about a new category
void notify_clients_about_new_category(int msg_category) {
    struct subscriber *current = subscriber_list;
    while (current) {
        struct msg_packet notification;
        notification.type = ACTION_NOTIFY;
        snprintf(notification.body, MSG_BUFFER_SIZE, "New notification category: %d", msg_category);
        notification.sender_id = 0;  // Dispatcher as the sender
        notification.msg_category = msg_category;

        if (msgsnd(current->notification_queue_id, &notification, sizeof(notification) - sizeof(long), 0) == -1) {
            perror("Error sending notification about new category");
        } else {
            printf("Notified subscriber %d about new category %d\n", current->id, msg_category);
        }
        current = current->next;
    }
}

// Distribute notifications to subscribers
void distribute_notification(int msg_category, const char *message) {
    struct subscriber *current = subscriber_list;
    while (current) {
        if (current->msg_category == msg_category) {
            struct msg_packet notification;
            notification.type = ACTION_NOTIFY;
            snprintf(notification.body, MSG_BUFFER_SIZE, "%s", message);
            notification.sender_id = 0;  // Dispatcher as the sender
            notification.msg_category = msg_category;

            if (msgsnd(current->notification_queue_id, &notification, sizeof(notification) - sizeof(long), 0) == -1) {
                perror("Error sending notification to subscriber");
            } else {
                printf("Sent notification to subscriber %d for category %d\n", current->id, msg_category);
            }
        }
        current = current->next;
    }
}

// Generate a list of producers
void generate_producer_list(char *buffer) {
    struct producer *current = producer_list;
    char temp[MSG_BUFFER_SIZE] = {0};
    while (current) {
        snprintf(temp, sizeof(temp), "ID: %d, Category: %d\n", current->id, current->msg_category);
        strncat(buffer, temp, MSG_BUFFER_SIZE - strlen(buffer) - 1);
        current = current->next;
    }
}

// Generate a list of subscriptions for a client
void generate_subscribed_list(char *buffer, int id) {
    struct subscriber *current = subscriber_list;
    char temp[MSG_BUFFER_SIZE] = {0};
    while (current) {
        if (current->id == id) {
            snprintf(temp, sizeof(temp), "Category: %d\n", current->msg_category);
            strncat(buffer, temp, MSG_BUFFER_SIZE - strlen(buffer) - 1);
        }
        current = current->next;
    }
}

// Check if client is a subscriber
int client_is_subscriber(int id) {
    struct subscriber *current = subscriber_list;
    int count = 0;
    while (current) {
        if (current->id == id) {
            count++;
            break;
        }
    }
    return count;
}

// Unregister a subscriber
void unregister_subscriber(int id, int msg_category) {
    struct subscriber **current = &subscriber_list;
    while (*current) {
        if ((*current)->id == id && (*current)->msg_category == msg_category) {
            struct subscriber *to_delete = *current;
            *current = (*current)->next;
            free(to_delete);
            printf("Unregistered subscriber: ID %d, category %d\n", id, msg_category);
            return;
        }
        current = &((*current)->next);
    }
    printf("Subscriber not found: ID %d, category %d\n", id, msg_category);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <key_file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    key_t ipc_key;
    int dispatcher_queue_id;

    // Generate IPC key
    if ((ipc_key = ftok(argv[1], 42)) == -1) {
        perror("Error generating IPC key");
        exit(EXIT_FAILURE);
    }

    // Create dispatcher queue
    if ((dispatcher_queue_id = msgget(ipc_key, 0666 | IPC_CREAT)) == -1) {
        perror("Error creating dispatcher queue");
        exit(EXIT_FAILURE);
    }

    struct msg_packet packet;
    while (1) {
        if (msgrcv(dispatcher_queue_id, &packet, sizeof(packet) - sizeof(long), 0, 0) == -1) {
            perror("Error receiving message");
            exit(EXIT_FAILURE);
        }

        printf("Received message: type=%ld, sender_id=%d, category=%d, notification_queue_id=%d\n",
               packet.type, packet.sender_id, packet.msg_category, packet.notification_queue_id);

        struct msg_packet response;
        response.sender_id = 0;
        response.notification_queue_id = packet.notification_queue_id;
        response.action_queue_id = packet.action_queue_id;

        switch (packet.type) {
            case TYPE_PRODUCER:
                printf("Producer registered: ID %d, Category %d\n", packet.sender_id, packet.msg_category);
                if (category_exists(packet.msg_category)) {
                    response.type = ACTION_NACK;
                    snprintf(response.body, MSG_BUFFER_SIZE, "Category %d already exists.", packet.msg_category);
                } else {
                    register_producer(packet.sender_id, packet.msg_category);
                    response.type = ACTION_ACK;
                    snprintf(response.body, MSG_BUFFER_SIZE, "Producer registered successfully.");
                }
                if (msgsnd(dispatcher_queue_id, &response, sizeof(response) - sizeof(long), 0) == -1) {
                    perror("Error sending producer acknowledgment");
                }
                break;

            case TYPE_CONSUMER:
                printf("Consumer registered: ID %d\n", packet.sender_id);
                response.type = ACTION_ACK;
                break;

            case ACTION_SUBSCRIBE_LIST:
                printf("Consumer %d requested list of available notifications.\n", packet.sender_id);
                if (producer_list == NULL) {
                    response.type = ACTION_NACK;
                    snprintf(response.body, MSG_BUFFER_SIZE, "No available notifications.");
                } else {
                    memset(response.body, 0, MSG_BUFFER_SIZE);
                    generate_producer_list(response.body);
                    response.type = ACTION_SUBSCRIBE_LIST;
                }
                if (msgsnd(packet.action_queue_id, &response, sizeof(response) - sizeof(long), 0) == -1) {
                    perror("Error sending subscription list to client");
                }
                break;

            case ACTION_UNSUBSCRIBE_LIST:
                printf("Consumer %d requested list of subscribed notifications.\n", packet.sender_id);
                if (client_is_subscriber(packet.sender_id) == 0) {
                    response.type = ACTION_NACK;
                    snprintf(response.body, MSG_BUFFER_SIZE, "No subscriptions to unsubscribe.");
                } else {
                    memset(response.body, 0, MSG_BUFFER_SIZE);
                    generate_subscribed_list(response.body, packet.sender_id);
                    response.type = ACTION_UNSUBSCRIBE_LIST;
                }
                if (msgsnd(packet.action_queue_id, &response, sizeof(response) - sizeof(long), 0) == -1) {
                    perror("Error sending unsubscription list to client");
                }
                break;

            case ACTION_SUBSCRIBE:
                printf("Consumer %d subscribed to category %d\n", packet.sender_id, packet.msg_category);
                register_subscriber(packet.sender_id, packet.msg_category, packet.notification_queue_id);
                response.type = ACTION_ACK;
                if (msgsnd(packet.action_queue_id, &response, sizeof(response) - sizeof(long), 0) == -1) {
                    perror("Error sending acknowledgment for subscription");
                }
                break;

            case ACTION_UNSUBSCRIBE:
                printf("Consumer %d unsubscribed from category %d\n", packet.sender_id, packet.msg_category);
                unregister_subscriber(packet.sender_id, packet.msg_category);
                response.type = ACTION_ACK;
                if (msgsnd(packet.action_queue_id, &response, sizeof(response) - sizeof(long), 0) == -1) {
                    perror("Error sending acknowledgment for unsubscription");
                }
                break;

            case ACTION_NOTIFY:
                printf("Notification received from producer %d for category %d: %s\n",
                       packet.sender_id, packet.msg_category, packet.body);
                distribute_notification(packet.msg_category, packet.body);
                break;

            default:
                printf("Unknown message type: %ld\n", packet.type);
                response.type = ACTION_NACK;
        }
    }

    return 0;
}
