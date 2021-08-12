# Client-Server-databank-service
Server offers a key-value-databank that a Client can manage with the orders: Set(), Get() and Delete().

- Protocol (based on TCP):

![image](https://user-images.githubusercontent.com/83579009/129104311-7fe78077-df06-4a8c-a537-7a6acb6fa893.jpg)

- A fixed length header (7 bytes) and optionally a key with variable length and a value with variable length. Length of the key is saved in a 16bit unsigned integer, length of the value is saved in a 32bit unsigned integer
- Get() and Delete() requests contain only a Key, so the length of Value is set to 0. Responses to Get() requests contain Key and Value, the other responses contain neither key nor Value.
- As parameter the server should get the port number. Example:
    ./server 4999
- The client should be passed the DNS-name or IP address of the server, the port,  the method (SET, GET, DELETE) and the key as command line arguments. Examples:
    ./client localhost 4999 SET selfie < selfie.jpg
- The database is able to work with any type of data as key or value
