#include "moradores.h"
static Morador cadastro[] = {
  //  apto   UID da tag        email
  { 101, "8A2F7C15", ""},
  { 201, "00000000", ""},
  { 301, "00000000", ""},
  { 401, "00000000", ""},
  { 501, "00000000", ""},
  { 601, "00000000", ""},
  { 701, "7BAF561C", "jorgelucas1107@gmail.com" },
  { 801, "00000000", ""},
  { 901, "00000000", ""},
  {1001, "00000000", ""},
  {1101, "00000000", ""},
  {1201, "00000000", ""},
  {1301, "00000000", ""},
  {1401, "00000000", ""}
};
static const int N_MORADORES = sizeof(cadastro) / sizeof(cadastro[0]);

int getAptoByUID(const String& uid) {
  for (int i = 0; i < N_MORADORES; i++) {
    if (cadastro[i].uidTag.equalsIgnoreCase(uid)) {
      return cadastro[i].apartamento;
    }
  }
  return -1;
}

String getEmailByApto(int apartamento) {
  for (int i = 0; i < N_MORADORES; i++) {
    if (cadastro[i].apartamento == apartamento) {
      return cadastro[i].email;
    }
  }
  return "";
}

bool aptoCadastrado(int apartamento) {
  for (int i = 0; i < N_MORADORES; i++) {
    if (cadastro[i].apartamento == apartamento) return true;
  }
  return false;
}
