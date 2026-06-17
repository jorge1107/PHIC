#include "slot.h"
#include <iostream>

Slot::Slot(int _id) {
    id = _id;
    ocupado = false;
    apartamento = 0;
}

Slot::~Slot() {
    // Destrutor vazio, pois não há alocação dinâmica
}

Time Slot::getInsercao() {
    return insercao;
}

void Slot::setInsercao(int horario) {
    insercao.segundo = horario % 60;
}

Time Slot::getRetirada() {
    return retirada;
}

void Slot::setRetirada(int horario) {
    retirada.segundo = horario % 60;
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
    apartamento = _apartamento;
}

int Slot::getId() {
    return id;
}

void Slot::setId(int _id) {
    id = _id;
}