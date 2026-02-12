#include "../examples/server.hpp"
#include "../include/packet.hpp"
#include "../include/filter.hpp"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <cstdlib>

Server::Server(int p) : port(p), server_fd(-1), fixed_buffer(nullptr) {}

Server::~Server() {
    if (server_fd != -1) close(server_fd);
    if (fixed_buffer) free(fixed_buffer);
    io_uring_queue_exit(&ring);
};

void Server::setup_fixed_buffer() {
    size_t total_size = MAX_CONNECTIONS * SLOT_SIZE;
    if (posix_memalign((void**)&fixed_buffer, BUFFER_ALIGN, total_size))
    {
        perror("Aligned allocation failed");
        exit(1);
    }
    memset(fixed_buffer, 0, total_size);
    
    struct iovec iov;
    iov.iov_base = fixed_buffer;
    iov.iov_len = total_size;

    int ret = io_uring_register_buffers(&ring, &iov, 1);
    if(ret < 0) {
        std::cerr << "Buffer Registration failed: " <<ret << std::endl;
        exit(1);
    }

    for (int i = 0; i < MAX_CONNECTIONS; i++)
    {
        free_slots.push_back(i);
    }

    std::cout << "Fixed Buffer Memory Size: " << (total_size/1024) << "KB" << std::endl;
}

char* Server::get_slot_addr(int slot_idx) {
    return fixed_buffer + (slot_idx * SLOT_SIZE);
}

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

    struct io_uring_params params;
    memset(&params, 0, sizeof(params));
    params.flags |= IORING_SETUP_SQPOLL;
    params.sq_thread_idle = 2;

    //Init io_uring queue
    if (io_uring_queue_init_params(QUEUE_DEPTH, &ring, &params) < 0)
    {
        perror("io_uring init failed");
        return false;
    }

    setup_fixed_buffer();
    return true;
    
}

void Server::add_accept_request(struct sockaddr_in *client_address, socklen_t *client_len) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    Request *req = new Request(RequestType::ACCEPT, server_fd);

    io_uring_prep_accept(sqe, server_fd, (struct sockaddr*)client_address, client_len, 0);
    io_uring_sqe_set_data(sqe, req);
}

void Server::add_read_request(int client_fd, int slot_idx) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    Request *req = new Request(RequestType::READ, client_fd, slot_idx);

    io_uring_prep_read_fixed(sqe, client_fd, get_slot_addr(slot_idx), SLOT_SIZE, 0, 0);
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
        //Makes loop non blocking instead of waiting
        int ret = io_uring_peek_cqe(&ring, &cqe);
        if (ret == -EAGAIN)
        {
            continue;
        }
        else if (ret < 0)
        {
            std::cerr << "CQE Error:" << ret << std::endl;
            exit(1);
        }
        
        Request *req = (Request *)io_uring_cqe_get_data(cqe);
        if (req -> type == RequestType::ACCEPT)
        {
            int new_fd = cqe->res;
            if (new_fd >= 0)
            {
                if (!free_slots.empty())
                {
                    int slot = free_slots.back();
                    free_slots.pop_back();
                    std::cout << "New Client: " << new_fd << std::endl;
                    add_read_request(new_fd, slot);
                } else {
                    close(new_fd); //Server is full
                }
                add_accept_request(&client_addr, &client_len);
            }
            
        }
        else if (req->type == RequestType::READ)
        {
            int bytes_read = cqe->res;
            int slot = req->buffer_slot_idx;
            if (bytes_read > 0)
            {
                Packet* pkt = (Packet*)get_slot_addr(slot);
                uint8_t mask = filter_batch_8_avx2(pkt);
                bool drop_me = (mask & 1);

                if (drop_me)
                {
                    std::cout << "AVX2 dropped the packet" << std::endl;
                } else {
                    std::cout << "Packet is valid: " << bytes_read << std::endl;
                }

                add_read_request(req->client_fd, slot);
                
            } else {
                std::cout << "Disconnect: " << req->client_fd << std::endl;
                close(req->client_fd);
                free_slots.push_back(slot);
            }
            
        }
        
        delete req;
        io_uring_cqe_seen(&ring, cqe);
        io_uring_submit(&ring);
    }
    
    
}