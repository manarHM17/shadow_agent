#include <amqp.h>
#include <amqp_tcp_socket.h>
#include <iostream>
#include "monitoring.grpc.pb.h"

using namespace std;
using namespace monitoring;

void consumeFromRabbitMQ() {
    const string hostname = "localhost";
    const int port = 5672;
    const string queueName = "monitoring_queue";

    // Connexion à RabbitMQ
    amqp_connection_state_t conn = amqp_new_connection();
    amqp_socket_t* socket = amqp_tcp_socket_new(conn);
    if (!socket) {
        throw runtime_error("Failed to create TCP socket");
    }

    if (amqp_socket_open(socket, hostname.c_str(), port)) {
        throw runtime_error("Failed to open TCP socket");
    }

    amqp_rpc_reply_t loginReply = amqp_login(conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, "guest", "guest");
    if (loginReply.reply_type != AMQP_RESPONSE_NORMAL) {
        throw runtime_error("Failed to login to RabbitMQ");
    }

    amqp_channel_open(conn, 1);
    if (amqp_get_rpc_reply(conn).reply_type != AMQP_RESPONSE_NORMAL) {
        throw runtime_error("Failed to open channel");
    }

    amqp_basic_consume(conn, 1, amqp_cstring_bytes(queueName.c_str()), amqp_empty_bytes, 0, 1, 0, amqp_empty_table);
    if (amqp_get_rpc_reply(conn).reply_type != AMQP_RESPONSE_NORMAL) {
        throw runtime_error("Failed to start consuming");
    }

    while (true) {
        amqp_rpc_reply_t res;
        amqp_envelope_t envelope;

        amqp_maybe_release_buffers(conn);
        res = amqp_consume_message(conn, &envelope, nullptr, 0);

        if (res.reply_type == AMQP_RESPONSE_NORMAL) {
            string message((char*)envelope.message.body.bytes, envelope.message.body.len);

            // Désérialiser le message Protobuf
            MonitoringData monitoringData;
            if (monitoringData.ParseFromString(message)) {
                cout << "Received data for device: " << monitoringData.device_id() << endl;
                // Insérer les données dans la base de données ici
            } else {
                cerr << "Failed to parse Protobuf message" << endl;
            }

            amqp_destroy_envelope(&envelope);
        } else {
            cerr << "Failed to consume message" << endl;
        }
    }

    amqp_channel_close(conn, 1, AMQP_REPLY_SUCCESS);
    amqp_connection_close(conn, AMQP_REPLY_SUCCESS);
    amqp_destroy_connection(conn);
}

int main() {
    try {
        consumeFromRabbitMQ();
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
    }

    return 0;
}