/* ============================================================================
 *  ARMÁRIO INTELIGENTE DE ENCOMENDAS — ESP32
 *  Fluxo 1 (Entregador): teclado -> apartamento + tamanho -> abre slot vazio
 *                        compatível -> registra inserção -> notifica morador.
 *  Fluxo 2 (Morador):    aproxima tag RFID -> sistema abre TODOS os slots
 *                        ocupados do seu apartamento -> registra retirada.
 * ----------------------------------------------------------------------------
 *  Bibliotecas (Library Manager):
 *    - LiquidCrystal_I2C
 *    - Keypad
 *    - MFRC522                            (by GithubCommunity)
 *  Núcleo: ESP32 (Espressif)
 * ==========================================================================*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <time.h>
#include <Wire.h>
#include <SPI.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <MFRC522.h>
#include <vector>

#include "config.h"
#include "slot.h"
#include "moradores.h"

// ----------------------------- Periféricos ----------------------------------
LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);
Adafruit_MCP23X17 mcp;                       // 16 saídas -> 16 trincos
MFRC522           rfid(RFID_SS_PIN, RFID_RST_PIN);

// --------------------------- Teclado 4x4 ------------------------------------
const byte LINHAS = 4;
const byte COLUNAS = 4;
char teclas[LINHAS][COLUNAS] = {
  {'1','2','3','A'}, // A -> tamanho P
  {'4','5','6','B'}, // B -> tamanho M
  {'7','8','9','C'}, // C -> tamanho G
  {'*','0','#','D'}  // '*' limpa/cancela  |  '#' confirma
};
byte pinosLinhas[LINHAS]   = {13, 12, 14, 27};
byte pinosColunas[COLUNAS] = {26, 25, 33, 32};
Keypad teclado = Keypad(makeKeymap(teclas), pinosLinhas, pinosColunas, LINHAS, COLUNAS);

// ----------------------------- Slots ----------------------------------------
std::vector<Slot> listaDeSlots;

// ------------------------- Máquina de estados -------------------------------
enum EstadoSistema { AGUARDANDO, DIGITANDO_APTO, SELECIONANDO_TAMANHO, ALOCANDO_SLOT };
EstadoSistema estadoAtual = AGUARDANDO;

String apartamentoDigitado = "";
char   tamanhoSelecionado  = ' ';

// ----------------------------- Protótipos -----------------------------------
void   atualizarDisplay();
void   processarTeclado(char tecla);
void   tentarAlocarCompartimento();
void   processarRetirada(const String& uid);
void   enviarNotificacao(int numeroApto, int idSlot, char tamanho);
void   abrirSlot(int idSlot);
String lerRFID();
Time   obterHoraAtual();
void   modoCadastroTag();

// ============================================================================
//                                  SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  Wire.begin();            // I2C: SDA=21, SCL=22 (LCD + MCP23017)
  SPI.begin();             // SPI: SCK=18, MISO=19, MOSI=23 (RFID)

  // ---- LCD ----
  lcd.init();
  lcd.backlight();
  lcd.print("Iniciando...");

  // ---- RFID ----
  rfid.PCD_Init();

  // ---- Wi-Fi ----
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(WiFi.status() == WL_CONNECTED ? "\nWiFi conectado!" : "\nSem WiFi (segue offline).");

  // ---- NTP ----
  configTime(GMT_OFFSET_SEC, DAYLIGHT_SEC, NTP_SERVER);
  Serial.println("Tempo sincronizado via NTP.");

  // ---- Cria os 16 slots: 8 'P', 4 'M', 4 'G' ----
  for (int i = 1; i <= N_SLOTS; i++) {
    Slot slotTemporario(i);
    if (i <= PEQUENOS) slotTemporario.setTamanho('P');
    else if (i <= MEDIOS) slotTemporario.setTamanho('M');
    else slotTemporario.setTamanho('G');
    listaDeSlots.push_back(slotTemporario);
  }

  estadoAtual = AGUARDANDO;
  atualizarDisplay();
}

// ============================================================================
//                                   LOOP
// ============================================================================
void loop() {
  char tecla = teclado.getKey();
  if (tecla) processarTeclado(tecla);

  // O leitor RFID só fica ativo na tela inicial (retirada pelo morador).
  if (estadoAtual == AGUARDANDO) {
    String uid = lerRFID();
    if (uid.length() > 0) processarRetirada(uid);
  }
}

// ============================================================================
//                          RELÓGIO (NTP -> struct Time)
// ============================================================================
Time obterHoraAtual() {
  Time t = {0, 0, 0, 0, 0, 0};
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Falha ao obter a hora. Retornando tempo zerado.");
    return t;
  }
  t.ano     = timeinfo.tm_year + 1900;
  t.mes     = timeinfo.tm_mon + 1;
  t.dia     = timeinfo.tm_mday;
  t.hora    = timeinfo.tm_hour;
  t.minuto  = timeinfo.tm_min;
  t.segundo = timeinfo.tm_sec;
  Serial.println(&timeinfo, "%A, %d/%B/%Y %H:%M:%S");
  return t;
}

// ============================================================================
//                                 DISPLAY
// ============================================================================
void atualizarDisplay() {
  lcd.clear();
  switch (estadoAtual) {
    case AGUARDANDO:
      lcd.setCursor(0, 0);
      lcd.print("Entrega: digite");
      lcd.setCursor(0, 1);
      lcd.print("Morador: use tag");
      break;

    case DIGITANDO_APTO:
      lcd.setCursor(0, 0);
      lcd.print("Apto: ");
      lcd.print(apartamentoDigitado);
      lcd.setCursor(0, 1);
      lcd.print("#=OK  *=Apaga");
      break;

    case SELECIONANDO_TAMANHO:
      lcd.setCursor(0, 0);
      lcd.print("Tam: A=P B=M C=G");
      lcd.setCursor(0, 1);
      lcd.print("Escolha uma opc");
      break;

    case ALOCANDO_SLOT:
      lcd.setCursor(0, 0);
      lcd.print("Buscando vaga...");
      break;
  }
}

// ============================================================================
//                            TECLADO / FLUXO ENTREGA
// ============================================================================
void processarTeclado(char tecla) {
  // '*' cancela e volta ao início em qualquer estado
  if (tecla == '*') {
    apartamentoDigitado = "";
    estadoAtual = AGUARDANDO;
    atualizarDisplay();
    return;
  }

  switch (estadoAtual) {
    case AGUARDANDO:
      // segurar '#' entra no modo cadastro de tag (utilitário de manutenção)
      if (tecla == '#') { modoCadastroTag(); return; }
      // qualquer dígito inicia a digitação do apartamento
      if (tecla >= '0' && tecla <= '9') {
        apartamentoDigitado = String(tecla);
        estadoAtual = DIGITANDO_APTO;
        atualizarDisplay();
      }
      break;

    case DIGITANDO_APTO:
      if (tecla >= '0' && tecla <= '9') {
        if (apartamentoDigitado.length() < 4) {
          apartamentoDigitado += tecla;
          atualizarDisplay();
        }
      } else if (tecla == '#') {
        if (apartamentoDigitado.length() > 0) {
          estadoAtual = SELECIONANDO_TAMANHO;
          atualizarDisplay();
        }
      }
      break;

    case SELECIONANDO_TAMANHO:
      if      (tecla == 'A') { tamanhoSelecionado = 'P'; estadoAtual = ALOCANDO_SLOT; }
      else if (tecla == 'B') { tamanhoSelecionado = 'M'; estadoAtual = ALOCANDO_SLOT; }
      else if (tecla == 'C') { tamanhoSelecionado = 'G'; estadoAtual = ALOCANDO_SLOT; }

      if (estadoAtual == ALOCANDO_SLOT) {
        atualizarDisplay();
        tentarAlocarCompartimento();
      }
      break;

    default:
      break;
  }
}

void tentarAlocarCompartimento() {
  int  apNum     = apartamentoDigitado.toInt();
  bool encontrou = false;

  // Opcional: rejeitar apartamento sem morador cadastrado (não recebe aviso).
  // if (!aptoCadastrado(apNum)) { ...mensagem... ; return; }

  for (size_t i = 0; i < listaDeSlots.size(); i++) {
    if (listaDeSlots[i].getTamanho() == tamanhoSelecionado && !listaDeSlots[i].isOcupado()) {
      listaDeSlots[i].setApartamento(apNum);
      listaDeSlots[i].setOcupado(true);
      listaDeSlots[i].setInsercao(obterHoraAtual());

      int idSlot = listaDeSlots[i].getId();

      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("Slot aberto: ");  lcd.print(idSlot);
      lcd.setCursor(0, 1); lcd.print("Coloque e feche");

      Serial.printf("Abrindo trinco do slot ID: %d\n", idSlot);
      abrirSlot(idSlot);                          // <-- aciona o trinco físico
      enviarNotificacao(apNum, idSlot, tamanhoSelecionado);

      encontrou = true;
      break;
    }
  }

  if (!encontrou) {
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Sem slots "); lcd.print(tamanhoSelecionado);
    lcd.setCursor(0, 1); lcd.print("disponiveis!");
  }

  delay(4000);
  apartamentoDigitado = "";
  tamanhoSelecionado  = ' ';
  estadoAtual = AGUARDANDO;
  atualizarDisplay();
}

// ============================================================================
//                          RFID / FLUXO RETIRADA
// ============================================================================
String lerRFID() {
  if (!rfid.PICC_IsNewCardPresent()) return "";
  if (!rfid.PICC_ReadCardSerial())   return "";

  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  return uid;
}

void processarRetirada(const String& uid) {
  int apto = getAptoByUID(uid);

  lcd.clear();
  if (apto < 0) {
    lcd.setCursor(0, 0); lcd.print("Tag nao");
    lcd.setCursor(0, 1); lcd.print("cadastrada!");
    Serial.printf("Tag desconhecida: %s\n", uid.c_str());
    delay(3000);
    estadoAtual = AGUARDANDO;
    atualizarDisplay();
    return;
  }

  int abertos = 0;
  for (size_t i = 0; i < listaDeSlots.size(); i++) {
    if (listaDeSlots[i].isOcupado() && listaDeSlots[i].getApartamento() == apto) {
      int idSlot = listaDeSlots[i].getId();
      Serial.printf("Retirada apto %d -> abrindo slot %d\n", apto, idSlot);
      abrirSlot(idSlot);
      listaDeSlots[i].setRetirada(obterHoraAtual());
      listaDeSlots[i].setOcupado(false);
      listaDeSlots[i].setApartamento(0);   // 0 é válido na sua classe Slot
      abertos++;
    }
  }

  lcd.clear();
  if (abertos > 0) {
    lcd.setCursor(0, 0); lcd.print("Apto "); lcd.print(apto);
    lcd.setCursor(0, 1); lcd.print(abertos); lcd.print(" encomenda(s)");
  } else {
    lcd.setCursor(0, 0); lcd.print("Nenhuma");
    lcd.setCursor(0, 1); lcd.print("encomenda.");
  }
  delay(4000);
  estadoAtual = AGUARDANDO;
  atualizarDisplay();
}

// ============================================================================
//                       ACIONAMENTO FÍSICO DO TRINCO
// ============================================================================
// O ID do slot (1..16) mapeia diretamente para o pino do MCP23017 (0..15).
// A saída HIGH energiza o driver (transistor/relé/ULN2803) que abre o solenoide.
void abrirSlot(int idSlot) {
  int pino = idSlot - 1;
  if (pino < 0 || pino >= N_SLOTS) return;

  switch (pino)
  {
  case 0:
    digitalWrite(s3, LOW);
    digitalWrite(s2, LOW);
    digitalWrite(s1, LOW);
    digitalWrite(s0, LOW);
    digitalWrite(e, HIGH);
    delay(LOCK_PULSE_MS);
    digitalWrite(e, LOW);
    break;
  case 1:
    digitalWrite(s3, LOW);
    digitalWrite(s2, LOW);
    digitalWrite(s1, LOW);
    digitalWrite(s0, HIGH);
    digitalWrite(e, HIGH);
    delay(LOCK_PULSE_MS);
    digitalWrite(e, LOW);
    break;
  case 2:
    digitalWrite(s0, LOW);
    digitalWrite(s1, LOW);
    digitalWrite(s2, HIGH);
    digitalWrite(s3, LOW);
    digitalWrite(e, HIGH);
    delay(LOCK_PULSE_MS);
    digitalWrite(e, LOW);
    break;
  case 3:
    digitalWrite(s0, LOW);
    digitalWrite(s1, LOW);
    digitalWrite(s2, HIGH);
    digitalWrite(s3, HIGH);
    digitalWrite(e, HIGH);
    delay(LOCK_PULSE_MS);
    digitalWrite(e, LOW);
    break;
  case 4:
    digitalWrite(s0, LOW);
    digitalWrite(s1, HIGH);
    digitalWrite(s2, LOW);
    digitalWrite(s3, LOW);
    digitalWrite(e, HIGH);
    delay(LOCK_PULSE_MS);
    digitalWrite(e, LOW);
    break;
  case 5:
    digitalWrite(s3, LOW);
    digitalWrite(s2, HIGH);
    digitalWrite(s1, LOW);
    digitalWrite(s0, HIGH);
    digitalWrite(e, HIGH);
    delay(LOCK_PULSE_MS);
    digitalWrite(e, LOW);
    break;
  case 6:
    digitalWrite(s3, LOW);
    digitalWrite(s2, HIGH);
    digitalWrite(s1, HIGH);
    digitalWrite(s0, LOW);
    digitalWrite(e, HIGH);
    delay(LOCK_PULSE_MS);
    digitalWrite(e, LOW);
    break;
  case 7:
    digitalWrite(s3, LOW);
    digitalWrite(s2, HIGH);
    digitalWrite(s1, HIGH);
    digitalWrite(s0, HIGH);
    digitalWrite(e, HIGH);
    delay(LOCK_PULSE_MS);
    digitalWrite(e, LOW);
    break;
  case 8:
    digitalWrite(s3, HIGH);
    digitalWrite(s2, LOW);
    digitalWrite(s1, LOW);
    digitalWrite(s0, LOW);
    digitalWrite(e, HIGH);
    delay(LOCK_PULSE_MS);
    digitalWrite(e, LOW);
    break;
  case 9:
    digitalWrite(s3, HIGH);
    digitalWrite(s2, LOW);
    digitalWrite(s1, LOW);
    digitalWrite(s0, HIGH);
    digitalWrite(e, HIGH);
    delay(LOCK_PULSE_MS);
    digitalWrite(e, LOW);
    break;
  case 10:
    digitalWrite(s3, HIGH);
    digitalWrite(s2, LOW);
    digitalWrite(s1, HIGH);
    digitalWrite(s0, LOW);
    digitalWrite(e, HIGH);
    delay(LOCK_PULSE_MS);
    digitalWrite(e, LOW);
    break;
  case 11:
    digitalWrite(s3, HIGH);
    digitalWrite(s2, LOW);
    digitalWrite(s1, HIGH);
    digitalWrite(s0, HIGH);
    digitalWrite(e, HIGH);
    delay(LOCK_PULSE_MS);
    digitalWrite(e, LOW);
    break;
  case 12:
    digitalWrite(s3, HIGH);
    digitalWrite(s2, HIGH);
    digitalWrite(s1, LOW);
    digitalWrite(s0, LOW);
    digitalWrite(e, HIGH);
    delay(LOCK_PULSE_MS);
    digitalWrite(e, LOW);
    break;
  case 13:
    digitalWrite(s3, HIGH);
    digitalWrite(s2, HIGH);
    digitalWrite(s1, LOW);
    digitalWrite(s0, HIGH);
    digitalWrite(e, HIGH);
    delay(LOCK_PULSE_MS);
    digitalWrite(e, LOW);
    break;
  case 14:
    digitalWrite(s3, HIGH);
    digitalWrite(s2, HIGH);
    digitalWrite(s1, HIGH);
    digitalWrite(s0, LOW);
    digitalWrite(e, HIGH);
    delay(LOCK_PULSE_MS);
    digitalWrite(e, LOW);
    break;
  case 15:
    digitalWrite(s3, HIGH);
    digitalWrite(s2, HIGH);
    digitalWrite(s1, HIGH);
    digitalWrite(s0, HIGH);
    digitalWrite(e, HIGH);
    delay(LOCK_PULSE_MS);
    digitalWrite(e, LOW);
    break;
  default:
    break;
  }
}

// ============================================================================
//                NOTIFICAÇÃO (WhatsApp via CallMeBot - HTTPS GET)
// ============================================================================
void enviarNotificacao(int numeroApto, int idSlot, char tamanho) {
  Serial.printf("Aviso de encomenda -> apto %d (slot %d, tam %c)\n", numeroApto, idSlot, tamanho);

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Sem WiFi: notificacao nao enviada.");
    return;
  }
  String telefone = getTelefoneByApto(numeroApto);
  if (telefone == "") {
    Serial.println("Apto sem telefone cadastrado: notificacao ignorada.");
    return;
  }

  // Mensagem sem acentos/caracteres especiais (simplifica o URL-encode).
  String msg = "Ola! Chegou uma encomenda (tam " + String(tamanho) + ") para o apto " + String(numeroApto) + ", guardada no armario " + String(idSlot) + ". Aproxime sua tag RFID para retirar.";
  msg.replace(" ", "+");

  String url = "https://api.callmebot.com/whatsapp.php?phone=" + telefone + "&text=" + msg + "&apikey=" + String(CALLMEBOT_APIKEY);

  WiFiClientSecure client;
  client.setInsecure();                 // CallMeBot: dispensa validar certificado
  HTTPClient https;
  if (https.begin(client, url)) {
    int code = https.GET();
    Serial.printf("CallMeBot resposta HTTP: %d\n", code);
    https.end();
  } else {
    Serial.println("Falha ao iniciar conexao HTTPS.");
  }
}

// ============================================================================
//          UTILITÁRIO: descobrir o UID de uma tag p/ cadastrar moradores
// ============================================================================
void modoCadastroTag() {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("MODO CADASTRO");
  lcd.setCursor(0, 1); lcd.print("Aproxime a tag");
  Serial.println(">> MODO CADASTRO: aproxime a tag para ler o UID (Serial).");

  unsigned long t0 = millis();
  while (millis() - t0 < 10000) {       // 10 s de janela
    String uid = lerRFID();
    if (uid.length() > 0) {
      Serial.printf(">> UID lido: %s  (copie para moradores.cpp)\n", uid.c_str());
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("UID:");
      lcd.setCursor(0, 1); lcd.print(uid);
      delay(4000);
      break;
    }
    if (teclado.getKey() == '*') break; // cancela
  }
  estadoAtual = AGUARDANDO;
  atualizarDisplay();
}
