#ifndef MORADORES_H
#define MORADORES_H

// Cada morador vincula: número do apartamento <-> tag RFID <-> telefone WhatsApp.
struct Morador {
  int    apartamento;
  String uidTag;     // UID do cartão em HEX maiúsculo, sem espaços (ex.: "A1B2C3D4")
  String telefone;   // formato internacional sem '+' (ex.: 5584999990001)
};

// Retorna o número do apartamento vinculado à tag, ou -1 se a tag não existir.
int getAptoByUID(const String& uid);

// Retorna o telefone do apartamento, ou "" se não houver cadastro.
String getTelefoneByApto(int apartamento);

// Indica se o apartamento existe no cadastro.
bool aptoCadastrado(int apartamento);

#endif
