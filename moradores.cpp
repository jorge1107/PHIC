#include "moradores.h"

// ----------------------------------------------------------------------------
// CADASTRO. Em um produto final isto viria de um banco/NVS/SD; aqui fica fixo
// no código para facilitar os testes. Para descobrir o UID de uma tag, use o
// "modo cadastro" (segure '#' por ~2 s na tela inicial) e leia o Serial Monitor.
// ----------------------------------------------------------------------------
static Morador cadastro[] = {
  //  apto   UID da tag        telefone (WhatsApp)
  {  101,  "",      "5584999990001" },
  {  201,  "",      "5584999990002" },
  {  301,  "",      "5584999990003" },
  {  401,  "",      "5584999990001" },
  {  501,  "",      "5584999990002" },
  {  601,  "",      "5584999990003" },
  {  701,  "",      "5584999990001" },
  {  801,  "",      "5584999990002" },
  {  901,  "",      "5584999990003" },
  { 1001,  "",      "5584999990001" },
  { 1101,  "",      "5584999990002" },
  { 1201,  "",      "5584999990003" },
  { 1301,  "",      "5584999990001" },
  { 1401,  "",      "5584999990002" }
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

String getTelefoneByApto(int apartamento) {
  for (int i = 0; i < N_MORADORES; i++) {
    if (cadastro[i].apartamento == apartamento) {
      return cadastro[i].telefone;
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
