#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include <unistd.h>

#define MSG_BUFFER_SIZE 512
#define TYPE_CONSUMER 200
#define ACTION_SUBSCRIBE 500
#define ACTION_UNSUBSCRIBE 555
#define ACTION_SUBSCRIBE_LIST 550
#define ACTION_UNSUBSCRIBE_LIST 505
#define ACTION_NOTIFY 600
#define ACTION_NACK 400
#define ACTION_ACK 300

// Structure for messages in the queue
struct msg_packet {
    long type;
    char body[MSG_BUFFER_SIZE];
    int sender_id;
    int msg_category;
    int notification_queue_id; // Notification queue ID
    int action_queue_id;       // Action queue ID
};

// Function prototypes
void request_notification_list(int dispatcher_queue_id, int client_action_queue_id, struct msg_packet request_packet, struct msg_packet response_packet);
void subscribe(int dispatcher_queue_id, int client_action_queue_id, struct msg_packet subscribe_packet, struct msg_packet response_packet);
void request_subscribed_notifications_list(int dispatcher_queue_id, int client_action_queue_id, struct msg_packet request_packet, struct msg_packet response_packet);
void unsubscribe(int dispatcher_queue_id, int client_action_queue_id, struct msg_packet subscribe_packet, struct msg_packet response_packet);

void request_notification_list(int dispatcher_queue_id, int client_action_queue_id, struct msg_packet request_packet, struct msg_packet response_packet) {
    request_packet.type = ACTION_SUBSCRIBE_LIST;
    if (msgsnd(dispatcher_queue_id, &request_packet, sizeof(request_packet) - sizeof(long), 0) == -1) {
        perror("Error requesting subscription list");
        exit(EXIT_FAILURE);
    }

    // Receive the list of notifications
    if (msgrcv(client_action_queue_id, &response_packet, sizeof(response_packet) - sizeof(long), 0, 0) == -1) {
        perror("Error receiving subscription list");
        exit(EXIT_FAILURE);
    }

    if (response_packet.type == ACTION_NACK) {
        fprintf(stderr, "Dispatcher returned: %s\n", response_packet.body);
        exit(EXIT_FAILURE);
    } else if (response_packet.type != ACTION_SUBSCRIBE_LIST) {
        fprintf(stderr, "Unexpected response type: %ld\n", response_packet.type);
        exit(EXIT_FAILURE);
    }

    printf("Available notifications:\n%s", response_packet.body);
}

void request_subscribed_notifications_list(int dispatcher_queue_id, int client_action_queue_id, struct msg_packet request_packet, struct msg_packet response_packet) {
    request_packet.type = ACTION_UNSUBSCRIBE_LIST;
    if (msgsnd(dispatcher_queue_id, &request_packet, sizeof(request_packet) - sizeof(long), 0) == -1) {
        perror("Error requesting unsubscription list");
        exit(EXIT_FAILURE);
    }

    if (msgrcv(client_action_queue_id, &response_packet, sizeof(response_packet) - sizeof(long), 0, 0) == -1) {
        perror("Error receiving subscription list");
        exit(EXIT_FAILURE);
    }

    if (response_packet.type == ACTION_NACK) {
        fprintf(stderr, "Dispatcher returned: %s\n", response_packet.body);
        exit(EXIT_FAILURE);
    } else if (response_packet.type != ACTION_UNSUBSCRIBE_LIST) {
        fprintf(stderr, "Unexpected response type in unsubscribe list action: %ld\n", response_packet.type);
        exit(EXIT_FAILURE);
    }

    printf("Subscribed categories:\n%s", response_packet.body);
}

void subscribe(int dispatcher_queue_id, int client_action_queue_id, struct msg_packet subscribe_packet, struct msg_packet response_packet) {
    subscribe_packet.type = ACTION_SUBSCRIBE;
    printf("Enter category to subscribe to: ");
    int category;
    if (scanf("%d", &category) != 1) {
        fprintf(stderr, "Invalid input. Exiting.\n");
        exit(EXIT_FAILURE);
    }
    subscribe_packet.msg_category = category;

    if (msgsnd(dispatcher_queue_id, &subscribe_packet, sizeof(subscribe_packet) - sizeof(long), 0) == -1) {
        perror("Error sending subscription request");
        exit(EXIT_FAILURE);
    }

    // Wait for subscription acknowledgment
    if (msgrcv(client_action_queue_id, &response_packet, sizeof(response_packet) - sizeof(long), 0, 0) == -1) {
        perror("Error receiving subscription acknowledgment");
        exit(EXIT_FAILURE);
    }

    if (response_packet.type == ACTION_NACK) {
        fprintf(stderr, "Subscription request rejected by dispatcher. Exiting.\n");
        exit(EXIT_FAILURE);
    } else if (response_packet.type != ACTION_ACK) {
        fprintf(stderr, "Unexpected response type: %ld\n", response_packet.type);
        exit(EXIT_FAILURE);
    }

    printf("Subscribed to category %d. Waiting for notifications...\n", category);
}

void unsubscribe(int dispatcher_queue_id, int client_action_queue_id, struct msg_packet subscribe_packet, struct msg_packet response_packet) {
    subscribe_packet.type = ACTION_UNSUBSCRIBE;
    printf("Enter category to unsubscribe: ");
    int category;
    if (scanf("%d", &category) != 1) {
        fprintf(stderr, "Invalid input. Exiting.\n");
        exit(EXIT_FAILURE);
    }
    subscribe_packet.msg_category = category;

    if (msgsnd(dispatcher_queue_id, &subscribe_packet, sizeof(subscribe_packet) - sizeof(long), 0) == -1) {
        perror("Error sending unsubscription request");
        exit(EXIT_FAILURE);
    }

    // Wait for unsubscription acknowledgment
    if (msgrcv(client_action_queue_id, &response_packet, sizeof(response_packet) - sizeof(long), 0, 0) == -1) {
        perror("Error receiving unsubscription acknowledgment");
        exit(EXIT_FAILURE);
    }

    if (response_packet.type == ACTION_NACK) {
        fprintf(stderr, "Unsubscription request rejected by dispatcher. Exiting.\n");
        exit(EXIT_FAILURE);
    } else if (response_packet.type != ACTION_ACK) {
        fprintf(stderr, "Unexpected response type: %ld\n", response_packet.type);
        exit(EXIT_FAILURE);
    }

    printf("Unsubscribed from category %d.\n", category);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <key_file> <client_id>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    key_t ipc_key, client_notification_key, client_action_key;
    int dispatcher_queue_id, client_queue_id, client_action_queue_id;
    int client_id = atoi(argv[2]);

    // Generate IPC keys
    if ((ipc_key = ftok(argv[1], 42)) == -1) {
        perror("Error generating IPC key for dispatcher");
        exit(EXIT_FAILURE);
    }

    if ((client_notification_key = ftok(argv[1], client_id)) == -1) {
        perror("Error generating IPC key for client notifications");
        exit(EXIT_FAILURE);
    }

    if ((client_action_key = ftok(argv[1], client_id + 555)) == -1) {
        perror("Error generating IPC key for client actions");
        exit(EXIT_FAILURE);
    }

    // Connect to dispatcher queue
    if ((dispatcher_queue_id = msgget(ipc_key, 0666)) == -1) {
        perror("Error connecting to dispatcher queue");
        exit(EXIT_FAILURE);
    }

    // Create client queues
    if ((client_queue_id = msgget(client_notification_key, 0666 | IPC_CREAT)) == -1) {
        perror("Error creating client notifications queue");
        exit(EXIT_FAILURE);
    }

    if ((client_action_queue_id = msgget(client_action_key, 0666 | IPC_CREAT)) == -1) {
        perror("Error creating client actions queue");
        exit(EXIT_FAILURE);
    }

    // Register client
    struct msg_packet registration_packet;
    registration_packet.type = TYPE_CONSUMER;
    registration_packet.sender_id = client_id;
    registration_packet.msg_category = 0;
    registration_packet.notification_queue_id = client_queue_id;
    registration_packet.action_queue_id = client_action_queue_id;

    if (msgsnd(dispatcher_queue_id, &registration_packet, sizeof(registration_packet) - sizeof(long), 0) == -1) {
        perror("Error registering client");
        exit(EXIT_FAILURE);
    }

    printf("Client %d registered successfully.\n", client_id);

    // Request available notifications
    struct msg_packet request_packet;
    request_packet.sender_id = client_id;
    request_packet.notification_queue_id = client_queue_id;
    request_packet.action_queue_id = client_action_queue_id;

    struct msg_packet response_packet;

    request_notification_list(dispatcher_queue_id, client_action_queue_id, request_packet, response_packet);

    // Subscribe to categories
    struct msg_packet subscribe_packet;
    subscribe_packet.sender_id = client_id;
    subscribe_packet.notification_queue_id = client_queue_id;
    subscribe_packet.action_queue_id = client_action_queue_id;

    subscribe(dispatcher_queue_id, client_action_queue_id, subscribe_packet, response_packet);

    if (fork() == 0) {
        // Wait for notifications
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
    } else {
        // Handle client actions
        while (1) {
            char user_input[MSG_BUFFER_SIZE];
            if (fgets(user_input, MSG_BUFFER_SIZE, stdin) == NULL) {
                perror("Error reading input");
                continue;
            }

            user_input[strcspn(user_input, "\n")] = '\0';

            if (strcmp(user_input, "unsubscribe") == 0) {
                request_subscribed_notifications_list(dispatcher_queue_id, client_action_queue_id, request_packet, response_packet);
                unsubscribe(dispatcher_queue_id, client_action_queue_id, subscribe_packet, response_packet);
            } else if (strcmp(user_input, "subscribe") == 0) {
                request_notification_list(dispatcher_queue_id, client_action_queue_id, request_packet, response_packet);
                subscribe(dispatcher_queue_id, client_action_queue_id, subscribe_packet, response_packet);
            } else {
                continue;
            }
        }
    }

    return 0;
}
