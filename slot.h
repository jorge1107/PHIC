#ifndef SLOT_H
#define SLOT_H

struct Time {
    unsigned int ano;
    unsigned int mes;
    unsigned int dia;
    unsigned int hora;
    unsigned int minuto;
    unsigned int segundo;
};

class Slot {
private:
    Time insercao; // Data e hora de inserção do slot
    Time retirada; // Data e hora de retirada do slot
    int id; // Identificador do slot
    int apartamento; // Número do apartamento associado ao slot
    char tamanho; // Tamanho do slot (P, M, G)
    bool ocupado; // Indica se o slot está ocupado ou não
public:
    Slot(int _id); // Construtor da classe
    ~Slot(); // Destrutor da classe
    Time getInsercao(); // Retorna a data e hora de inserção do slot
    void setInsercao(int horario); // Define a data e hora de inserção do slot
    Time getRetirada(); // Retorna a data e hora de retirada do slot
    void setRetirada(int horario); // Define a data e hora de retirada do slot
    int getApartamento(); // Retorna o número do apartamento associado ao slot
    void setApartamento(int _apartamento); // Define o número do apartamento associado ao slot
    int getId(); // Retorna o identificador do slot
    void setId(int _id); // Define o identificador do slot
    char getTamanho(); // Retorna o tamanho do slot
    void setTamanho(char _tamanho); // Define o tamanho do slot
    bool isOcupado(); // Retorna true se o slot estiver ocupado, false caso contrário
    void setOcupado(bool _ocupado); // Define se o slot está ocupado ou não
};

// ID
// Horários de chegada da encomenda (inserção) e de retirada da encomenda (retirada)
// Número do apartamento associado ao slot
// Tamanho do slot (P, M, G)
// Indica se o slot está ocupado ou não

#endif