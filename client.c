#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include <stdint.h>



void send_package(int server, uint8_t *buffer, size_t package_len) {
    long nsent = 0;
    while(nsent != package_len){
        int bytes = send(server, buffer + nsent, package_len - nsent, 0);
        if (bytes < 0){
            fprintf(stderr, "Sending data failed!\n");
            break;
        }
        nsent += bytes;
    }
}



int main(int argc, char** argv) {
    if (argc < 5){
        fprintf(stderr, "please provide the parameters: 1/DNS-name 2/port-number 3/request 4/key (and if needed: '<' for stdin OR '>' for stdout)\n");
        return 1;
    }
    // stdin and stdout are assumed valid
    char* host = argv[1];
    char* service = argv[2];
    char* request = argv[3];
    char* key = argv[4];


    uint8_t header[7];
    memset(header, 0, 7);
    if (strcmp(request, "SET") == 0) {
        header[0]= 2;
    }
    else if (strcmp(request, "GET") == 0) {
        header[0]= 4;
    }
    else if (strcmp(request, "DELETE") == 0) {
        header[0]= 1;
    } else {
        fprintf(stderr, "Unknown method\n");
        return 1;
    }


    short key_len= strlen(key);
            // the server database can handle a key of any datatype but
            // this client can only send keys of type string because
            // a command line argument can only be a string
    header[1]= key_len >> 8;
    header[2]= key_len;


    // if Set()
    long value_len = 0;
    uint8_t *value;
    if (header[0] == 2) {
        uint8_t x;
        while (fscanf(stdin, "%c", &x) != EOF) {
            value_len++;
        }
        rewind(stdin);

        header[3] = value_len >> 24;
        header[4] = value_len >> 16;
        header[5] = value_len >> 8;
        header[6] = value_len;

        value= (uint8_t *) malloc(value_len);
        for (int i=0; fscanf(stdin, "%c", &x) != EOF; i++) {
            value[i]= x;
        }
    }


    struct addrinfo hints;
    struct addrinfo* res;
    struct addrinfo* p;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    int status = getaddrinfo(host, service, &hints, &res);
    if (status != 0){
        fprintf(stderr, "%s\n", "getaddrinfo() failed!");
        return 1;
    }
    // client socket translated !

    int s = -1;
    for(p = res; p != NULL;p = p->ai_next){
        s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (s == -1){
            continue;
        }
        //
        status = connect(s, res->ai_addr, res->ai_addrlen);
        if (status != 0){
           fprintf(stderr, "%s\n", "connect() failed!");
           continue;
        }
        break;
    }
    freeaddrinfo(res);
    if (s == -1){
        fprintf(stderr, "%s\n", "socket() failed!");
        return 1;
    }
    if(status != 0) return 1;
    // client socket created and connected !


    // send msg
    send_package(s, header, 7);
    send_package(s, key, key_len);
    if (header[0]==2) {
        send_package(s, value, value_len);
        free(value);
    }


    // receive msg
    uint8_t msg[7], *msg_get = NULL; // just because msg_get needs dynamic allocation
    size_t nreceived= 0;
    while(true) {
        int nbytes = 0;
        uint8_t buffer[512];
        nbytes = recv(s, buffer, 512, 0);
        if (nbytes == 0){
            break;
        }

        if ( header[0] != 4) {
            memcpy(msg + nreceived, buffer, nbytes);
        }
        else {
            msg_get= (uint8_t *) realloc(msg_get, (nreceived + nbytes) * sizeof(uint8_t));
            memcpy(msg_get + nreceived, buffer, nbytes);
        }
        nreceived += nbytes;

    }   // recv() is a blocking call but we assume the server would close the connection, so recv() returns


    /*
        TODO: decode msg[0:7] and print it
    */


   // print the response to Get() request
    if (header[0] == 4) {
        if  (nreceived > 7) {
            unsigned long written = 0;
            while (written != nreceived - 7 - key_len) {    // written != value_len
                unsigned long n = fwrite(msg_get + 7 + key_len + written, sizeof(uint8_t),
                                         nreceived - 7 - key_len - written, stdout);
                if (n == 0) {
                    fprintf(stderr, "%s\n", "Write to stdout failed!");
                    return 1;
                }
                written += n;
            }
        }
        free(msg_get);
    }


    return 0;
}