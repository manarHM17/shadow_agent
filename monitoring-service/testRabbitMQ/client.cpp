#include <iostream>
#include <SimpleAmqpClient/SimpleAmqpClient.h>
#include <chrono>
#include <thread>

void sendMessage(int client_id) {
    try {
        // Connexion à RabbitMQ
        AmqpClient::Channel::ptr_t channel = AmqpClient::Channel::Create("localhost");

        // Déclaration de la queue (durable = true pour persistance)
        std::string queue_name = "test_queue";
        channel->DeclareQueue(queue_name, false, true, false, false);

        // Envoi de messages avec l'ID unique
        for (int i = 0; i < 5; ++i) {
            std::string message = "Message du client " + std::to_string(client_id) + " - " + std::to_string(i);
            auto basicMessage = AmqpClient::BasicMessage::Create(message);  // ✅ corriger ici
            channel->BasicPublish("", queue_name, basicMessage);
            std::cout << "[Client-" << client_id << "] Message envoyé: " << message << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } catch (const std::exception &e) {
        std::cerr << "Erreur : " << e.what() << std::endl;
    }
}

int main() {
    int client_id;
    std::cout << "Entrez l'ID du client : ";
    std::cin >> client_id;

    sendMessage(client_id);

    return 0;
}
