#pragma once

#include <winsock2.h>
#include <thread>
#include <atomic>

class UDPServer
{
private:
    SOCKET serverSocket;
    std::thread serverThread;
    std::atomic<bool> running;

    void receiveLoop();

public:
    UDPServer();
    ~UDPServer();

    bool start();
    void stop();
};
