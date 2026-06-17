#include "slot.h"
#include <iostream>
#include <vector>

int main() {
    std::vector<Slot> listaDeSlots;

    for (int i = 1; i <= 12; i++) {
        Slot slotTemporario(i);

        if (i < 5) {
            slotTemporario.setTamanho('P');
        }else if(i < 9) {
            slotTemporario.setTamanho('M');
        }else{
            slotTemporario.setTamanho('G');
        }

        listaDeSlots.push_back(slotTemporario);
    }

    int x = 0;
    std::cin >> x;
    x++;
    std::cout << "\nTestando o acesso fora do loop:" << std::endl;
    std::cout << "O ID do 5º slot salvo é: " << listaDeSlots[x].getId() << std::endl;
    std::cout << "O Apartamento dele é: " << listaDeSlots[x].getApartamento() << std::endl;
    std::cout << "O tamanho é: " << listaDeSlots[x].getTamanho() << std::endl;
    std::cout << "O tempo de postagem foi: " << listaDeSlots[x].getInsercao().segundo << std::endl;
    std::cout << "O tempo de retirada foi: " << listaDeSlots[x].getRetirada().segundo << std::endl;

    return 0;
}
