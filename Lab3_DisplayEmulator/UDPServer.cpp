#define WIN32_LEAN_AND_MEAN
#include "UDPServer.h"

#include <iostream>
#include <vector>
#include <cstdint>
#include <stdexcept>

#define DISPLAY_SERVER
#include "DisplayProtocol.cpp"

using namespace std;

extern void executeCommand(const Command& cmd);

UDPServer::UDPServer()
{
    serverSocket = INVALID_SOCKET;
    running = false;
}

UDPServer::~UDPServer()
{
    stop();
}

bool UDPServer::start()
{
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        return false;

    serverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (serverSocket == INVALID_SOCKET)
    {
        WSACleanup();
        return false;
    }

    sockaddr_in serverAddress = {};

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(12345);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (sockaddr*)&serverAddress,
        sizeof(serverAddress)) == SOCKET_ERROR)
    {
        closesocket(serverSocket);
        WSACleanup();
        return false;
    }

    running = true;

    serverThread = thread(&UDPServer::receiveLoop, this);

    cout << "UDP server started on port 12345" << endl;

    return true;
}

void UDPServer::stop()
{
    if (!running)
        return;

    running = false;

    closesocket(serverSocket);

    if (serverThread.joinable())
        serverThread.join();

    WSACleanup();
}

void UDPServer::receiveLoop()
{
    while (running)
    {
        uint8_t buffer[65507];

        sockaddr_in clientAddress = {};
        int clientSize = sizeof(clientAddress);

        int bytesReceived = recvfrom(
            serverSocket,
            (char*)buffer,
            sizeof(buffer),
            0,
            (sockaddr*)&clientAddress,
            &clientSize
        );

        if (bytesReceived == SOCKET_ERROR)
        {
            if (!running)
                break;

            cout << "UDP receive error" << endl;
            continue;
        }

        cout << "\nReceived packet: "
            << bytesReceived << " bytes" << endl;

        try
        {
            vector<uint8_t> packet(
                buffer, buffer + bytesReceived);

            DisplayProtocol protocol(packet);

            vector<Command> commands = protocol.parse();

            for (const Command& cmd : commands)
            {
                cout << "Command: "
                    << (int)cmd.type << endl;

                executeCommand(cmd);
            }
        }
        catch (const exception& e)
        {
            cout << "Protocol error: "
                << e.what() << endl;
        }
    }
}