#pragma once
#include <liburing.h>
#include <netinet/in.h>
#include <vector>

#define QUEUE_DEPTH 512
#define BUFFER_SIZE 1024
#define MAX_CONNECTIONS 1024
#define SLOT_SIZE 1024 //Client gets 1kb
#define BUFFER_ALIGN 4096 //Aligns to page sizse for kernal

// Enum to track what kind of request completed
enum class RequestType {
    ACCEPT,
    READ,
    WRITE
};

// Context for every IO operation
struct Request {
    RequestType type;
    int client_fd;
    int buffer_slot_idx; //Slot in the global buffer that belongs to this request

    Request(RequestType t, int fd, int slot=-1) : type(t), client_fd(fd), buffer_slot_idx(slot) {}
};

class Server {
public:
    Server(int port);
    ~Server();

    // Initialize Ring and Socket
    bool setup();
    
    // Start the Event Loop
    void run();

private:
    int port;
    int server_fd;
    struct io_uring ring;

    char* fixed_buffer;//Block of memory;
    std::vector<int>free_slots;//Available slots ids
    
    // Helpers
    void add_accept_request(struct sockaddr_in *client_addr, socklen_t *client_len);
    void add_read_request(int client_fd, int slot_idx);

    void setup_fixed_buffer();
    char* get_slot_addr(int slot_idx);

};