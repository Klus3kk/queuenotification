#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>

#define MSG_BUFFER_SIZE 512
#define TYPE_PRODUCER 100
#define ACTION_NOTIFY 600
#define ACTION_ACK 300
#define ACTION_NACK 400

// Message structure
struct msg_packet {
    long type;
    char body[MSG_BUFFER_SIZE];
    int sender_id;
    int msg_category;
    int notification_queue_id;
    int action_queue_id;
};

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <key_file> <producer_id> <message_category>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    key_t ipc_key;
    int dispatcher_queue_id;
    int producer_id = atoi(argv[2]);
    int message_category = atoi(argv[3]);

    // Generate IPC key
    if ((ipc_key = ftok(argv[1], 42)) == -1) {
        perror("Error generating IPC key");
        exit(EXIT_FAILURE);
    }

    // Connect to the dispatcher queue
    if ((dispatcher_queue_id = msgget(ipc_key, 0666 | IPC_CREAT)) == -1) {
        perror("Error connecting to dispatcher queue");
        exit(EXIT_FAILURE);
    }

    // Register producer
    struct msg_packet registration_packet;
    registration_packet.type = TYPE_PRODUCER;
    registration_packet.sender_id = producer_id;
    registration_packet.msg_category = message_category;
    registration_packet.notification_queue_id = producer_id;
    registration_packet.action_queue_id = producer_id;

    if (msgsnd(dispatcher_queue_id, &registration_packet, sizeof(registration_packet) - sizeof(long), 0) == -1) {
        perror("Error sending registration packet");
        exit(EXIT_FAILURE);
    }

    // Wait for acknowledgment
    struct msg_packet response_packet;
    if (msgrcv(dispatcher_queue_id, &response_packet, sizeof(response_packet) - sizeof(long), 0, 0) == -1) {
        perror("Error receiving acknowledgment");
        exit(EXIT_FAILURE);
    }

    if (response_packet.type == ACTION_NACK) {
        fprintf(stderr, "Registration rejected by dispatcher: %s\n", response_packet.body);
        exit(EXIT_FAILURE);
    } else if (response_packet.type != ACTION_ACK) {
        fprintf(stderr, "Unexpected response type: %ld\n", response_packet.type);
        exit(EXIT_FAILURE);
    }

    printf("Registration successful. Producer ID: %d, Category: %d.\n", producer_id, message_category);

    // Send notifications
    struct msg_packet notification_packet;
    notification_packet.type = ACTION_NOTIFY;
    notification_packet.sender_id = producer_id;
    notification_packet.msg_category = message_category;
    notification_packet.notification_queue_id = producer_id;
    notification_packet.action_queue_id = producer_id;

    while (1) {
        printf("Enter a message to send (or 'exit' to quit): ");
        if (fgets(notification_packet.body, MSG_BUFFER_SIZE, stdin) == NULL) {
            perror("Error reading input");
            continue;
        }

        // Remove the newline character
        notification_packet.body[strcspn(notification_packet.body, "\n")] = '\0';

        if (strcmp(notification_packet.body, "exit") == 0) {
            printf("Exiting producer program.\n");
            break;
        }

        // Send notification to dispatcher
        if (msgsnd(dispatcher_queue_id, &notification_packet, sizeof(notification_packet) - sizeof(long), 0) == -1) {
            perror("Error sending notification");
        } else {
            printf("Notification sent: %s\n", notification_packet.body);
        }
    }

    return 0;
}
