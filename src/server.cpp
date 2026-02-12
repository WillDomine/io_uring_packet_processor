#include "../include/server.hpp"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>

Server::Server(int p) : port(p), server_fd(-1) {}

Server::~Server() {
    if (server_fd != -1) close(server_fd);
    io_uring_queue_exit(&ring);
};

bool Server::setup() {
    //Set up the socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) 
    {
        perror("Socket Connection Failed");
        return false;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    //Bind to the port
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        perror("Bind failed");
        return false;
    }
    listen(server_fd, 128);

    //Init io_uring queue
    if (io_uring_queue_init(QUEUE_DEPTH, &ring, 0) < 0)
    {
        perror("io_uring init failed");
        return false;
    }

    return true;
    
}

void Server::add_accept_request(struct sockaddr_in *client_address, socklen_t *client_len) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    Request *req = new Request(RequestType::ACCEPT, server_fd);

    io_uring_prep_accept(sqe, server_fd, (struct sockaddr*)client_address, client_len, 0);
    io_uring_sqe_set_data(sqe, req);
}

void Server::add_read_request(int client_fd) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    Request *req = new Request(RequestType::READ, client_fd);

    io_uring_prep_readv(sqe, client_fd, &req->iov, 1, 0);
    io_uring_sqe_set_data(sqe, req);
}

void Server::run() {
    struct sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);

    add_accept_request(&client_addr, &client_len);
    io_uring_submit(&ring);

    std::cout << "Server on port:" << port << std::endl;
    
    while (true)
    {
        struct io_uring_cqe *cqe;
        int ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0)
        {
            std::cerr << "Wait CQE Failed:" << ret << std::endl;
            continue;
        }
        
        Request *req = (Request *)io_uring_cqe_get_data(cqe);
        if (req -> type == RequestType::ACCEPT)
        {
            int new_fd = cqe-> res;
            if (new_fd >= 0)
            {
                std::cout << "New Client: " << new_fd << std::endl;
                add_read_request(new_fd);
                add_accept_request(&client_addr, &client_len);
            }
            
        }
        else if (req->type == RequestType::READ)
        {
            int bytes_read = cqe->res;
            if (bytes_read > 0)
            {
                add_read_request(req->client_fd);
            } else {
                std::cout << "Disconnect: " << req->client_fd << std::endl;
                close(req->client_fd);
            }
            
        }
        
        delete req;
        io_uring_cqe_seen(&ring, cqe);
        io_uring_submit(&ring);
    }
    
    
}