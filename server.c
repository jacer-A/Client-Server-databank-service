#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include <stdint.h>
#include "uthash.h"


struct data {
    UT_hash_handle hh;   /* makes this structure hashable */
    uint8_t *value;
    long value_len;
    short key_len;
    uint8_t key[];      // has to be put in the end, which makes the struct "open" (= array is allocated at the assignment)
};

void send_package(int client, uint8_t *buffer, size_t package_len) {
    long nsent = 0;
    while(nsent != package_len){
        int bytes = send(client, buffer + nsent, package_len - nsent, 0);
        if (bytes < 0){
            fprintf(stderr, "Sending data failed!\n");
            break;
        }
        nsent += bytes;
    }
}



int main(int argc, char** argv) {
    if (argc < 2) {
        printf("%s\n", "Not enough args provided!");
        return 1;
    }
    char* port = argv[1];


    struct addrinfo hints;
    struct addrinfo* res;
    struct addrinfo* p;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    int status = getaddrinfo(NULL, port, &hints, &res);
    if (status != 0){
        fprintf(stderr, "%s\n", "getaddrinfo() failed!");
        return 1;
    }
    // server socket translated !


    int s = -1;
    for(p = res; p != NULL;p = p->ai_next){
        s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (s == -1){
            continue;
        }
        int optval = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));
        //
        status = bind(s, res->ai_addr, res->ai_addrlen);
        if (status != 0){
           close(s);
           continue;
        }
        break;
    }
    freeaddrinfo(res);
    if (s == -1){
        fprintf(stderr, "%s\n", "Unable to create socket!");
        return 1;
    }
    if (status != 0){
        fprintf(stderr, "%s\n", "Failed to bind socket!");
        return 1;
    }
    // server socket created and bound !


    status = listen(s, 1);
    if (status != 0){
        fprintf(stderr, "Listen failed!\n");
        return 1;
    }
    // server started listening !


    struct data *datas= NULL;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
    while(true) {
        struct sockaddr_storage client_addr;
        socklen_t c_addr_size = sizeof(client_addr);
        printf("\n=====================================================\n");
        printf("Database has %d items\n", HASH_COUNT(datas));
        printf("waiting for connections..\n");
        int client = accept(s, (struct sockaddr*) &client_addr, &c_addr_size); // blocking call
        fprintf(stderr, "%s\n", "Client accepted!");


        uint8_t *msg= NULL;
        size_t msg_len= 0;
        uint8_t request= 0;
        short key_len= 0;
        long value_len= 0;
        do {
            int nbytes= 0;
            uint8_t buffer[512];
            nbytes = recv(client, buffer, 512, 0);
            if (nbytes == 0) {
                break;
            }

            msg= (uint8_t *) realloc(msg, (msg_len + nbytes) * sizeof(uint8_t));

            memcpy(msg + msg_len, buffer, nbytes);
            msg_len += nbytes;

            if (msg_len >= 7) {     // means that we received at the full header, so we can translate
                request= msg[0];
                key_len= msg[1] << 8 | msg[2];
                value_len= msg[3] << 24 | msg[4] << 16 | msg[5] << 8 | msg[6];
            }
        } while (msg_len != 7 + key_len + value_len);


        printf("-------------------------------\n");
        printf("request= %d\nkey_len= %d\nvalue_len=%ld\n", request, key_len, value_len);
        printf("key= ");
        fwrite(msg+7, sizeof(uint8_t), key_len, stdout);
        printf("\nvalue= ");
        fwrite(msg+7+key_len, sizeof(uint8_t), value_len, stdout);
        printf("\n-------------------------------\n");


        struct data *data;

        // Set() item
        if (request == 2) {
            uint8_t *value= (uint8_t *) malloc(value_len);
            memcpy(value, msg+7+key_len, value_len);

            struct data *existent;
            HASH_FIND(hh, datas, msg+7, key_len, existent);
            if (existent) {
                HASH_DEL(datas, existent);
                free(existent->value);
                printf("--- Key exists already ! updating value..\n");

                data= existent;
            } else {
                data= (struct data*) calloc(1, sizeof(struct data) + key_len);  // setting to 0 is really important here
                memcpy(data-> key, msg+7, key_len);
            }
            data-> value= value;
            data-> value_len= value_len;
            data->key_len= key_len;

            HASH_ADD(hh, datas, key, key_len, data);

            uint8_t header[7];
            memset(header, 0, 7);
            header[0] = request + 8;   // Acknowledgement
            send_package(client, header, 7);
            printf("==> Item set !\n");
        }

        // Get() item
        if (request == 4) {
            HASH_FIND(hh, datas, msg+7, key_len, data);

            uint8_t header[7];
            header[0] = request;
            if (data) {
                header[0] += 8;   // Acknowledgement
                header[1] = key_len >> 8;
                header[2] = key_len;
                header[3] = data->value_len >> 24;
                header[4] = data->value_len >> 16;
                header[5] = data->value_len >> 8;
                header[6] = data->value_len;
                send_package(client, header, 7);
                send_package(client, data->key, key_len);
                send_package(client, data->value, data->value_len);
                printf("==> Item sent !\n");
            } else {
                memset(header+1, 0, 6);
                send_package(client, header, 7);
                printf("==> Item not found !\n");
            }
        }

        // Delete() item
        if (request == 1) {
            HASH_FIND(hh, datas, msg+7, key_len, data);

            uint8_t header[7];
            header[0] = request;
            memset(header+1, 0, 6);
            if (data) {
                printf("--- Item Found\n");
                HASH_DEL(datas, data);
                header[0] += 8;   // Acknowledgement
                free(data->value);
                free(data);
            }
            send_package(client, header, 7);

            printf("==> Item deleted or didn't exist!\n");
        }

        free(msg);

        close(client);
    }
#pragma clang diagnostic pop

    return 0;
}
