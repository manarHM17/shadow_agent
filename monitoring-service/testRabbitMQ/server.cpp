#include <iostream>
#include <SimpleAmqpClient/SimpleAmqpClient.h>
#include <thread>

void consumeMessages() {
    try {
        // Connexion à RabbitMQ
        AmqpClient::Channel::ptr_t channel = AmqpClient::Channel::Create("localhost");

        // Déclaration de la queue à consommer
        std::string queue_name = "test_queue";
        channel->DeclareQueue(queue_name, false, true, false, false);

        // Démarrer la consommation
        std::string consumer_tag = channel->BasicConsume(queue_name);

        std::cout << "Serveur en attente de messages...\n";

        // Consommer des messages en boucle
        while (true) {
            AmqpClient::Envelope::ptr_t envelope;
            bool message_received = channel->BasicConsumeMessage(consumer_tag, envelope, 500);
            
            if (message_received) {
                std::string msg = envelope->Message()->Body();
                std::cout << "[Serveur] Message reçu: " << msg << std::endl;
            }
        }
    } catch (const std::exception &e) {
        std::cerr << "Erreur : " << e.what() << std::endl;
    }
}

int main() {
    // Lancer le serveur dans un thread
    std::thread t(consumeMessages);
    t.join();
    
    return 0;
}
