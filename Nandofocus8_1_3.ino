//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
//
//  NandofocusV8
//
//  Focuser based in Arduino HW and Robofocus (open source) 
//          serial protocol
//
//  Site old: http://es.groups.yahoo.com/group/nandofocus_group/
//  New Site: https://github.com/Nandorroloco/Nandofocus
//
//  License: http://creativecommons.org/licenses/by-sa/3.0/deed.es
//
//  Created by; Nandorroloco
//  year: 2013
//////////////////////////////////////////////////////////////////
//  beta: version 811   24/02/2013
// --------------------------------
//  Version 812         01/05/2021
//  Se añade compilación condicional para generar una versión para USB
//  elimina los comando AT iniciales, contesta en primera instancia con la 
//  versión del programa. Para generar esta opición descomentar la siguiente línea
//  Versión 813         12/02/2022
//  Según qué motor posicionarse a velocidad máxima, pierde pasos, cambio para que 
//  el posicionamiento sea a velocidad media, será más lento pero más preciso
#define USB_VERSION
//////////////////////////////////////////////////////////////////

#include <TimerOne.h>
#include <EEPROM.h>


#define MAX_CADENA 128

#define SLOW 40
#define MSLW 20
#define MIDD 10
#define FAST 3

#define B_UP   0b00100000
#define B_SUP  0b00010000
#define N_UP   0b00010001
#define B_HUP  0b00110000
#define B_DWN  0b01000000
#define N_DWN  0b01000001
#define B_SDWN 0b10000000
#define B_HDWN 0b11000000
#define B_RST ((B_SUP)|(B_SDWN))

struct mieeprom_t{
  int reset;
  int paso;
  unsigned int posicion;
  unsigned int bsdir;
  unsigned int backslash;
  unsigned int limit;
//  int sw1, sw2, sw3, sw4;
};

mieeprom_t mieeprom;
mieeprom_t reset = {1, 0, 30000, '2', 32, 65000 };

#define pos_fL (0x11)
#define pos_fH (0x12)

int version = 813;

int led = 13;
int fase1 = 11;
int fase2 = 10;
int fase3 = 9;
int fase4 = 8;

int pin_B0 = 7; 
int pin_B1 = 6;
int pin_B2 = 5;
int pin_B3 = 4;

int sensorPin = A0;

int pulsado;
int boton;
int direccion = 0;
int actua     = 0;
int paso=0;
int tick;
int pulsos = SLOW;
int compensa = 0;
unsigned int c_int = 30000, c_prg = 30000;
int diff = 0;
int rbf = false;
//int i;
unsigned int N = '2';
unsigned int bsdir;
unsigned int posicion;
unsigned int backslash = 30;
unsigned int limit = 65000;
//long temp = 0L;
int temp = 0;
int rst;
int sw1, sw2, sw3, sw4;
char comando[10]={
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
char salida[10]={
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
char *ptrc;
char cmd;
int c_relax = 0;

char buffer_e[10];
int pointer;
int pntr;
char in_c, segun;

char tmp_b[128]; // resulting string limited to 128 chars

void carga_focuser( )
{
  int i;
  byte *v;
  v = (byte *) &mieeprom;
  for ( i=0; i < sizeof(mieeprom); i++)
    *v++ = (byte) EEPROM.read(i);
  rst       = (mieeprom.reset);
  paso      = (mieeprom.paso);
  c_int     = (mieeprom.posicion);
  bsdir     = (mieeprom.bsdir);
  backslash = (mieeprom.backslash);
  limit     = (mieeprom.limit);
//  sw1       = mieeprom.sw1;
//  sw2       = mieeprom.sw2;
//  sw3       = mieeprom.sw3;
//  sw4       = mieeprom.sw4;
}

void put_eeprom()
{
  unsigned int i;
  byte *v;
  v = (byte *) &mieeprom;
  for (i=0; i< sizeof(mieeprom);i++)
    EEPROM.write(i,*v++);
}

void set_focuser()
{
  mieeprom.reset    = rst;
  mieeprom.paso     = paso;
  mieeprom.posicion = c_int;
  mieeprom.bsdir    = bsdir;
  mieeprom.backslash= backslash;
  mieeprom.limit    = limit;
//  mieeprom.sw1      = sw1;
//  mieeprom.sw2      = sw2;
//  mieeprom.sw3      = sw3;
//  mieeprom.sw4      = sw4;
  put_eeprom();
}

void reset_focuser()
{
  int i;
  memcpy(&mieeprom,&reset,sizeof(mieeprom));
  put_eeprom();
  for( i = 0; i<10; i++ )
  {
  digitalWrite(led, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(20);               // wait for a 1/20 second
  digitalWrite(led, LOW);    // turn the LED off by making the voltage LOW
  delay(20);               // wait for a 1/20 second
  }
}


void relaja_motor()
{
  digitalWrite(fase1, LOW);
  digitalWrite(fase2, LOW);  
  digitalWrite(fase3, LOW);
  digitalWrite(fase4, LOW);
}

void excita_motor(int paso)
{
  switch(paso)
  {
  case 0:         //paso 0
  digitalWrite(fase1, HIGH);
  digitalWrite(fase2, HIGH);  
  digitalWrite(fase3, LOW);
  digitalWrite(fase4, LOW);
    break;
  case 1:         //paso 1
  digitalWrite(fase1, LOW);
  digitalWrite(fase2, HIGH);  
  digitalWrite(fase3, HIGH);
  digitalWrite(fase4, LOW);
    break;
  case 2:         //paso 2
  digitalWrite(fase1, LOW);
  digitalWrite(fase2, LOW);  
  digitalWrite(fase3, HIGH);
  digitalWrite(fase4, HIGH);
    break;
  case 3:         //paso 3
  digitalWrite(fase1, HIGH);
  digitalWrite(fase2, LOW);  
  digitalWrite(fase3, LOW);
  digitalWrite(fase4, HIGH);
    break;
  }
}

char lee()
{
  char rc = 0;
  if ( pntr <= pointer )
    rc = buffer_e[pntr++];
  return(rc);
}

int siguiente_paso(int direccion)
{
int rc=0;

  if ( direccion )
  {
    if ( paso < 3 )
      paso++;
    else
      paso = 0;
    if ( c_int < limit )
      c_int++;
    else
      return(rc);
  }
  else
  {
    if ( paso > 0 )
      paso--;
    else
      paso = 3;
    if ( c_int > 1 )
      c_int--;
    else
      return(rc);
  }
  rc = 1;
  return(rc);
}

void escribe(char *s)
{
  int i;
  byte chs=0;
  for( i=0; i<8 ; i++)
    Serial.write((byte)s[i]);
  chs=(byte)s[0]+(byte)s[1]+(byte)s[2]+(byte)s[3]+(byte)s[4]+(byte)s[5]+(byte)s[6]+(byte)s[7];
  Serial.write((byte)chs);
  Serial.println();
}

void responde( char op, unsigned int num )
{
  sprintf(salida,"F%c%06u", op, num);
  escribe(salida);
  switch( op )
  {
  case 'D':
  case 'S':
  case 'L':
  case 'B':
    set_focuser();
    break;
  }
}



void rbf_in()
{
  if (( c_int - posicion - ( bsdir == '3'? backslash: 0)) >= 1 ) 
  {
    switch( bsdir)
    {
    case '3':
      c_prg = c_int - posicion - backslash;
      compensa = 1;
      break;
    case '2':
      c_prg = c_int - posicion;
      compensa = 0;
      break;
    default:
      c_prg = c_int;
      break;
    }
    direccion = 0;
    actua = true;
  }
}

void rbf_out()
{
  if (( c_int + posicion + ( bsdir == '2'?  backslash: 0)) <= limit) 
  {
    switch( bsdir)
    {
    case '3':
      c_prg = c_int + posicion;
      compensa = 0;
      break;
    case '2':
      c_prg = c_int + posicion + backslash;
      compensa = 1;
      break;
    default:
      c_prg = c_int;
      break;
    }
    direccion = 1;
    actua = true; 
  }
}

void rbf_go()
{
  if ( (posicion == 0) || (posicion == c_int) )
  {
    responde('D', c_int);
  }
  else
  {
    if ( posicion < c_int )  // veo quÃ© funciÃ³n invocar y ajusto la variable posiciÃ³n a un mov. relativo
    {
      posicion = c_int - posicion;
      rbf_in();
    }
    else
    {
      posicion = posicion - c_int;
      rbf_out();
    }
  }
}

void rbf_set()
{
  if (( posicion >= 1 )&& (posicion <= limit))
    c_int = posicion;  
  responde('S', c_int);
}

void rbf_lim()
{
  if ( (posicion >= c_int) && (posicion <= 65000))
    limit = posicion;
  responde('L', limit);
}


void activa_switches()
{
  if (sw1 == 2 )
      digitalWrite(A1, HIGH);
  else
      digitalWrite(A1, LOW);
  if (sw2 == 2 )
      digitalWrite(A2, HIGH);
  else
      digitalWrite(A2, LOW);
  if (sw3 == 2 )
      digitalWrite(A3, HIGH);
  else
      digitalWrite(A3, LOW);
  if (sw4 == 2 )
      digitalWrite(A4, HIGH);
  else
      digitalWrite(A4, LOW);   
}      

void rbf_pwr()
{
  if (posicion != 0)
    {
    if ( comando[3] == '2' )
      sw1=2;
    else
      sw1=1;
    if ( comando[4] == '2' )
      sw2=2;
    else
      sw2=1;
    if ( comando[5] == '2' )
      sw3=2;
    else
      sw3=1;
    if ( comando[6] == '2' )
      sw4=2;
    else
      sw4=1;      
    activa_switches(); 
    } 
responde('P', sw1*1000+sw2*100+sw3*10+sw4 );
}

void robofocus()
{
  int i;
  char c;
  posicion=0;
  pulsos = MIDD;        // 12-02-2022  a velocidad media porque hay motores que pierden pasos
  ptrc=comando;
  for (i = 0; i<8; i++)
    *ptrc++ = lee();
  ptrc=comando;
  cmd = *ptrc++;
  N=*ptrc++;   //salto un caracter es 
  for (i=0; i<5;i++)
  {
    c = *ptrc++;
    if ((c >= '0') && ( c <= '9' ))
      posicion = posicion*10 + ( c - '0');
  }
  switch( cmd )
  {
  case 'V':
    responde('V',version);
    break;
  case 'G':
    rbf_go();
    break;
  case 'I':
    rbf_in();
    break;
  case 'O':
    rbf_out();
    break;
  case 'S':
    rbf_set();
    break;
  case 'L':
    rbf_lim();
    break;
  case 'B':
    if ( posicion != 0 )
    {
      bsdir= N;
      backslash = posicion;  
    } 
    set_focuser();
    sprintf(salida,"FB%c%05u", bsdir, backslash);
    escribe(salida);
    break;
  case 'R':
    if ( posicion == 0 )
    {
      reset_focuser();
      responde('R',11111);
    } 
    break;
  case 'C':
    responde('C',11);
    break;     
  case 'P':
    rbf_pwr();
    break; 
  case 'T':                  
    temp = analogRead(sensorPin); 
    responde('T', (724-temp));
    break;
  default:
    sprintf( tmp_b, "F_Error\r\n");
    Serial.print(tmp_b);
    break;
  }
}


void int_comando()
{
  char c;
  pntr=0;
  c = lee();
  switch ( c )
  {
  case ':' :    //Primera letra
    rbf = false;
    c = lee();
    if( c == 'F' )
    {
      if (( segun = lee())!= '?')
      {
        c = lee();
        if( c == '#' )   // tercera letra y confirma
        {
          switch( segun )         //Segunda letra
          {
          case '-':
            direccion = 0;
            actua = true;
            break;
          case '+':
            direccion = 1;
            actua = true;
            break;
          case 'Q':
            actua = false;
            responde('D', c_int);
            break;
          case 'F':
          case '4':
            pulsos = FAST;
            break;
          case 'S':
          case '1':
            pulsos = SLOW;
            break;
          case '2':
            pulsos = MSLW;
            break;
          case '3':
            pulsos = MIDD;
            break;
          case 'v':
            sprintf( tmp_b, "Nandofocus v%u", version);
            Serial.print(tmp_b);
            break;
          }
        }
      }
      else
      {
          sprintf( tmp_b, "\rdir,\tpls,\tc_prg,\tc_int,\tlimit,\tbslash,\tbsdir\r\n");
          Serial.print(tmp_b);
          sprintf( tmp_b, "%3d,\t%3d,\t%05u,\t%05u,\t%05u,\t%u,\t%d\r\n",
                      direccion, pulsos, c_prg, c_int, limit, backslash, bsdir );
          Serial.print(tmp_b);            
      }
    }  
    break;

  case 'F':    // comandos del robofocus...
    robofocus();
    break;
  case '?':
    sprintf( tmp_b, "Nandofocus v%u", version);
    Serial.print(tmp_b);
    Serial.print ( "\r\n:F{+|-|Q}#\t\t Mov: Out, In, Stop" );
    Serial.print ( "\r\n:F{[1,2,3,4]|F|S}#\t Vel: lento a rapido, fast, slow" );
    Serial.print ( "\r\n:F?|?\t\t\t Cons: estado, ayuda" );
    Serial.print ( "\r\nFXNNNNNNZ\t\t Comandos de robofocus\r\n" );
    break;
  default:
    actua = false;
    break;
  }
}


void interrup_t1()
{
  if ( tick == 0 )
  {
    if ( actua )
    {
      if ( siguiente_paso( direccion ) == 1 )
        excita_motor(paso);
      if( direccion )
        Serial.print('O');
      else
        Serial.print('I');

      diff = c_prg - c_int;
      actua = ( diff != 0 );
      if ( diff == 0 )
      {
        if( compensa )
        {
          compensa = 0;
          switch( bsdir)
          {
          case '3':
            c_prg = c_int + backslash;
            direccion = 1;
            actua = true;
            break;
          case '2':
            c_prg = c_int - backslash;
            direccion = 0;
            actua = true;
            break;
          }
        }
        else
        {
          if( boton == 0 )
          {
            responde('D', c_int);
          }
        }
      }
    }
    tick = pulsos;
  }
  else
  {
    if ( actua )
    {
      tick--;
      if( tick > (pulsos /2)   )
          digitalWrite(led, HIGH);   // turn the LED on
      else
          digitalWrite(led, LOW);    // turn the LED off
    }
    else
      digitalWrite(led, HIGH );   // turn the LED on
  }
  if ( ! actua && ( c_relax > 0) )
  {
    c_relax--;
    if ( c_relax == 0 )
    {
      relaja_motor();
    }
  }
  else
    c_relax = 1000;
}

int botones()
{
  int rc = 0;
 
   if( digitalRead(pin_B0)== LOW) rc += B_SUP;
   if( digitalRead(pin_B1)== LOW) rc += B_UP;
   if( digitalRead(pin_B2)== LOW) rc += B_DWN;
   if( digitalRead(pin_B3)== LOW) rc += B_SDWN;
    
 return(rc);
}

void pulsados()
{

  boton = botones();
  switch( boton )
  {
  case B_DWN:    // boton down
    pulsos = SLOW;
    direccion = 1;
    break;
  case B_UP:    // boton up
    pulsos = SLOW;
    direccion = 0;
    break;
  case B_SDWN:    // boton superdown
    pulsos = MIDD;
    direccion = 1;
    break;
  case B_SUP:    // boton superup
    pulsos = MIDD;
    direccion = 0;
    break;
  case B_HDWN:
    pulsos = FAST;
    direccion = 1;
    break;
  case B_HUP:    // si se aprietan up y superup a la vez
    pulsos = FAST;
    direccion = 0;
    break;
  }

  if ( boton != 0 )
  {
    actua = true;
    if ( !pulsado )
      tick = 0;
    pulsado = true;
    compensa = 0;
  }
  else
  {
    if ( pulsado )
    {
      actua = false;
      responde('D', c_int);
    }
    pulsado = false;
  }
}


void setup() {

  
  // initialize the button pin as a input:
  pinMode(pin_B0, INPUT);
  digitalWrite(pin_B0, HIGH);       // turn on pullup resistors
  pinMode(pin_B1, INPUT);
  digitalWrite(pin_B1, HIGH);       // turn on pullup resistors
  pinMode(pin_B2, INPUT);
  digitalWrite(pin_B2, HIGH);       // turn on pullup resistors
  pinMode(pin_B3, INPUT);
  digitalWrite(pin_B3, HIGH);       // turn on pullup resistors
  // initialize the LED as an output:
  pinMode(led, OUTPUT);  
  digitalWrite(led, HIGH);   // turn the LED on
  
  // initialize the motor phases:
  pinMode(fase1, OUTPUT);
  pinMode(fase2, OUTPUT);
  pinMode(fase3, OUTPUT);
  pinMode(fase4, OUTPUT);
  
  // initialize switches
  pinMode(A1, OUTPUT);
  pinMode(A2, OUTPUT);
  pinMode(A3, OUTPUT);
  pinMode(A4, OUTPUT);

  if ( botones() == B_RST )
  {
    reset_focuser();
  }
  else
  {
    carga_focuser();
    if ( mieeprom.reset !=1 )
      reset_focuser();
  }

  excita_motor(paso);
  activa_switches();
  // programita
 
  // initialize timer1

  Timer1.initialize(1000);
  Timer1.attachInterrupt(interrup_t1);
  
  pointer=0;

  // initialize serial communication:
  Serial.begin(9600L);
  digitalWrite(led, LOW);   // turn the LED off
#ifndef USB_VERSION
  //Iniciamos comunicacion con modulo bluetooth mediante comandos AT  
  Serial.print("AT");  
  //Espera de 1 segundo según datasheet entre envio de comandos AT  
  delay(1000);  
  digitalWrite(led, HIGH);   // turn the LED on
  //Cambio de nombre donde se envia AT+NAME y seguido el nombre que deseemos  
  Serial.print("AT+NAMEnandofocusV8");  
  //Espera de 1 segundo según datasheet entre envio de comandos AT  
  delay(1000);  
  digitalWrite(led, LOW);   // turn the LED off
  /*Cambio de la velocidad del modulo en baudios  
  Se envia AT+BAUD y seguido el numero correspondiente:  
     
   1 --> 1200 baudios   
   2 --> 2400 baudios  
   3 --> 4800 baudios  
   4 --> 9600 baudios (por defecto)  
   5 --> 19200 baudios  
   6 --> 38400 baudios  
   7 --> 57600 baudios  
   8 --> 115200 baudios  
  */ 
  
  Serial.print("AT+BAUD4");  
  //Espera de 1 segundo según datasheet entre envio de comandos AT  
  delay(1000);  
  digitalWrite(led, HIGH);   // turn the LED on
  //Configuracion Password, se envia AT+PIN y seguido password que queremos  
   Serial.print("AT+PIN1234"); 
   delay(1000); 
#else
  Serial.print("Inicializado!");
#endif
  digitalWrite(led, LOW);   // turn the LED off
  // emite la versión del Nandofocus con el protocolo Robofocus
  responde('V',version);
  digitalWrite(led, HIGH);   // turn the LED on
}

  void loop()
  {
    pulsados();
    if ( Serial.available() > 0 )
        {
          actua = false;
          in_c= Serial.read();
          switch (in_c)
          {
            case '?':
            case '\n':
            case '\r':
            case '#':  
                buffer_e[pointer++]= in_c;        
                int_comando();
                pointer = 0;
                break;
            case ':':
                pointer =0;   
                buffer_e[pointer++]= in_c;
                break;
            case 'F':
                if (buffer_e[pointer-1] != ':')
                  {
                   pointer =0;   
                   buffer_e[pointer++]= in_c;
                   break;
                  }
            default:
                buffer_e[pointer++]= in_c;
                if (pointer==9)
                  {
                    int_comando();
                    pointer=0;
                  }  
             }  
        }
        delay(5);
  }
