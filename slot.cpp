#include "slot.h"

Slot::Slot(int _id) {
    id = _id;
    ocupado = false;
    apartamento = 0;
    insercao = {0, 0, 0, 0, 0, 0};
    retirada = {0, 0, 0, 0, 0, 0};
    
}

Slot::~Slot() {
    // Destrutor vazio, pois não há alocação dinâmica
}

Time Slot::getInsercao() {
    return insercao;
}

void Slot::setInsercao(Time _insercao) {
    insercao = _insercao;
}

Time Slot::getRetirada() {
    return retirada;
}

void Slot::setRetirada(Time _retirada) {
    retirada = _retirada;
}

bool Slot::isOcupado() {
    return ocupado;
}

void Slot::setOcupado(bool _ocupado) {
    ocupado = _ocupado;
}

int Slot::getApartamento() {
    return apartamento;
}

void Slot::setApartamento(int _apartamento) {
    if (_apartamento ==    0 ||
        _apartamento ==  101 ||
        _apartamento ==  201 ||
        _apartamento ==  301 ||
        _apartamento ==  401 ||
        _apartamento ==  501 ||
        _apartamento ==  601 ||
        _apartamento ==  701 ||
        _apartamento ==  801 ||
        _apartamento ==  901 ||
        _apartamento == 1001 ||
        _apartamento == 1101 ||
        _apartamento == 1201 ||
        _apartamento == 1301 ||
        _apartamento == 1401) {

        apartamento = _apartamento;
    }
}

int Slot::getId() {
    return id;
}

void Slot::setId(int _id) {
    if (_id > 0) {
        id = _id;
    }
}

char Slot::getTamanho() {
    return tamanho;
}

void Slot::setTamanho(char _tamanho) {
    if (_tamanho == 'P' || _tamanho == 'M' || _tamanho == 'G') {
        tamanho = _tamanho;
    }
}