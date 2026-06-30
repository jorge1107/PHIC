#ifndef MORADORES_H
#define MORADORES_H

#include <Arduino.h>

// Cada morador vincula: número do apartamento <-> tag RFID <-> email.
struct Morador {
  int    apartamento;
  String uidTag;     // UID do cartão em HEX maiúsculo, sem espaços (ex.: "A1B2C3D4")
  String email;
};

// Retorna o número do apartamento vinculado à tag, ou -1 se a tag não existir.
int getAptoByUID(const String& uid);

// Retorna o email do apartamento, ou "" se não houver cadastro.
String getEmailByApto(int apartamento);

// Indica se o apartamento existe no cadastro.
bool aptoCadastrado(int apartamento);

#endif
