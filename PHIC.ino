/* ============================================================================
 * ARMÁRIO INTELIGENTE DE ENCOMENDAS — ESP32 (Versão OLED + E-mail SMTP)
 * Fluxo 1 (Entregador): teclado -> apartamento + tamanho -> abre slot vazio
 * compatível -> registra inserção -> notifica morador.
 * Fluxo 2 (Morador):    aproxima tag RFID -> sistema abre TODOS os slots
 * ocupados do seu apartamento -> registra retirada.
 * ==========================================================================*/

#include <WiFi.h>
#include <time.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>       // Biblioteca gráfica para o OLED
#include <Adafruit_SSD1306.h>   // Biblioteca do display OLED SSD1306
#include <Keypad.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include <vector>
#include <ESP_Mail_Client.h>    // Biblioteca para envio de e-mails

#include "config.h"
#include "slot.h"
#include "moradores.h"

// ----------------------------- Periféricos ----------------------------------
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
MFRC522           rfid(RFID_SS_PIN, RFID_RST_PIN);
Servo servos[N_SERVOS];
const int pinosServos[N_SERVOS] = {S2, S1, S0};

// --------------------------- Teclado 4x4 ------------------------------------
const byte LINHAS = 4;
const byte COLUNAS = 4;
char teclas[LINHAS][COLUNAS] = {
  {'1','2','3','A'}, // A -> tamanho P
  {'4','5','6','B'}, // B -> tamanho M
  {'7','8','9','C'}, // C -> tamanho G
  {'*','0','#','D'}  // '*' limpa/cancela  | '#' confirma
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

// ------------------------ Configuração do e-mail ----------------------------
SMTPSession smtp;

// Função de callback para monitorar o status do envio no Serial Monitor
void smtpCallback(SMTP_Status status) {
  Serial.println(status.info());
  if (status.success()) {
    Serial.println("------------------------------------");
    Serial.println("E-mail enviado com sucesso via SMTP!");
    Serial.println("------------------------------------");
  }
}

// ============================================================================
//                                  SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  Wire.begin();            // I2C: SDA=21, SCL=22
  SPI.begin();             // SPI: SCK=18, MISO=19, MOSI=23 (RFID)

  // ---- Inicialização do Display OLED ----
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("Falha na alocação do display SSD1306"));
    for(;;); 
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Iniciando...");
  display.display();

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

  for (int i = 0; i < N_SERVOS; i++) {
    servos[i].setPeriodHertz(50);
    servos[i].attach(pinosServos[i], 500, 2400);
    servos[i].write(0);
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

  // O leitor RFID só fica ativo na tela inicial (retirada pelo morador)
  if (estadoAtual == AGUARDANDO) {
    String uid = lerRFID();
    if (uid.length() > 0) processarRetirada(uid);
  }
}

// ============================================================================
//                NOTIFICAÇÃO (E-mail via Protocolo SMTP)
// ============================================================================
void enviarNotificacao(int numeroApto, int idSlot, char tamanho) {
    Serial.printf("Aviso de encomenda -> apto %d (slot %d, tam %c)\n", numeroApto, idSlot, tamanho);
    
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Sem WiFi: notificacao nao enviada.");
      return;
    }
    
    String emailDestinatario = getEmailByApto(numeroApto);
    if (emailDestinatario == "") {
      Serial.println("Apto sem email cadastrado: notificacao ignorada.");
      return;
    }

    // Vincula a função de monitoramento à sessão SMTP
    smtp.callback(smtpCallback);
    
    // Configura os parâmetros do servidor
    Session_Config config;
    config.server.host_name = SMTP_HOST;
    config.server.port = SMTP_PORT;
    config.login.email = AUTHOR_EMAIL;
    config.login.password = SMTP_PASSWORD;
    config.login.user_domain = "";

    // Monta a estrutura da mensagem
    SMTP_Message message;
    message.sender.name = SENDER_NAME;
    message.sender.email = AUTHOR_EMAIL;
    
    String assunto = "Nova Encomenda Recebida - Apto " + String(numeroApto);
    message.subject = assunto.c_str();
    message.addRecipient("Morador", emailDestinatario.c_str());
    
    // Conteúdo formatado em HTML
    String textoMsg = "<h2>Olá, morador do apartamento " + String(numeroApto) + "!</h2>";
    textoMsg += "<p>Uma nova encomenda foi depositada para você no armário inteligente.</p>";
    textoMsg += "<ul>";
    textoMsg += "<li><strong>Compartimento (Slot):</strong> " + String(idSlot) + "</li>";
    textoMsg += "<li><strong>Tamanho do pacote:</strong> " + String(tamanho) + "</li>";
    textoMsg += "</ul>";
    textoMsg += "<br><p><em>Para retirar, basta aproximar sua tag RFID cadastrada no leitor do armário.</em></p>";

    message.html.content = textoMsg.c_str();
    message.html.charSet = "utf-8";
    message.html.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
    
    message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
    
    // Tenta conectar e disparar o e-mail
    if (!smtp.connect(&config)) {
      Serial.printf("Falha ao conectar no servidor SMTP: %s\n", smtp.errorReason().c_str());
      return;
    }

    if (!MailClient.sendMail(&smtp, &message)) {
      Serial.printf("Erro ao enviar o e-mail: %s\n", smtp.errorReason().c_str());
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
//                                 DISPLAY (OLED)
// ============================================================================
void atualizarDisplay() {
  display.clearDisplay();
  
  switch (estadoAtual) {
    case AGUARDANDO:
      display.setCursor(0, 0);  display.print("Entrega: digite");
      display.setCursor(0, 16); display.print("Morador: use tag");
      break;

    case DIGITANDO_APTO:
      display.setCursor(0, 0);  display.print("Apto: "); display.print(apartamentoDigitado);
      display.setCursor(0, 16); display.print("#=OK *=Apagar");
      break;

    case SELECIONANDO_TAMANHO:
      display.setCursor(0, 0);  display.print("Tam: A=P B=M C=G");
      display.setCursor(0, 16); display.print("Escolha uma opc");
      break;

    case ALOCANDO_SLOT:
      display.setCursor(0, 0);  display.print("Buscando vaga...");
      break;
  }
  display.display();
}

// ============================================================================
//                            TECLADO / FLUXO ENTREGA
// ============================================================================
void processarTeclado(char tecla) {
  if (tecla == '*') {
    apartamentoDigitado = "";
    estadoAtual = AGUARDANDO;
    atualizarDisplay();
    return;
  }

  switch (estadoAtual) {
    case AGUARDANDO:
      if (tecla == '#') { modoCadastroTag(); return; }
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

  for (size_t i = 0; i < listaDeSlots.size(); i++) {
    if (listaDeSlots[i].getTamanho() == tamanhoSelecionado && !listaDeSlots[i].isOcupado()) {
      listaDeSlots[i].setApartamento(apNum);
      listaDeSlots[i].setOcupado(true);
      listaDeSlots[i].setInsercao(obterHoraAtual());

      int idSlot = listaDeSlots[i].getId();

      display.clearDisplay();
      display.setCursor(0, 0);  display.print("Slot aberto: "); display.print(idSlot);
      display.setCursor(0, 16); display.print("Coloque e feche");
      display.display();
      
      Serial.printf("Abrindo trinco do slot ID: %d\n", idSlot);
      abrirSlot(idSlot);
      
      // DISPARO DO E-MAIL AUTOMÁTICO
      enviarNotificacao(apNum, idSlot, tamanhoSelecionado);
      
      encontrou = true;
      break;
    }
  }

  if (!encontrou) {
    display.clearDisplay();
    display.setCursor(0, 0);  display.print("Sem slots "); display.print(tamanhoSelecionado);
    display.setCursor(0, 16); display.print("disponiveis!");
    display.display();
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

  display.clearDisplay();
  
  if (apto < 0) {
    display.setCursor(0, 0);  display.print("Tag nao");
    display.setCursor(0, 16); display.print("cadastrada!");
    display.display();
    
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
      listaDeSlots[i].setApartamento(0);
      abertos++;
    }
  }

  display.clearDisplay();
  if (abertos > 0) {
    display.setCursor(0, 0);  display.print("Apto "); display.print(apto);
    display.setCursor(0, 16); display.print(abertos); display.print(" emcomenda(s)");
  } else {
    display.setCursor(0, 0);  display.print("Nenhuma");
    display.setCursor(0, 16); display.print("encomenda.");
  }
  display.display();
  
  delay(4000);
  estadoAtual = AGUARDANDO;
  atualizarDisplay();
}

// ============================================================================
//                       ACIONAMENTO FÍSICO DO TRINCO
// ============================================================================
void abrirSlot(int idSlot) {
  // Lógica dos trincos físicos / multiplexador permanece aqui
}

// ============================================================================
//          UTILITÁRIO: descobrir o UID de uma tag p/ cadastrar moradores
// ============================================================================
void modoCadastroTag() {
  display.clearDisplay();
  display.setCursor(0, 0);  display.print("MODO CADASTRO");
  display.setCursor(0, 16); display.print("Aproxime a tag");
  display.display();
  
  Serial.println(">> MODO CADASTRO: aproxime a tag para ler o UID (Serial).");
  unsigned long t0 = millis();
  
  while (millis() - t0 < 10000) {
    String uid = lerRFID();
    if (uid.length() > 0) {
      Serial.printf(">> UID lido: %s  (copie para moradores.cpp)\n", uid.c_str());
      display.clearDisplay();
      display.setCursor(0, 0);  display.print("UID lido:");
      display.setCursor(0, 16); display.print(uid);
      display.display();
      delay(4000);
      break;
    }
    if (teclado.getKey() == '*') break;
  }
  estadoAtual = AGUARDANDO;
  atualizarDisplay();
}