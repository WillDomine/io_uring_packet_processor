#include <liburing.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <vector>

#define PORT 9000
#define QUEUE_DEPTH 256
#define BUFFER_SIZE 1024

enum EventType {
    EVENT_ACCEPT,
    EVENT_READ
};

struct Request {
    EventType type;
    int client_fd;
    std::vector<char> buffer;
    struct iovec iov;

    Request(EventType t, int fd) : type(t), client_fd(fd) {
        buffer.resize(BUFFER_SIZE);
        iov.iov_base = buffer.data();
        iov.iov_len = buffer.size();
    };
};

void add_read_request(struct io_uring *ring, int client_fd) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    Request *req = new Request(EVENT_READ, client_fd);
    io_uring_prep_readv(sqe, client_fd, &req->iov, 1, 0);
    io_uring_sqe_set_data(sqe, req);
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("Socket failed"); return 1; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Bind failed"); return 1;
    }
    listen(server_fd, 128);

    struct io_uring ring;
    if (io_uring_queue_init(QUEUE_DEPTH, &ring, 0) < 0) {
        perror("io_uring init failed"); return 1;
    }

    struct sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);

    add_accept_request(&ring, server_fd, &client_addr, &client_len);
    io_uring_submit(&ring);

    std::cout << "Running on Port: " << PORT << std::endl;

    while (true) {
        struct io_uring_cqe *cqe;
        
        int ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0) { perror("Wait error"); continue; }

        Request *req = (Request *)io_uring_cqe_get_data(cqe);

        if (req->type == EVENT_ACCEPT) {
            int client_sock = cqe->res;
            if (client_sock >= 0) {
                std::cout << "New Connection: " << client_sock << std::endl;
                add_read_request(&ring, client_sock);
                
                add_accept_request(&ring, server_fd, &client_addr, &client_len);
            }
        } 
        else if (req->type == EVENT_READ) {
            int bytes_read = cqe->res;
            if (bytes_read > 0) {
                add_read_request(&ring, req->client_fd);
            } else {
                std::cout << "Disconnect: " << req->client_fd << std::endl;
                close(req->client_fd);
            }
        }

        delete req;
        io_uring_cqe_seen(&ring, cqe);
        
        io_uring_submit(&ring);
    }

    close(server_fd);
    io_uring_queue_exit(&ring);
    return 0;
}