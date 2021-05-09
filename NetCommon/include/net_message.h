#pragma once

#include "net_common.h"

namespace bsl {
    namespace net {
        // Message Header is sent at start of all messages. It has fixed size
        template<typename T>
        struct message_header {
            T id{};
            uint32_t size = 0;
        };

        // Message Body contains a header and a std::vector, containing raw bytes of infomation.
        template<typename T>
        struct message {
            // Header & Body vector
            message_header<T> header{};
            std::vector<uint8_t> body;

            // returns body size of the message
            size_t size() const {
                return body.size();
            }

            // Override for std::cout
            friend std::ostream &operator<<(std::ostream &os, const message<T> &msg) {
                os << "ID:" << int(msg.header.id) << " Size:" << msg.header.size;
                return os;
            }

            // Operator overloads, let the message act like a stack, it can only recieve and output POD-like object because of the template DataType
            // Pushes any POD-like data into the message buffer
            template<typename DataType>
            friend message<T> &operator<<(message<T> &msg, const DataType &data) {
                // Check that the type of the data is standard data type
                static_assert(std::is_standard_layout<DataType>::value, "Data is too complex to be pushed into vector");

                // Use the current body size as a pointer to add data int message type
                size_t i = msg.body.size();

                // Resize the vector
                msg.body.resize(msg.body.size() + sizeof(DataType));

                // Copy the data into the new allocated vector space
                std::memcpy(msg.body.data() + i, &data, sizeof(DataType));

                // Recalculate the message size
                msg.header.size = msg.size();

                // Return the target message
                return msg;
            }

            // Pulls any POD-like data form the message buffer
            template<typename DataType>
            friend message<T> &operator>>(message<T> &msg, DataType &data) {
                // Check that the type of the data is standard data type
                static_assert(std::is_standard_layout<DataType>::value, "Data is too complex to be pulled from vector");

                // Cache the end of the vector where the data start
                size_t i = msg.body.size() - sizeof(DataType);

                // Copy the data from the vector into data
                std::memcpy(&data, msg.body.data() + i, sizeof(DataType));

                // Resize the body vector size
                msg.body.resize(i);

                // Recalculate the message size
                msg.header.size = msg.size();

                // Return the target message so it can be "chained"
                return msg;
            }
        };


        // Owned message add a shared_ptr of connection, the owner is the one who sent the message
        // Forward declare the connection
        template<typename T>
        class connection;

        template<typename T>
        struct owned_message {
            std::shared_ptr<connection<T>> remote = nullptr;
            message<T> msg;

            friend std::ostream &operator<<(std::ostream &os, const owned_message<T> &msg) {
                os << msg.msg;
                return os;
            }
        };

    }
}

