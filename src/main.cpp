#include <iostream>
#include "../include/server.hpp"
#include "../include/filter.hpp"


int main() {
    Server server(9090);

    if (!server.setup())
    {
        std::cerr << "Could not set up server" << std::endl;
        return 1;
    }

    server.run();

    return 0;
}