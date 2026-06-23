#include <iostream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ostream>
#include <sys/socket.h>
#include <unistd.h>

int main() {
    std::cout << "I will be serving soon." << std::endl;

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in address;

    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_family = AF_INET;
    address.sin_port = htons(8080);

    int bindRes = bind(serverSocket, (struct sockaddr *)&address, sizeof(address));
    if (bindRes == -1) {
        std::cerr << "Error while binding socket." << std::endl;
        return -1;
    }

    listen(serverSocket, 5);
    int clientSocket = accept(serverSocket, nullptr, nullptr);
    char buffer[1024] = {0};
    recv(clientSocket, buffer, sizeof(buffer), 0);
    std::cout << "Message from client: " << buffer << std::endl;

    close(serverSocket);
    return 0;
}
