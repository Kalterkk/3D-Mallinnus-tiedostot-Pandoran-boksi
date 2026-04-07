#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = {
  {'7','8','9'},
  {'4','5','6'},
  {'1','2','3'},
  {' ', '0', 'E'},
};

byte rowPins[ROWS] = {2, 3, 4, 5};
byte colPins[COLS] = {8, 9, 10};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

const int SOLENOIDI_PIN = 13;
const int SUMMERI_PIN = 6;  // 🔊 uusi

const int KOODIN_PITUUS = 4;
const unsigned long SOLENOIDI_MAX_AUKI_MS = 3000;
const String MASTER_KOODI = "6767";
const int EEPROM_OSOITE = 0;      // Koodi tallennetaan osoitteisiin 0-3
const byte EEPROM_MERKKI = 0xAA;  // Tarkistusmerkki osoitteessa 4

enum Tila {
  ASETA_KOODI,
  ODOTA_SYOTE,
  AUKI,
  VAPAA,
  RESET_ODOTA
};

Tila tila = ASETA_KOODI;
String oikeaKoodi = "";
String syote = "";
unsigned long aukiAlkuAika = 0;

int syoteAlkuKohta = 0;
bool vapaaKaytetty = false;
bool vaihdaKoodi = false;
bool masterAvasi = false;

// 🔊 PIIPPAUS
void piippaa() {
  digitalWrite(SUMMERI_PIN, HIGH);
  delay(300);
  digitalWrite(SUMMERI_PIN, LOW);
}

void naytaViesti(String rivi1, String rivi2 = "") {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(rivi1);

  if (rivi2 != "") {
    lcd.setCursor(0, 1);
    lcd.print(rivi2);

    int indeksi = rivi2.indexOf(':');
    if (indeksi != -1) {
      syoteAlkuKohta = indeksi + 1;
    } else {
      syoteAlkuKohta = rivi2.length();
    }
  }
}

void tallennaKoodi(String koodi) {
  for (int i = 0; i < KOODIN_PITUUS; i++) {
    EEPROM.write(EEPROM_OSOITE + i, koodi[i]);
  }
  EEPROM.write(EEPROM_OSOITE + KOODIN_PITUUS, EEPROM_MERKKI);
}

String lueKoodi() {
  String koodi = "";
  for (int i = 0; i < KOODIN_PITUUS; i++) {
    koodi += (char)EEPROM.read(EEPROM_OSOITE + i);
  }
  return koodi;
}

bool koodiTallennettu() {
  return EEPROM.read(EEPROM_OSOITE + KOODIN_PITUUS) == EEPROM_MERKKI;
}

void avaaSolenoidi() {
  digitalWrite(SOLENOIDI_PIN, HIGH);
  piippaa();  // 🔊 ääni avauksessa
  aukiAlkuAika = millis();
  naytaViesti("Auki!", "Sulkeutuu 3s...");
}

void setup() {
  Serial.begin(9600);
  pinMode(SOLENOIDI_PIN, OUTPUT);
  pinMode(SUMMERI_PIN, OUTPUT);  // 🔊 tärkeä

  digitalWrite(SOLENOIDI_PIN, LOW);
  digitalWrite(SUMMERI_PIN, LOW);

  lcd.init();
  lcd.backlight();

  if (koodiTallennettu()) {
    oikeaKoodi = lueKoodi();
    tila = ODOTA_SYOTE;
    naytaViesti("Tervetuloa!", "Syota koodi:");
  } else {
    naytaViesti("Aseta uusi koodi", "(4 num) + E:");
  }
}

void loop() {

  // ===============================
  // AUKI -> seuraava tila
  // ===============================
  if (tila == AUKI && (millis() - aukiAlkuAika >= SOLENOIDI_MAX_AUKI_MS)) {
    digitalWrite(SOLENOIDI_PIN, LOW);
    piippaa();  // 🔊 ääni sulkeutuessa

    if (vapaaKaytetty) {
      tila = ODOTA_SYOTE;
      syote = "";
      naytaViesti("Lukittu.", "Syota koodi:");
    } else if (masterAvasi) {
      masterAvasi = false;
      tila = RESET_ODOTA;
      naytaViesti("9 = nollaa", "E = lukitse");
    } else {
      tila = VAPAA;
      vapaaKaytetty = false;
      naytaViesti("Lukittu.", "E = avaa uudelleen");
    }
    return;
  }

  char key = keypad.getKey();
  if (key == NO_KEY) return;

  // ===============================
  // ASETA_KOODI
  // ===============================
  if (tila == ASETA_KOODI) {
    if (key == 'E') {
      if (syote.length() == KOODIN_PITUUS) {
        oikeaKoodi = syote;
        tallennaKoodi(oikeaKoodi);
        syote = "";
        if (vaihdaKoodi) {
          vaihdaKoodi = false;
          tila = VAPAA;
          naytaViesti("Koodi vaihdettu!", "E = avaa uudelleen");
        } else {
          tila = ODOTA_SYOTE;
          naytaViesti("Koodi asetettu!", "Syota koodi:");
        }
      } else {
        naytaViesti("Oltava 4 numeroa", "Yrita uudelleen:");
        syote = "";
        delay(1500);
        naytaViesti("Aseta uusi koodi", "(4 num) + E:");
      }
    } else if (key != ' ') {
      if (syote.length() < KOODIN_PITUUS) {
        syote += key;
        lcd.setCursor(syoteAlkuKohta + syote.length() - 1, 1);
        lcd.print('*');
      }
    }
  }

  // ===============================
  // ODOTA_SYOTE
  // ===============================
  else if (tila == ODOTA_SYOTE) {
    if (key == 'E') {
      if (syote == oikeaKoodi || syote == MASTER_KOODI) {
        masterAvasi = (syote == MASTER_KOODI);
        tila = AUKI;
        syote = "";
        vapaaKaytetty = false;
        avaaSolenoidi();
      } else {
        naytaViesti("Vaara koodi!", "Yrita uudelleen:");
        syote = "";
        delay(1500);
        naytaViesti("Lukittu.", "Syota koodi:");
      }
    } else if (key != ' ') {
      if (syote.length() < KOODIN_PITUUS) {
        syote += key;
        lcd.setCursor(syoteAlkuKohta + syote.length() - 1, 1);
        lcd.print('*');
      }
    }
  }

  // ===============================
  // VAPAA
  // ===============================
  else if (tila == VAPAA) {
    if (key == 'E') {
      tila = AUKI;
      vapaaKaytetty = true;
      avaaSolenoidi();
    } else if (key == '5') {
      tila = ASETA_KOODI;
      vaihdaKoodi = true;
      syote = "";
      oikeaKoodi = "";
      naytaViesti("Vaihda koodi:", "(4 num) + E:");
    }
  }

  // ===============================
  // RESET_ODOTA
  // ===============================
  else if (tila == RESET_ODOTA) {
    if (key == '9') {
      // Tyhjennetään EEPROM tarkistusmerkki -> laite palaa alkutilaan
      EEPROM.write(EEPROM_OSOITE + KOODIN_PITUUS, 0xFF);
      oikeaKoodi = "";
      syote = "";
      tila = ASETA_KOODI;
      naytaViesti("Nollattu!", "Aseta uusi koodi");
      delay(1500);
      naytaViesti("Aseta uusi koodi", "(4 num) + E:");
    } else if (key == 'E') {
      tila = ODOTA_SYOTE;
      syote = "";
      naytaViesti("Lukittu.", "Syota koodi:");
    }
  }
}