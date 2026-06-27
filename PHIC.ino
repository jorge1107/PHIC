#include <WiFi.h>
#include <time.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <vector>
#include "slot.h"

// --- Configurações do Servidor de Tempo (NTP) ---
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -10800; // Fuso horário de Brasília (UTC-3) em segundos (-3 * 3600)
const int   daylightOffset_sec = 0; // Horário de verão (0 se não estiver em horário de verão)

// --- Configuração do Display LCD 16x2 (I2C) ---
// Endereço padrão geralmente é 0x27 ou 0x3F. Pinos padrão no ESP32: SDA (GPIO 21), SCL (GPIO 22)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- Configuração do Teclado Matricial 4x4 ---
const byte LINHAS = 4;
const byte COLUNAS = 4;
char teclas[LINHAS][COLUNAS] = {
  {'1','2','3','A'}, // Vamos usar 'A' para tamanho P
  {'4','5','6','B'}, // Vamos usar 'B' para tamanho M
  {'7','8','9','C'}, // Vamos usar 'C' para tamanho G
  {'*','0','#','D'}  // '*' limpa, '#' confirma
};
// Sugestão de pinos para o ESP32 (evitando pinos de boot/restritos)
byte pinosLinhas[LINHAS] = {13, 12, 14, 27}; 
byte pinosColunas[COLUNAS] = {26, 25, 33, 32}; 

Keypad teclado = Keypad(makeKeymap(teclas), pinosLinhas, pinosColunas, LINHAS, COLUNAS);

// --- Dados do Wi-Fi ---
const char* ssid = "TESTE";
const char* password = "MICROCONTROLADOR";

// --- Instanciação dos Slots ---
std::vector<Slot> listaDeSlots;

// --- Variáveis de Controle e Máquina de Estados ---
enum EstadoSistema { AVISO_INICIAL, DIGITANDO_APTO, SELECIONANDO_TAMANHO, ALOCANDO_SLOT };
EstadoSistema estadoAtual = AVISO_INICIAL;

String apartamentoDigitado = "";
char tamanhoSelecionado = ' ';

void setup() {
  Serial.begin(115200);

  // Inicialização do LCD
  lcd.init();
  lcd.backlight();
  lcd.print("Iniciando...");

  // Conexão Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado!");

  // Inicializa e sincroniza o relógio interno do ESP32 com a rede
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Tempo sincronizado via NTP.");

  // Inicialização dos 16 slots do armário inteligente
  for (int i = 1; i <= 16; i++) {
    Slot slotTemporario(i);
    if (i < 9) {
      slotTemporario.setTamanho('P');
    } else if (i < 13) {
      slotTemporario.setTamanho('M');
    } else {
      slotTemporario.setTamanho('G');
    }
    listaDeSlots.push_back(slotTemporario);
  }

  estadoAtual = DIGITANDO_APTO;
  atualizarDisplay();
}

void loop() {
  char tecla = teclado.getKey();

  if (tecla) {
    processarTeclado(tecla);
  }
}

Time obterHoraAtual() {
  Time t = {0, 0, 0, 0, 0, 0};
  struct tm timeinfo;
  
  // getLocalTime tenta pegar a hora sincronizada
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Falha ao obter a hora. Retornando tempo zerado.");
    return t;
  }
  
  // Mapeia os dados da biblioteca nativa para a sua struct personalizada
  t.ano = timeinfo.tm_year + 1900; // A biblioteca conta os anos a partir de 1900
  t.mes = timeinfo.tm_mon + 1;     // A biblioteca conta meses de 0 a 11
  t.dia = timeinfo.tm_mday;
  t.hora = timeinfo.tm_hour;
  t.minuto = timeinfo.tm_min;
  t.segundo = timeinfo.tm_sec;
  
  return t;
}

void atualizarDisplay() {
  lcd.clear();
  switch (estadoAtual) {
    case DIGITANDO_APTO:
      lcd.setCursor(0, 0);
      lcd.print("Apto: ");
      lcd.print(apartamentoDigitado);
      lcd.setCursor(0, 1);
      lcd.print("#->Conf *->Apaga");
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

void processarTeclado(char tecla) {
  if (tecla == '*') {
    apartamentoDigitado = "";
    estadoAtual = DIGITANDO_APTO;
    atualizarDisplay();
    return;
  }

  switch (estadoAtual) {
    case DIGITANDO_APTO:
      if (tecla >= '0' && tecla <= '9') {
        if (apartamentoDigitado.length() < 4) { // Limita a 4 dígitos
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
      if (tecla == 'A') {
        tamanhoSelecionado = 'P';
        estadoAtual = ALOCANDO_SLOT;
      } else if (tecla == 'B') {
        tamanhoSelecionado = 'M';
        estadoAtual = ALOCANDO_SLOT;
      } else if (tecla == 'C') {
        tamanhoSelecionado = 'G';
        estadoAtual = ALOCANDO_SLOT;
      }
      
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
  int apNum = apartamentoDigitado.toInt();
  bool encontrou = false;

  for (size_t i = 0; i < listaDeSlots.size(); i++) {
    // Busca um slot que combine com o tamanho e esteja desocupado
    if (listaDeSlots[i].getTamanho() == tamanhoSelecionado && !listaDeSlots[i].isOcupado()) { //Slot do memsmo tamanho e desocupado
      
      // Valida e define o apartamento no objeto
      listaDeSlots[i].setApartamento(apNum);
      listaDeSlots[i].setOcupado(true);
      listaDeSlots[i].setInsercao(obterHoraAtual());
      
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Slot Aberto: ");
      lcd.print(listaDeSlots[i].getId());
      
      lcd.setCursor(0, 1);
      lcd.print("Apto registrado!");
      
      // Aqui você acionará o pino do relé/MOSFET correspondente ao ID encontrado
      Serial.print("Abrindo trinco do slot ID: ");
      Serial.println(listaDeSlots[i].getId());

      // Chamar função de envio de notificação (E-mail/WhatsApp via HTTP)
      enviarNotificacao(apNum, listaDeSlots[i].getId());

      encontrou = true;
      break;
    }
  }

  if (!encontrou) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Sem slots ");
    lcd.print(tamanhoSelecionado);
    lcd.setCursor(0, 1);
    lcd.print("disponiveis!");
  }

  delay(4000); // Exibe o resultado por 4 segundos antes de resetar a tela inicial
  apartamentoDigitado = "";
  estadoAtual = DIGITANDO_APTO;
  atualizarDisplay();
}

void enviarNotificacao(int numeroApto, int idSlot) {
  Serial.print("Enviando aviso de encomenda para o apartamento ");
  Serial.println(numeroApto);
  // A ser implementado com bibliotecas HTTPClient ou ESP-Mail-Client
}