#include <EEPROM.h>
#include <SPI.h>
#include <MFRC522.h>

#define RED_LED     3
#define GREEN_LED   4
#define BLUE_LED    2

#define TRANSISTOR  5

// Timpul de deblocare al incuietorii
#define LOCK_DELAY  2000

// Timpul de aprindere/stingere al LED-ului
#define LED_DELAY   200

// Timpul de aprindere/stingere al LED-ului cand se acceseaza senzorul
#define ACCESS_LED_DELAY 1000

// Numar magic pentru detectia cardului master
#define MAGIC_NUMBER 143

// Modul program
bool program_mode = false;

// Citire cu succes
uint8_t success_read;

// Vector unde se stocheaza ID-ul citit din EEPROM
byte stored_card[4];
// Vector unde se stocheaza ID-ul scanat
byte read_card[4];
// Vector unde se stocheaza ID-ul cardului master citit din EEPROM
byte master_card[4];

// Creez instanta MFRC522
#define SS_PIN 10
#define RST_PIN 9
MFRC522 mfrc522(SS_PIN, RST_PIN);

void setup() {
  // Setez LED-urile ca OUTPUT
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);
  
  // Setez releul ca OUTPUT
  pinMode(TRANSISTOR, OUTPUT);
  
  digitalWrite(TRANSISTOR, LOW);
  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(BLUE_LED, LOW);

  Serial.begin(9600);
  SPI.begin();
  mfrc522.PCD_Init();

  // Afiseaza detalii despre reader
  show_reader_details();
  
  // Retinem un numar magic pentru a vedea daca este definit cardul master
  if (EEPROM.read(1) != MAGIC_NUMBER) {
    Serial.println(F("No Master Card Defined"));
    Serial.println(F("Scan A PICC to Define as Master Card"));
    
    do {
      success_read = get_ID(); // Iau ID-ul
      
      digitalWrite(BLUE_LED, HIGH);
      delay(LED_DELAY);
      digitalWrite(BLUE_LED, LOW);
      delay(LED_DELAY);
    } while (!success_read);

    // Scriu in EEPROM ID-ul cardului master
    for (uint8_t j = 0; j < 4; ++j) {
      EEPROM.write(j + 2, read_card[j]);
    }
    
    EEPROM.write(1, MAGIC_NUMBER);
    Serial.println(F("Master Card Defined"));
  }
  
  Serial.println(F("-------------------"));
  Serial.println(F("Master Card's UID"));

  // Afisez ID-ul cardului master
  for (uint8_t i = 0; i < 4; ++i) {
    master_card[i] = EEPROM.read(i + 2);
    Serial.print(master_card[i], HEX);
  }
  
  Serial.println("");
  Serial.println(F("-------------------"));
  Serial.println(F("Everything is ready"));
  Serial.println(F("Waiting PICCs to be scanned"));

  cycle_leds();
}

void loop () {
  do {
    // Iau ID-ul
    success_read = get_ID();

    // Verificam daca este modul program
    if (program_mode) {
      cycle_leds();
    } else {
      normal_mode_on();
    }
  } while (!success_read);
  
  if (program_mode) {
    // Daca este master, se iese din program mode
    if (is_master(read_card)) {
      Serial.println(F("Master Card Scanned"));
      Serial.println(F("Exiting Program Mode"));
      Serial.println(F("-----------------------------"));
      
      program_mode = false;
      return;
    } else {
      // Daca ID-ul este deja existent
      if (find_ID(read_card)) {
        Serial.println(F("I know this PICC, removing..."));
        
        // Sterg ID-ul
        delete_ID(read_card);
        
        Serial.println("-----------------------------");
        Serial.println(F("Scan a PICC to ADD or REMOVE to EEPROM"));
      } else {
        Serial.println(F("I do not know this PICC, adding..."));

        // Adaug ID-ul
        write_ID(read_card);

        Serial.println(F("-----------------------------"));
        Serial.println(F("Scan a PICC to ADD or REMOVE to EEPROM"));
      }
    }
  } else {
    // Daca este master, se intra in program mode
    if (is_master(read_card)) {
      program_mode = true;

      Serial.println(F("Hello Master - Entered Program Mode"));
      
      uint8_t count = EEPROM.read(0);
      
      Serial.print(F("I have "));
      Serial.print(count);
      Serial.print(F(" record(s) on EEPROM"));
      Serial.println("");

      Serial.println(F("Scan a PICC to ADD or REMOVE to EEPROM"));
      Serial.println(F("Scan Master Card again to Exit Program Mode"));
      Serial.println(F("-----------------------------"));
    } else {
      // Daca gasesc ID-ul
      if (find_ID(read_card)) {
        // Permit accesul
        Serial.println(F("Welcome, You shall pass"));
        granted(LOCK_DELAY);
      } else {
        // Refuz accesul
        Serial.println(F("You shall not pass"));
        denied();
      }
    }
  }
}

void granted(uint16_t set_delay) {
  digitalWrite(BLUE_LED, LOW);
  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, HIGH);
  digitalWrite(TRANSISTOR, HIGH);
  delay(set_delay);

  digitalWrite(TRANSISTOR, LOW);
  delay(ACCESS_LED_DELAY);
}

void denied() {
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(BLUE_LED, LOW);
  digitalWrite(RED_LED, HIGH);
  delay(ACCESS_LED_DELAY);
}

uint8_t get_ID() {
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return 0;
  }
  
  if (!mfrc522.PICC_ReadCardSerial()) {
    return 0;
  }

  Serial.println(F("Scanned PICC's UID:"));

  // Stochez bytes cititi
  for (uint8_t i = 0; i < 4; ++i) {
    read_card[i] = mfrc522.uid.uidByte[i];
    Serial.print(read_card[i], HEX);
  }
  
  Serial.println("");
  mfrc522.PICC_HaltA();
  
  return 1;
}

void show_reader_details() {
  // Get the MFRC522 software version
  byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  
  Serial.print(F("MFRC522 Software Version: 0x"));
  Serial.print(v, HEX);
  
  if (v == 0x91)
    Serial.print(F(" = v1.0"));
  else if (v == 0x92)
    Serial.print(F(" = v2.0"));
  else
    Serial.print(F(" (unknown),probably a chinese clone?"));
  
  Serial.println("");
  
  // When 0x00 or 0xFF is returned, communication probably failed
  if ((v == 0x00) || (v == 0xFF)) {
    Serial.println(F("WARNING: Communication failure, is the MFRC522 properly connected?"));
    Serial.println(F("SYSTEM HALTED: Check connections."));
    
    // Visualize system is halted
    digitalWrite(GREEN_LED, LOW);  // Make sure green LED is off
    digitalWrite(BLUE_LED, LOW);   // Make sure blue LED is off
    digitalWrite(RED_LED, HIGH);   // Turn on red LED
    while (true); // do not go further
  }
}

void cycle_leds() {
  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, HIGH);
  digitalWrite(BLUE_LED, LOW);
  delay(LED_DELAY);
  
  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(BLUE_LED, HIGH);
  delay(LED_DELAY);
  
  digitalWrite(RED_LED, HIGH);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(BLUE_LED, LOW);
  delay(LED_DELAY);
}

void normal_mode_on() {
  digitalWrite(BLUE_LED, HIGH);
  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(TRANSISTOR, LOW);
}

void read_ID(uint8_t number) {
  // Extrag numarul byte-ului de start
  uint8_t start = (number * 4) + 2;

  // Citesc din memorie si stochez in vector
  for (uint8_t i = 0; i < 4; i++) {
    stored_card[i] = EEPROM.read(start + i);
  }
}

void write_ID(byte a[]) {
  // Daca iD-ul nu exista
  if (!find_ID(a)) {
    // Extrag numarul de chei stocate
    uint8_t num = EEPROM.read(0);
    // Calculez locul unde ar trebui sa stochez ID-ul
    uint8_t start = (num * 4) + 6;
    
    // Rescriu noul numar de chei
    num++;
    EEPROM.write(0, num);

    // Scriu in memorie ID-ul
    for (uint8_t j = 0; j < 4; ++j) {
      EEPROM.write(start + j, a[j]);
    }

    success_write();
    Serial.println(F("Succesfully added ID record to EEPROM"));
  } else {
    failed_write();
    Serial.println(F("Failed! There is something wrong with ID or bad EEPROM"));
  }
}

void delete_ID(byte a[]) {
  // Daca ID-ul nu exista
  if (!find_ID(a)) {
    failed_write();
    Serial.println(F("Failed! There is something wrong with ID or bad EEPROM"));
  } else {
    // Citesc numarul de inregistrari
    uint8_t num = EEPROM.read(0);   // Get the numer of used spaces, position 0 stores the number of ID cards
    uint8_t slot;       // Figure out the slot number of the card
    uint8_t start;      // = ( num * 4 ) + 6; // Figure out where the next slot starts
    uint8_t looping;    // The number of times the loop repeats
    uint8_t count = EEPROM.read(0); // Read the first Byte of EEPROM that stores number of cards
    uint8_t j;

    // Extrag slotul de la care trebuie sa sterg
    slot = findIDSLOT(a);   // Figure out the slot number of the card to delete
    // Calculez byte-ul de start
    start = (slot * 4) + 2;
    // Calculez numarul de pasi care trebuie facuti 
    looping = ((num - slot) * 4);

    // Scriu noul numar de inregistrari in memorie
    num--;
    EEPROM.write(0, num);

    // Mut intrarile in locurile eliberate
    for (j = 0; j < looping; ++j) {
      EEPROM.write(start + j, EEPROM.read(start + 4 + j));
    }

    // Scriu bytes de 0 in locul ultimei intrari
    for (uint8_t k = 0; k < 4; ++k) {
      EEPROM.write(start + j + k, 0);
    }

    success_delete();
    Serial.println(F("Succesfully removed ID record from EEPROM"));
  }
}

bool check_two (byte a[], byte b[]) {   
  for (uint8_t k = 0; k < 4; ++k) {   // Loop 4 times
    if (a[k] != b[k]) {
       return false;
    }
  }
  
  return true;  
}

uint8_t findIDSLOT(byte find[]) {
  // Citesc numarul de inregistrari
  uint8_t count = EEPROM.read(0);

  for (uint8_t i = 1; i <= count; ++i) {
    // Citesc ID-ul
    read_ID(i);

    // Verific daca este ID-ul cautat
    if (check_two(find, stored_card)) {
      return i;
    }
  }
}

bool find_ID(byte find[]) {
  // Citesc numarul de inregistrari
  uint8_t count = EEPROM.read(0);

  for (uint8_t i = 1; i <= count; ++i) {
    // Citesc ID-ul
    read_ID(i);

    // Verific daca este ID-ul cautat
    if (check_two(find, stored_card)) {
      return true;
    }
  }

  return false;
}

void success_write() {
  // Se aprinde LED-ul verde de 3 ori
  digitalWrite(BLUE_LED, LOW);
  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  delay(LED_DELAY);

  digitalWrite(GREEN_LED, HIGH);
  delay(LED_DELAY);
  
  digitalWrite(GREEN_LED, LOW);
  delay(LED_DELAY);
  
  digitalWrite(GREEN_LED, HIGH);
  delay(LED_DELAY);
  
  digitalWrite(GREEN_LED, LOW);
  delay(LED_DELAY);
  
  digitalWrite(GREEN_LED, HIGH);
  delay(LED_DELAY);
}

// Flashes the red LED 3 times to indicate a failed write to EEPROM
void failed_write() {
  // Se aprinde LED-ul rosu de 3 ori
  digitalWrite(BLUE_LED, LOW);
  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  delay(LED_DELAY);

  digitalWrite(RED_LED, HIGH);
  delay(LED_DELAY);
  
  digitalWrite(RED_LED, LOW);
  delay(LED_DELAY);
  
  digitalWrite(RED_LED, HIGH);
  delay(LED_DELAY);
  
  digitalWrite(RED_LED, LOW);
  delay(LED_DELAY);
  
  digitalWrite(RED_LED, HIGH);
  delay(LED_DELAY);
}

void success_delete() {
  // Se aprinde LED-ul albastru de 3 ori
  digitalWrite(BLUE_LED, LOW);
  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  delay(LED_DELAY);
  
  digitalWrite(BLUE_LED, HIGH);
  delay(LED_DELAY);
  
  digitalWrite(BLUE_LED, LOW);
  delay(LED_DELAY);
  
  digitalWrite(BLUE_LED, HIGH);
  delay(LED_DELAY);
  
  digitalWrite(BLUE_LED, LOW);
  delay(LED_DELAY);
  
  digitalWrite(BLUE_LED, HIGH);
  delay(LED_DELAY);
}

bool is_master(byte test[]) {
	return check_two(test, master_card);
}
