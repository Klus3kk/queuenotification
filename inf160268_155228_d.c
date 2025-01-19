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
#define ACTION_SUBSCRIBE_LIST 550
#define ACTION_NOTIFY 600

struct msg_packet {
    long type;
    char body[MSG_BUFFER_SIZE];
    int sender_id;
    int msg_category;
    int queue_id;
};

struct producer {
    int id;
    int msg_category;
    struct producer *next;
};

struct subscriber {
    int id;
    int msg_category;
    int queue_id;
    struct subscriber *next;
};

// Globalne listy
struct producer *producer_list = NULL;
struct subscriber *subscriber_list = NULL;

// Funkcje pomocnicze
void register_producer(int id, int msg_category);
void register_subscriber(int id, int msg_category, int queue_id);
void generate_producer_list(char *buffer);
void unregister_subscriber(int id, int msg_category);
int category_exists(int msg_category);
void notify_clients_about_new_category(int msg_category);
void distribute_notification(int msg_category, const char *message);

// Dodanie producenta
void register_producer(int id, int msg_category) {
    struct producer *new_prod = (struct producer *)malloc(sizeof(struct producer));
    if (!new_prod) {
        perror("Błąd alokacji pamięci dla producenta");
        exit(EXIT_FAILURE);
    }
    new_prod->id = id;
    new_prod->msg_category = msg_category;
    new_prod->next = producer_list;
    producer_list = new_prod;
    printf("Zarejestrowano producenta: ID %d, kategoria %d\n", id, msg_category);
}

// Dodanie subskrybenta
void register_subscriber(int id, int msg_category, int queue_id) {
    struct subscriber *new_sub = (struct subscriber *)malloc(sizeof(struct subscriber));
    if (!new_sub) {
        perror("Błąd alokacji pamięci dla subskrybenta");
        exit(EXIT_FAILURE);
    }
    new_sub->id = id;
    new_sub->msg_category = msg_category;
    new_sub->queue_id = queue_id;
    new_sub->next = subscriber_list;
    subscriber_list = new_sub;
    printf("Dodano subskrybenta: ID %d, kategoria %d, kolejka %d\n", id, msg_category, queue_id);
}

// Sprawdzenie, czy istnieje kategoria
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

// Powiadamianie klientów o nowej kategorii
void notify_clients_about_new_category(int msg_category) {
    struct subscriber *current = subscriber_list;
    while (current) {
        struct msg_packet notification;
        notification.type = ACTION_NOTIFY;
        snprintf(notification.body, MSG_BUFFER_SIZE, "Nowa kategoria powiadomień: %d", msg_category);
        notification.sender_id = 0;  // Dyspozytor jako nadawca
        notification.msg_category = msg_category;

        if (msgsnd(current->queue_id, &notification, sizeof(notification) - sizeof(long), 0) == -1) {
            perror("Błąd wysyłania powiadomienia o nowej kategorii");
        } else {
            printf("Powiadomiono subskrybenta %d o nowej kategorii %d\n", current->id, msg_category);
        }
        current = current->next;
    }
}

// Rozsyłanie powiadomień do subskrybentów
void distribute_notification(int msg_category, const char *message) {
    struct subscriber *current = subscriber_list;
    while (current) {
        if (current->msg_category == msg_category) {
            struct msg_packet notification;
            notification.type = ACTION_NOTIFY;
            snprintf(notification.body, MSG_BUFFER_SIZE, "%s", message);
            notification.sender_id = 0;  // Dyspozytor jako nadawca
            notification.msg_category = msg_category;

            if (msgsnd(current->queue_id, &notification, sizeof(notification) - sizeof(long), 0) == -1) {
                perror("Błąd wysyłania powiadomienia do subskrybenta");
            } else {
                printf("Wysłano powiadomienie do subskrybenta %d dla kategorii %d\n", current->id, msg_category);
            }
        }
        current = current->next;
    }
}

// Generowanie listy producentów
void generate_producer_list(char *buffer) {
    struct producer *current = producer_list;
    char temp[MSG_BUFFER_SIZE] = {0};
    while (current) {
        snprintf(temp, sizeof(temp), "ID: %d, Kategoria: %d\n", current->id, current->msg_category);
        strncat(buffer, temp, MSG_BUFFER_SIZE - strlen(buffer) - 1);
        current = current->next;
    }
}

// Usunięcie subskrybenta
void unregister_subscriber(int id, int msg_category) {
    struct subscriber **current = &subscriber_list;
    while (*current) {
        if ((*current)->id == id && (*current)->msg_category == msg_category) {
            struct subscriber *to_delete = *current;
            *current = (*current)->next;
            free(to_delete);
            printf("Usunięto subskrybenta: ID %d, kategoria %d\n", id, msg_category);
            return;
        }
        current = &((*current)->next);
    }
    printf("Nie znaleziono subskrybenta: ID %d, kategoria %d\n", id, msg_category);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Użycie: %s <plik_klucza>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    key_t ipc_key;
    int dispatcher_queue_id;

    // Generowanie klucza IPC
    if ((ipc_key = ftok(argv[1], 42)) == -1) {
        perror("Błąd generowania klucza IPC");
        exit(EXIT_FAILURE);
    }

    // Tworzenie kolejki dyspozytora
    if ((dispatcher_queue_id = msgget(ipc_key, 0666 | IPC_CREAT)) == -1) {
        perror("Błąd tworzenia kolejki dyspozytora");
        exit(EXIT_FAILURE);
    }

    struct msg_packet packet;
    while (1) {
        if (msgrcv(dispatcher_queue_id, &packet, sizeof(packet) - sizeof(long), 0, 0) == -1) {
            perror("Błąd odbierania wiadomości");
            exit(EXIT_FAILURE);
        }

        printf("Odebrano wiadomość: type=%ld, sender_id=%d, category=%d, queue_id=%d\n",
               packet.type, packet.sender_id, packet.msg_category, packet.queue_id);

        struct msg_packet response;
        response.sender_id = 0; // Dyspozytor jako nadawca
        response.queue_id = packet.queue_id; // Kolejka odbiorcy

        switch (packet.type) {
            case TYPE_PRODUCER:
                printf("Producer registered: ID %d, Category %d\n", packet.sender_id, packet.msg_category);
                if (category_exists(packet.msg_category)) { // Sprawdź, czy kategoria istnieje
                    response.type = ACTION_NACK;
                    snprintf(response.body, MSG_BUFFER_SIZE, "Category %d already exists.", packet.msg_category);
                } else {
                    register_producer(packet.sender_id, packet.msg_category);
                    response.type = ACTION_ACK;
                    snprintf(response.body, MSG_BUFFER_SIZE, "Category %d registered.", packet.msg_category);
                    notify_clients_about_new_category(packet.msg_category); // Powiadom klientów
                }
                if (msgsnd(packet.queue_id, &response, sizeof(response) - sizeof(long), 0) == -1) {
                    perror("Error sending producer acknowledgment");
                }
                break;


            case TYPE_CONSUMER:
                printf("Zarejestrowano konsumenta: ID %d\n", packet.sender_id);
                response.type = ACTION_ACK;
                break;

            case ACTION_SUBSCRIBE_LIST:
                printf("Konsument %d zażądał listy powiadomień.\n", packet.sender_id);

                // Sprawdź, czy lista producentów jest pusta
                if (producer_list == NULL) {
                    printf("Brak dostępnych powiadomień dla konsumenta %d.\n", packet.sender_id);
                    response.type = ACTION_NACK;
                    snprintf(response.body, MSG_BUFFER_SIZE, "Brak dostępnych powiadomień.");
                } else {
                    // Generowanie listy producentów
                    memset(response.body, 0, MSG_BUFFER_SIZE);
                    generate_producer_list(response.body);
                    response.type = ACTION_SUBSCRIBE_LIST;
                }

                response.queue_id = packet.queue_id;

                if (msgsnd(packet.queue_id, &response, sizeof(response) - sizeof(long), 0) == -1) {
                    perror("Błąd wysyłania listy powiadomień do klienta");
                }
                break;


            case ACTION_SUBSCRIBE:
                printf("Consumer %d subscribed to category %d\n", packet.sender_id, packet.msg_category);
                register_subscriber(packet.sender_id, packet.msg_category, packet.queue_id);
                response.type = ACTION_ACK;
                if (msgsnd(packet.queue_id, &response, sizeof(response) - sizeof(long), 0) == -1) {
                    perror("Error sending acknowledgment for subscription");
                }
                break;
            case ACTION_NOTIFY:
                printf("Notification received from producer %d for category %d: %s\n", 
                    packet.sender_id, packet.msg_category, packet.body);
                distribute_notification(packet.msg_category, packet.body);
                response.type = ACTION_ACK;
                if (msgsnd(packet.queue_id, &response, sizeof(response) - sizeof(long), 0) == -1) {
                    perror("Error sending acknowledgment for notification");
                }
                break;

            default:
                printf("Nieznany typ wiadomości: %ld\n", packet.type);
                response.type = ACTION_NACK;
        }

        // Wysyłanie odpowiedzi
        if (msgsnd(packet.queue_id, &response, sizeof(response) - sizeof(long), 0) == -1) {
            perror("Błąd wysyłania odpowiedzi");
        }
    }

    return 0;
}
