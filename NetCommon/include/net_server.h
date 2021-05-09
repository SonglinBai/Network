#pragma once

#include "net_common.h"
#include "net_tsqueue.h"
#include "net_message.h"
#include "net_connection.h"

namespace bsl {
    namespace net {
        template<typename T>
        class server_interface {
        public:
            // Create a server, ready to listen on specific port
            server_interface(uint16_t port)
                    : m_asioAcceptor(m_asioContext, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)) {

            }

            virtual ~server_interface() {
                Stop();
            }

            // Starts the server
            bool Start() {
                try {
                    // Prime the asio context to do some work, because this is a server, so it should wait client connection
                    WaitForClientConnection();

                    // Run context in it's thread
                    m_threadContext = std::thread([this]() { m_asioContext.run(); });
                }
                catch (std::exception &e) {
                    std::cerr << "[SERVER] Exception: " << e.what() << "\n";
                    return false;
                }

                std::cout << "[SERVER] Started!\n";
                return true;
            }

            // Stops the server!
            void Stop() {
                // Request the context to close
                m_asioContext.stop();

                // Clean up the context thread
                if (m_threadContext.joinable()) m_threadContext.join();

                std::cout << "[SERVER] Stopped!\n";
            }

            // ASYNC - Instruct asio to wait for connection
            void WaitForClientConnection() {
                // Prime context with an instruction to wait until a socket connects. It will provide a unique socket for each incoming connection
                m_asioAcceptor.async_accept(
                        [this](std::error_code ec, asio::ip::tcp::socket socket) {
                            if (!ec) {
                                std::cout << "[SERVER] New Connection: " << socket.remote_endpoint() << "\n";

                                // Create a new connection to handle this client
                                std::shared_ptr<connection<T>> newconn =
                                        std::make_shared<connection<T>>(connection<T>::owner::server,
                                                                        m_asioContext, std::move(socket),
                                                                        m_qMessagesIn);



                                // OnClientConnect function will return bool
                                if (OnClientConnect(newconn)) {
                                    // Connection allowed, so add to connection container
                                    m_deqConnections.push_back(std::move(newconn));

                                    // Set the asio context to read of the header from the client
                                    m_deqConnections.back()->ConnectToClient(nIDCounter++);

                                    std::cout << "[" << m_deqConnections.back()->GetID() << "] Connection Approved\n";
                                } else {
                                    std::cout << "[-----] Connection Denied\n";
                                }
                            } else {
                                std::cout << "[SERVER] New Connection Error: " << ec.message() << "\n";
                            }

                            // Prime the asio context to wait for client connection agine
                            WaitForClientConnection();
                        });
            }

            // Send a message to a specific client
            void MessageClient(std::shared_ptr<connection<T>> client, const message<T> &msg) {
                // Check client is valid
                if (client && client->IsConnected()) {
                    client->Send(msg);
                } else {
                    // If the client is invalid, means that we can't communicate with it, so we need to disconnect it
                    OnClientDisconnect(client);
                    client.reset();

                    // Then remove it from the container
                    m_deqConnections.erase(
                            std::remove(m_deqConnections.begin(), m_deqConnections.end(), client),
                            m_deqConnections.end());
                }
            }

            // Send message to all clients
            void MessageAllClients(const message<T> &msg, std::shared_ptr<connection<T>> pIgnoreClient = nullptr) {
                bool bInvalidClientExists = false;

                // Iterate through all clients in container
                for (auto &client : m_deqConnections) {
                    // Check client is connected
                    if (client && client->IsConnected()) {
                        if (client != pIgnoreClient)
                            client->Send(msg);
                    } else {
                        // We can't communicate with the client, disconnect it
                        OnClientDisconnect(client);
                        client.reset();

                        bInvalidClientExists = true;
                    }
                }

                // Because remove item from queue cost too much resources, so we deleted dead clients at once
                // The connection in connection queue is shared pointer, if it is invalid means it is nullptr, so we just need to remove nullptr item from queue
                if (bInvalidClientExists)
                    m_deqConnections.erase(
                            std::remove(m_deqConnections.begin(), m_deqConnections.end(), nullptr),
                            m_deqConnections.end());
            }

            // Force server to respond to incoming messages
            void Update(size_t nMaxMessages = -1, bool bWait = false) {
                if (bWait) m_qMessagesIn.wait();

                // Process as many messages
                size_t nMessageCount = 0;
                while (nMessageCount < nMaxMessages && !m_qMessagesIn.empty()) {
                    // Grab the front message
                    auto msg = m_qMessagesIn.pop_front();

                    // Pass to message handler
                    OnMessage(msg.remote, msg.msg);

                    nMessageCount++;
                }
            }

        protected:
            // Called when a client want to connect, return true means that accept this client
            virtual bool OnClientConnect(std::shared_ptr<connection<T>> client) {
                return true;
            }

            // Called when a client appears to have disconnected
            virtual void OnClientDisconnect(std::shared_ptr<connection<T>> client) {

            }

            // Called when a message arrives
            virtual void OnMessage(std::shared_ptr<connection<T>> client, message<T> &msg) {

            }


        protected:
            // Thread Safe Queue for incoming message packets
            tsqueue<owned_message<T>> m_qMessagesIn;

            // Container of active validated connections
            std::deque<std::shared_ptr<connection<T>>> m_deqConnections;

            // Asio context and thread that run the context
            asio::io_context m_asioContext;
            std::thread m_threadContext;

            // Acceptor handles new incoming connection
            asio::ip::tcp::acceptor m_asioAcceptor;

            // Clients will be identified by this ID
            uint32_t nIDCounter = 10000;
        };
    }
}