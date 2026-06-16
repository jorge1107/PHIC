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