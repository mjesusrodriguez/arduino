/*
 *    Basado en https://github.com/wayoda/LedControl
 *    A library for controling Leds with a MAX7219/MAX7221
 *    Copyright (c) 2007 Eberhard Fahle
 * 
 *    Permission is hereby granted, free of charge, to any person
 *    obtaining a copy of this software and associated documentation
 *    files (the "Software"), to deal in the Software without
 *    restriction, including without limitation the rights to use,
 *    copy, modify, merge, publish, distribute, sublicense, and/or sell
 *    copies of the Software, and to permit persons to whom the
 *    Software is furnished to do so, subject to the following
 *    conditions:
 * 
 *    This permission notice shall be included in all copies or 
 *    substantial portions of the Software.
 * 
 *    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *    OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *    HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *    WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *    OTHER DEALINGS IN THE SOFTWARE.
 */


#include "MatrizLed.h"

//the opcodes for the MAX7221 and MAX7219
#define OP_DECODEMODE 9
#define OP_INTENSITY 10
#define OP_SCANLIMIT 11
#define OP_SHUTDOWN 12
#define OP_DISPLAYTEST 15

MatrizLed::MatrizLed() {
}

void MatrizLed::begin(int dataPin, int clkPin, int csPin, int numDevices) {
  SPI_MOSI = dataPin;
  SPI_CLK = clkPin;
  SPI_CS = csPin;

  if (numDevices <= 0 || numDevices > 8)
    numDevices = 8;

  maxDevices = numDevices;

  pinMode(SPI_MOSI, OUTPUT);
  pinMode(SPI_CLK, OUTPUT);
  pinMode(SPI_CS, OUTPUT);
  digitalWrite(SPI_CS, HIGH);
  
  for (int i = 0; i < 64; i++)
    status[i] = 0x00;

  for (int i = 0; i < maxDevices; i++) {
    spiTransfer(i, OP_DISPLAYTEST, 0);
    setScanLimit(i, 7);
    spiTransfer(i, OP_DECODEMODE, 0);
    clearDisplay(i);
  }
  
  apagar();
  setIntensidad(6);
  encender();
  borrar();
}

int MatrizLed::getDeviceCount() {
  return maxDevices;
}

void MatrizLed::setModelo(int m){
  modelo = m;
}

// Borra toda la información que se almacena para mostrarse en las pantallas
void MatrizLed::borrar() {
  for (int address = 0; address < maxDevices; address++) {
    clearDisplay(address);
  }
}

// Establece un valor de intensidad lumisona a todos los displays
void MatrizLed::setIntensidad(int intensidad) {
  for (int address = 0; address < getDeviceCount(); address++) {
    setIntensity(address, intensidad);
  }
}

// Apaga la pantalla pero sin borrar la información
void MatrizLed::apagar() {
  for (int address = 0; address < getDeviceCount(); address++) {
    shutdown(address, true);
  }
}

// Enciende la pantalla y muestra la información almacenada
void MatrizLed::encender() {
  for (int address = 0; address < getDeviceCount(); address++) {
    shutdown(address, false);
  }
}


/* Método que desplaza una frase por la pantalla
 * El mensaje aparecerá por la última fila desplazándose hacia la izquierda
 * El valor por defecto de la pausa entre movimientos es de 500ms
 */
void MatrizLed::escribirFraseScroll(const char* frase, unsigned long pausa) {
  //La posición de la última fila es el número de filas por display x número de displays 
  int ultFila = maxDevices * 8;
  int nPasos = strlen(frase) * 8;
  //En cada iteración se escribe el mensaje de forma completa pero en una posición anterior
  for (int i = ultFila ; i > -nPasos; i--) {
    escribirFrase(frase, i);
    delay(pausa);
  }
}

void MatrizLed::escribirFraseCompactaScroll(const char* frase, unsigned long pausa) {
  //La posición de la última fila es el número de filas por display x número de displays 
  int ultFila = maxDevices * 8;
  int nPasos = strlen(frase) * 8;
  //En cada iteración se escribe el mensaje de forma completa pero en una posición anterior
  for (int i = ultFila ; i > -nPasos; i--) {
    escribirFraseCompacta(frase, i);
    delay(pausa);
  }
}

/* Método que recibe un velor numérico entero y lo representa como una frase
 * La posición indica la columna de comienzo. Si no se indica se toma el valor por defecto 0
 */
void MatrizLed::escribirCifra(int cifra, int posicion) {
  //Array donde se almacenará cada caracter de la cifra
  char formateado[maxDevices + 1];  
  //Método convierte valor numérico a cadena de caracteres. 3er arg es la precisión (nº de decimales)
  dtostrf(cifra, maxDevices, 0, formateado);
  //Se llama al método encargado de escribir los caracteres a partir de una posición
  escribirFrase(formateado, posicion);
}


/* Método que escribe una cadena de caracteres caracter por caracter
 */
void MatrizLed::escribirFrase(const char* frase, int posicion) {
  for (size_t i = 0; i < strlen(frase); i++) {
    escribirCaracter(frase[i], (i * 8) + posicion);
  }
}

void MatrizLed::escribirFrase(const String frase, int posicion) {
  for (size_t i = 0; i < frase.length(); i++) {
    escribirCaracter(frase[i], (i * 8) + posicion);
  }
}

/* Método que escribe una cadena de caracteres caracter por caracter de forma compacta.
 * Sobreescribe el nuevo carácter en la última columna del anterior
 */
void MatrizLed::escribirFraseCompacta(const char* frase, int posicion) {
  escribirCaracter(frase[0], posicion-1);
  for (size_t i = 1 ; i < strlen(frase) ; i++) {
    escribirCaracter(frase[i], (i * 8) -(i*2) + posicion);
  }
}

void MatrizLed::escribirCaracter(char caracter, int posicion) {
  byte codigoCaracter[8];
  obtenerBitsCaracter(caracter, codigoCaracter);
  escribirBits(codigoCaracter, posicion);
}

void MatrizLed::obtenerBitsCaracter(char caracter, byte codigo[]){
  byte asterisco[] = { B00000000, B00001000, B00101010, B00011100, B01110111, B00011100, B00101010, B00001000 };
  
  if(caracter == ' '){
    for (int i = 0; i < 8; i++) {
      codigo[i] = 0;
    }
  }else{
    byte * nombreTabla = 0;
    char referencia;
    if (caracter >= 'A' && caracter <= 'Z') {
    referencia = 'A';
    nombreTabla = tablaCaracteresMayuscula;
    }else if(caracter >= 'a' && caracter <= 'z') {
    referencia = 'a';
    nombreTabla = tablaCaracteresMinuscula;
    }else if(caracter >= '0' && caracter <= '9') {
    referencia = '0';
    nombreTabla = tablaNumeros;
    }

    if(nombreTabla == 0){
      memcpy(codigo, asterisco, sizeof(asterisco[0]) * 8);
    }else{
      int posicion = ((int) caracter - referencia)*8;
      obtenerSecuenciaTabla(nombreTabla, posicion, codigo);
    }
  }
}

void MatrizLed::obtenerSecuenciaTabla(byte nombreTabla[], int posTabla, byte codigo[]){
    for (int i = 0; i < 8; i++) {
      codigo[i] = pgm_read_byte_near(nombreTabla + i + posTabla);
    }
}

// Metodo que permite representar un código representado por una matriz de 8x8 bits
void MatrizLed::escribirBits(const byte codigo[], int posicion){
  byte copia[8];
  memcpy(copia, codigo, sizeof(codigo[0]) * 8);
  rotarCodigo(copia);

  int address = posicion/8;
  int posDisplay;
  switch(modelo){
    case 0:
      posDisplay = 7-(posicion%8);
      for (int i = 7; i >= 0 ; i--) {
        if (address > maxDevices-1)
          return;

        setColumn(address, posDisplay, copia[i]);
        if(posDisplay==0){
          posDisplay=7;
          address++;
        } else {
          posDisplay--;
        }
      }
      break;
    case 1:
      posDisplay = posicion%8;
      for (int i = 0; i <8 ; i++) {
        if (address > maxDevices-1)
          return;

        setColumn(address, posDisplay, copia[i]);
        if(posDisplay==7){
          posDisplay=0;
          address++;
        } else {
          posDisplay++;
        }
      }
      break;
  }
/* Ejemplos de cómo se representa un caracter que comienza desplazado
  POS DESPLAZADA = 4
  setColumn(1, 4, codigocaracter[0]);
  setColumn(1, 5, codigocaracter[1]);
  setColumn(1, 6, codigocaracter[2]);
  setColumn(1, 7, codigocaracter[3]);
  setColumn(0, 0, codigocaracter[4]);
  setColumn(0, 1, codigocaracter[5]);
  setColumn(0, 2, codigocaracter[6]);
  setColumn(0, 3, codigocaracter[7]);

  POS DESPLAZADA = 2
  setColumn(1, 6, codigocaracter[0]);
  setColumn(1, 7, codigocaracter[1]);
  setColumn(0, 0, codigocaracter[2]);
  setColumn(0, 1, codigocaracter[3]);
  setColumn(0, 2, codigocaracter[4]);
  setColumn(0, 3, codigocaracter[5]);
  setColumn(0, 4, codigocaracter[6]);
  setColumn(0, 5, codigocaracter[7]); */
}


// Método que se comunican con el controlador a bajo nivel
void MatrizLed::shutdown(int addr, bool b) {
  if (addr < 0 || addr >= maxDevices)
    return;
  if (b)
    spiTransfer(addr, OP_SHUTDOWN, 0);
  else
    spiTransfer(addr, OP_SHUTDOWN, 1);
}
void MatrizLed::setScanLimit(int addr, int limit) {
  if (addr < 0 || addr >= maxDevices)
    return;
  if (limit >= 0 && limit < 8)
    spiTransfer(addr, OP_SCANLIMIT, limit);
}
void MatrizLed::setIntensity(int addr, int intensity) {
  if (addr < 0 || addr >= maxDevices)
    return;
  if (intensity >= 0 && intensity < 16)
    spiTransfer(addr, OP_INTENSITY, intensity);
}
void MatrizLed::clearDisplay(int addr) {
  int offset;
  if (addr < 0 || addr >= maxDevices)
    return;
  offset = addr * 8;
  for (int i = 0; i < 8; i++) {
    status[offset + i] = 0;
    spiTransfer(addr, i + 1, status[offset + i]);
  }
}


void MatrizLed::setLed(int addr, int row, int column, bool state) {
  int offset;
  byte val = 0x00;

  if (addr < 0 || addr >= maxDevices)
    return;
  if (row < 0 || row > 7 || column < 0 || column > 7)
    return;
  offset = addr * 8;
  val = B10000000 >> column;
  if (state)
    status[offset + row] = status[offset + row] | val;
  else {
    val = ~val;
    status[offset + row] = status[offset + row] & val;
  }
  spiTransfer(addr, row + 1, status[offset + row]);
}

void MatrizLed::setRow(int addr, int row, byte value) {
  int offset;
  if (addr < 0 || addr >= maxDevices)
    return;
  if (row < 0 || row > 7)
    return;
  offset = addr * 8;
  status[offset + row] = value;
  spiTransfer(addr, row + 1, status[offset + row]);
}

void MatrizLed::setColumn(int addr, int col, byte value) {
  if (addr < 0 || addr >= maxDevices || col < 0 || col > 7)
    return;
  for (int row = 0; row < 8; row++) {
    setLed(addr, row, col, bitRead(value,row));
  }
}

void MatrizLed::spiTransfer(int addr, volatile byte opcode, volatile byte data) {
  //Create an array with the data to shift out
  int offset = addr * 2;
  int maxbytes = maxDevices * 2;

  for (int i = 0; i < maxbytes; i++)
    spidata[i] = (byte)0;
  //put our device data into the array
  spidata[offset + 1] = opcode;
  spidata[offset] = data;
  //enable the line
  digitalWrite(SPI_CS, LOW);
  //Now shift out the data
  for (int i = maxbytes; i > 0; i--)
    shiftOut(SPI_MOSI, SPI_CLK, MSBFIRST, spidata[i - 1]);
  //latch the data onto the display
  digitalWrite(SPI_CS, HIGH);
}


// Métodos para girar códigos. Pendiente de ampliar para poder colocar la matriz en vertical, espejo, hacia abajo...
void MatrizLed::rotarCodigo(byte codigo[]){
    switch(modelo){
      case 0:
        rotar_antihorario_codigo(codigo);
        break;
      case 1: 
        rotar_horario_codigo(codigo);
        break;
      case 2: 
        espejo_codigo(codigo);
        break;
    }
}


void MatrizLed::rotar_antihorario_codigo(byte original[8]) {
  byte temporal[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  for (int i = 0; i < 8; i++) {
    bitWrite(temporal[7], 7-i, bitRead(original[i], 7));
    bitWrite(temporal[6], 7-i, bitRead(original[i], 6));
    bitWrite(temporal[5], 7-i, bitRead(original[i], 5));
    bitWrite(temporal[4], 7-i, bitRead(original[i], 4));
    bitWrite(temporal[3], 7-i, bitRead(original[i], 3));
    bitWrite(temporal[2], 7-i, bitRead(original[i], 2));
    bitWrite(temporal[1], 7-i, bitRead(original[i], 1));
    bitWrite(temporal[0], 7-i, bitRead(original[i], 0));
  }
  memcpy(original, temporal, sizeof(temporal[0]) * 8);
}

void MatrizLed::rotar_horario_codigo(byte original[8]) {
  byte temporal[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  for (int i = 0; i < 8; i++) {
    bitWrite(temporal[7], i, bitRead(original[i], 0));
    bitWrite(temporal[6], i, bitRead(original[i], 1));
    bitWrite(temporal[5], i, bitRead(original[i], 2));
    bitWrite(temporal[4], i, bitRead(original[i], 3));
    bitWrite(temporal[3], i, bitRead(original[i], 4));
    bitWrite(temporal[2], i, bitRead(original[i], 5));
    bitWrite(temporal[1], i, bitRead(original[i], 6));
    bitWrite(temporal[0], i, bitRead(original[i], 7));
  }
  memcpy(original, temporal, sizeof(temporal[0]) * 8);
}

void MatrizLed::espejo_codigo(byte original[8]) {
  byte temporal[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  for (int i = 0; i < 8; i++) {
    bitWrite(temporal[7], i, bitRead(original[0], i));
    bitWrite(temporal[6], i, bitRead(original[1], i));
    bitWrite(temporal[5], i, bitRead(original[2], i));
    bitWrite(temporal[4], i, bitRead(original[3], i));
    bitWrite(temporal[3], i, bitRead(original[4], i));
    bitWrite(temporal[2], i, bitRead(original[5], i));
    bitWrite(temporal[1], i, bitRead(original[6], i));
    bitWrite(temporal[0], i, bitRead(original[7], i));
  }
  memcpy(original, temporal, sizeof(temporal[0]) * 8);
}
