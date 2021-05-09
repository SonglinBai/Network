#pragma once

#include "net_common.h"
#include "net_tsqueue.h"
#include "net_message.h"


namespace bsl {
    namespace net {
        template<typename T>
        class connection : public std::enable_shared_from_this<connection<T>> {
        public:
            // A connection is owned by server
            enum class owner {
                server,
                client
            };

        public:
            // Constructor: Specify Owner, connect to context, transfer the socket, incoming message queue
            connection(owner parent, asio::io_context &asioContext, asio::ip::tcp::socket socket,
                       tsqueue <owned_message<T>> &qIn)
                    : m_asioContext(asioContext), m_socket(std::move(socket)), m_qMessagesIn(qIn) {
                m_nOwnerType = parent;
            }

            virtual ~connection() {}

            // This ID is used system wide
            uint32_t GetID() const {
                return id;
            }

        public:
            void ConnectToClient(uint32_t uid = 0) {
                if (m_nOwnerType == owner::server) {
                    if (m_socket.is_open()) {
                        id = uid;
                        ReadHeader();
                    }
                }
            }

            void ConnectToServer(const asio::ip::tcp::resolver::results_type &endpoints) {
                // Only clients can connect to servers
                if (m_nOwnerType == owner::client) {
                    // Request asio attempts to connect to an endpoint
                    asio::async_connect(m_socket, endpoints,
                                        [this](std::error_code ec, asio::ip::tcp::endpoint endpoint) {
                                            if (!ec) {
                                                ReadHeader();
                                            }
                                        });
                }
            }


            void Disconnect() {
                if (IsConnected())
                    asio::post(m_asioContext, [this]() { m_socket.close(); });
            }

            bool IsConnected() const {
                return m_socket.is_open();
            }

            // Prime the connection to wait for incoming messages
            void StartListening() {

            }

        public:
            // ASYNC - Send a message, connections are one-to-one so no need to specifiy
            // the target, for a client, the target is the server and vice versa
            void Send(const message <T> &msg) {
                asio::post(m_asioContext,
                           [this, msg]() {
                               bool bWritingMessage = !m_qMessagesOut.empty();
                               m_qMessagesOut.push_back(msg);
                               if (!bWritingMessage) {
                                   WriteHeader();
                               }
                           });
            }


        private:
            // ASYNC - Prime context to write a message header
            void WriteHeader() {
                asio::async_write(m_socket, asio::buffer(&m_qMessagesOut.front().header, sizeof(message_header<T>)),
                                  [this](std::error_code ec, std::size_t length) {
                                      if (!ec) {
                                          // Check if the message also have a message body
                                          if (m_qMessagesOut.front().body.size() > 0) {
                                              // If it is, then write the body too.
                                              WriteBody();
                                          } else {
                                              // It didnt, so we are done with this message. Remove it from th out message queue
                                              m_qMessagesOut.pop_front();

                                              // If the queue is not empty, there are more messages to send
                                              if (!m_qMessagesOut.empty()) {
                                                  WriteHeader();
                                              }
                                          }
                                      } else {
                                          std::cout << "[" << id << "] Write Header Fail.\n";
                                          m_socket.close();
                                      }
                                  });
            }

            // ASYNC - Prime context to write a message body
            void WriteBody() {
                // If this function is called, a header has just been sent
                asio::async_write(m_socket,
                                  asio::buffer(m_qMessagesOut.front().body.data(), m_qMessagesOut.front().body.size()),
                                  [this](std::error_code ec, std::size_t length) {
                                      if (!ec) {
                                          // Sending was successful, so we are done with the message
                                          m_qMessagesOut.pop_front();

                                          // If the queue is not empty, there are more messages to send
                                          if (!m_qMessagesOut.empty()) {
                                              WriteHeader();
                                          }
                                      } else {
                                          std::cout << "[" << id << "] Write Body Fail.\n";
                                          m_socket.close();
                                      }
                                  });
            }

            // ASYNC - Prime context ready to read a message header
            void ReadHeader() {
                // Because this function is asynchronized, so we need a temporary message to get full of the message
                asio::async_read(m_socket, asio::buffer(&m_msgTemporaryIn.header, sizeof(message_header<T>)),
                                 [this](std::error_code ec, std::size_t length) {
                                     if (!ec) {
                                         // A complete message header has been read, check if this message has a body
                                         if (m_msgTemporaryIn.header.size > 0) {
                                             // If it does, so allocate enough space in the messages' body, and tell asio context to read body
                                             m_msgTemporaryIn.body.resize(m_msgTemporaryIn.header.size);
                                             ReadBody();
                                         } else {
                                             // If it doesn't, so add this message to incoming queue
                                             AddToIncomingMessageQueue();
                                         }
                                     } else {
                                         std::cout << "[" << id << "] Read Header Fail.\n";
                                         m_socket.close();
                                     }
                                 });
            }

            // ASYNC - Prime context ready to read a message body
            void ReadBody() {
                // If this function is called, a header has already been read, and allocate enough space to store the body
                asio::async_read(m_socket, asio::buffer(m_msgTemporaryIn.body.data(), m_msgTemporaryIn.body.size()),
                                 [this](std::error_code ec, std::size_t length) {
                                     if (!ec) {
                                         // The message is complete now, just add it to the incoming message queue
                                         AddToIncomingMessageQueue();
                                     } else {
                                         std::cout << "[" << id << "] Read Body Fail.\n";
                                         m_socket.close();
                                     }
                                 });
            }

            // When a full message is arrived, call this function
            void AddToIncomingMessageQueue() {
                // Push the temporary message to the message queue and add owner information to the message
                if (m_nOwnerType == owner::server)
                    m_qMessagesIn.push_back({this->shared_from_this(), m_msgTemporaryIn});
                else
                    m_qMessagesIn.push_back({nullptr, m_msgTemporaryIn});

                // Prime the asio context to read another header
                ReadHeader();
            }

        protected:
            // Each connection has a unique socket to a remote
            asio::ip::tcp::socket m_socket;

            // This context is shared with the whole asio instance
            asio::io_context &m_asioContext;

            // This queue holds all messages to be sent to the remote side
            tsqueue <message<T>> m_qMessagesOut;

            // This references the incoming queue
            tsqueue <owned_message<T>> &m_qMessagesIn;

            // Incoming messages are constructed asynchronously, so we will store the part assembled message here, until it is ready
            message <T> m_msgTemporaryIn;

            // The owner of the connetion
            owner m_nOwnerType = owner::server;

            uint32_t id = 0;

        };
    }
}