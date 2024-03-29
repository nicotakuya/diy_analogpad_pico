// DIY-ANALOGPAD-PICO
// for Raspberry Pi Pico(Arduino-Pico)
// (C)2024 takuya matsubara.

#include <Wire.h>
#include <SPI.h>
#include <hardware/pwm.h>
#include "8x8font.h"

#define NOTUSE     0
#define SSD1306    3  // OLED Display
#define DISP_TYPE    3 // DISPLAY TYPE
#define DISP_ROTATE  2 // TURN SCREEN(0=normal / 1=turn90 / 2=turn180 / 3=turn270)

#if DISP_TYPE==SSD1306
#define VRAMWIDTH   128 // width[pixel]
#define VRAMHEIGHT  64  // height[pixel]
#define FONTSIZE    2
#endif

#define VRAMSIZE   (VRAMWIDTH*VRAMHEIGHT*2) // 115200 Bytes

#if DISP_ROTATE==0
#define VRAMXRANGE VRAMWIDTH
#define VRAMYRANGE VRAMHEIGHT
#endif
#if DISP_ROTATE==1
#define VRAMYRANGE VRAMWIDTH
#define VRAMXRANGE VRAMHEIGHT
#endif
#if DISP_ROTATE==2
#define VRAMXRANGE VRAMWIDTH
#define VRAMYRANGE VRAMHEIGHT
#endif
#if DISP_ROTATE==3
#define VRAMYRANGE VRAMWIDTH
#define VRAMXRANGE VRAMHEIGHT
#endif
#define VRAMXMAX  (VRAMXRANGE-1)
#define VRAMYMAX  (VRAMYRANGE-1)

unsigned char vram[VRAMSIZE];
int putch_x = 0;
int putch_y = 0;
unsigned int putch_color = 0xffff;
int putch_zoom = 1;

void disp_init(void);
void disp_update(void);
void vram_cls(void);
unsigned int vram_point(int x,int y);
void vram_pset(int x,int y,unsigned int color);
void vram_line(int x1 ,int y1 ,int x2 ,int y2 ,unsigned int color);
void vram_fill(int x1 ,int y1 ,int x2 ,int y2 ,unsigned int color);
void vram_locate(int textx, int texty);
void vram_textcolor(unsigned int color);
void vram_putch(unsigned char ch);
void vram_putstr(unsigned char *p);
void vram_putdec(unsigned int num);
void vram_puthex(unsigned int num);
void vram_scroll(int xd,int yd);
void vram_spput(int x,int y, int num,unsigned int color);
void vram_spclr(int x,int y);
unsigned int color16bit(int r,int g,int b);

// ABS
int fnc_abs(int a)
{
  if(a<0)a = -a;
  return (a);
}

// SGN
int fnc_sgn(int a)
{
  if(a<0)return(-1);
  return (1);
}

// VRAMキャプチャ
void capture(void)
{
  unsigned int color;
  int x,y,incomingByte;

  // request
  if (Serial.available() == 0) return;
  incomingByte = Serial.read();
  if(incomingByte != 0x43)return;
  // responce
  Serial.write(VRAMXRANGE);
  Serial.write(VRAMYRANGE);
  for(y=0; y<VRAMYRANGE; y++){
    for(x=0; x<VRAMXRANGE; x++){
      color = vram_point(x,y);
      Serial.write(color >> 8);
      Serial.write(color & 0xff);
      Serial.flush();
    }
  }
}

//
#if DISP_TYPE==SSD1306

#define OLEDADDR  (0x78 >> 1) // SSD1306 I2C address

#define SET_CONTRAST_CONTROL  0x81
#define SET_CHARGE_PUMP       0x8D
#define SET_ADDRESSING_MODE   0x20
#define SET_DISPLAY_STARTLINE 0x40
#define SET_SEGMENT_REMAP     0xA1
#define SET_ENTIRE_DISPLAY    0xA4
#define SET_DISPLAY_NORMAL    0xA6
#define SET_MULTIPLEX_RATIO   0xA8
#define SET_DISPLAY_ON        0xAF
#define SET_COM_OUTPUT_SCAN   0xC8
#define SET_DISPLAY_OFFSET    0xD3
#define SET_OSCILLATOR_FREQ   0xD5
#define SET_COM_PINS_HARDWARE 0xDA
#define SET_COLUMN_ADDRESS    0x21
#define SET_PAGE_ADDRESS      0x22

// byte data
void oled_command(unsigned char data)
{
  Wire1.beginTransmission(OLEDADDR);
  Wire1.write(0b10000000);
  Wire1.write(data);             
  Wire1.endTransmission();
}

// word data
void oled_command2(unsigned char data1,unsigned char data2)
{
  Wire1.beginTransmission(OLEDADDR);
  Wire1.write(0b00000000);
  Wire1.write(data1);             
  Wire1.write(data2);             
  Wire1.endTransmission();
}

// OLED INITIALIZE
void disp_init(void)
{
  Wire1.setSDA(2);
  Wire1.setSCL(3);
  Wire1.setClock(2000000);  
  Wire1.begin();

  delay(50);
  oled_command2(SET_MULTIPLEX_RATIO , 0x3F);
  oled_command2(SET_DISPLAY_OFFSET,0);
  oled_command(SET_DISPLAY_STARTLINE);
  oled_command(SET_COM_OUTPUT_SCAN);
  oled_command(SET_SEGMENT_REMAP);
  oled_command2(SET_COM_PINS_HARDWARE, 0x12);
  oled_command2(SET_CONTRAST_CONTROL , 0x80);
  oled_command(SET_ENTIRE_DISPLAY);
  oled_command(SET_DISPLAY_NORMAL);
  oled_command2(SET_OSCILLATOR_FREQ  , 0x80);
  oled_command2(SET_ADDRESSING_MODE  ,0); 
  oled_command2(SET_CHARGE_PUMP , 0x14);
  oled_command(SET_DISPLAY_ON);
  delay(10);
  vram_textzoom(FONTSIZE);
}

//   (OLED)SEND VRAM to DISPLAY 
void disp_update(void){
  int i,j,x,y;
  unsigned char *ptr,*ptr2;
  unsigned char work;

  Wire1.beginTransmission(OLEDADDR);
  Wire1.write(0b00000000);
  Wire1.write(SET_COLUMN_ADDRESS);
  Wire1.write(0);       // start column
  Wire1.write(VRAMWIDTH-1); // end column
  Wire1.write(SET_PAGE_ADDRESS);
  Wire1.write(0);           // start page
  Wire1.write((VRAMHEIGHT/8)-1); // end page
  Wire1.endTransmission();

  x=0;
  y=0;
  ptr = vram;
  while(y < VRAMHEIGHT){  
    Wire1.beginTransmission(OLEDADDR);
    Wire1.write(0b01000000);

    for(i=0; i<8; i++){
      ptr2 = ptr;
      work = 0;
      for(j=0; j<8; j++){  
        work >>= 1;
        if(*ptr2)work |= 0x80;
        ptr2 += VRAMWIDTH*2;
      }
      Wire1.write(work);
      x++;
      ptr += 2;
    }
    Wire1.endTransmission();
    if(x >= VRAMWIDTH){
      x=0;
      y+=8;
      ptr = vram + (y*VRAMWIDTH*2);
    }
  }
//capture();
}
#endif

// CALC. COLOR
unsigned int color16bit(int r,int g,int b)
{
// RRRRRGGGGGGBBBBB
// blue :bit4 -bit0 (0-31)
// green:bit10-bit5 (0-63)
// red  :bit15-bit11(0-31)
  r >>= 3;
  g >>= 2;
  b >>= 3;
  return(((unsigned int)r << 11)+(g << 5)+b);
}

// CLEAR VRAM
void vram_cls(void)
{
  long i;
  unsigned char *ptr;

  ptr = vram;
  i = VRAMSIZE;
  while(i--){
    *ptr++ = 0;
  }
}

// GET PIXEL
unsigned int vram_point(int x,int y)
{
  int i;
  unsigned int color;
  unsigned char *ptr;

#if DISP_ROTATE==1
  i=x;
  x=(VRAMWIDTH-1)-y;
  y=i;
#endif
#if DISP_ROTATE==2
  x=(VRAMWIDTH-1)-x;
  y=(VRAMHEIGHT-1)-y;
#endif
#if DISP_ROTATE==3
  i=x;
  x=y;
  y=(VRAMHEIGHT-1)-i;
#endif

  if(x<0)return(0);
  if(y<0)return(0);
  if(x>=VRAMWIDTH)return(0);
  if(y>=VRAMHEIGHT)return(0);

  ptr = vram;
  ptr += ((long)y*(VRAMWIDTH*2)) + (x*2);
  color =  *ptr << 8;
  ptr++;
  color += *ptr;
  return(color);
}

// DRAW PIXEL
void vram_pset(int x,int y,unsigned int color)
{
  int i;
  unsigned char *ptr;

#if DISP_ROTATE==1
  i=x;
  x=(VRAMWIDTH-1)-y;
  y=i;
#endif
#if DISP_ROTATE==2
  x=(VRAMWIDTH-1)-x;
  y=(VRAMHEIGHT-1)-y;
#endif
#if DISP_ROTATE==3
  i=x;
  x=y;
  y=(VRAMHEIGHT-1)-i;
#endif

  if(x<0)return;
  if(y<0)return;
  if(x>=VRAMWIDTH)return;
  if(y>=VRAMHEIGHT)return;
  ptr = vram;
  ptr += ((long)y*(VRAMWIDTH*2)) + (x*2);

  *ptr++ = color >> 8;   //high
  *ptr = color & 0xff;   //low 
}

// BOX FILL
void vram_fill(int x1 ,int y1 ,int x2 ,int y2 ,unsigned int color)
{
  int x,y;
  for(y=y1; y<=y2; y++){
    for(x=x1; x<=x2; x++){
      vram_pset(x, y ,color); //ドット描画
    }
  }
}

// DRAW LINE
void vram_line(int x1 ,int y1 ,int x2 ,int y2 ,unsigned int color)
{
  int xd;    // X2-X1座標の距離
  int yd;    // Y2-Y1座標の距離
  int xs=1;  // X方向の1pixel移動量
  int ys=1;  // Y方向の1pixel移動量
  int e;

  xd = x2 - x1;  // X2-X1座標の距離
  if(xd < 0){
    xd = -xd;  // X2-X1座標の絶対値
    xs = -1;    // X方向の1pixel移動量
  }
  yd = y2 - y1;  // Y2-Y1座標の距離
  if(yd < 0){
    yd = -yd;  // Y2-Y1座標の絶対値
    ys = -1;    // Y方向の1pixel移動量
  }
  vram_pset (x1, y1 ,color); //ドット描画
  e = 0;
  if( yd < xd ) {
    while( x1 != x2) {
      x1 += xs;
      e += (2 * yd);
      if(e >= xd) {
        y1 += ys;
        e -= (2 * xd);
      }
      vram_pset(x1, y1 ,color); //ドット描画
    }
  }else{
    while( y1 != y2) {
      y1 += ys;
      e += (2 * xd);
      if(e >= yd) {
        x1 += xs;
        e -= (2 * yd);
      }
      vram_pset(x1, y1 ,color); //ドット描画
    }
  }
}

// LOCATE
void vram_locate(int textx, int texty)
{
  putch_x = textx;
  putch_y = texty;
}

// TEXT COLOR
void vram_textcolor(unsigned int color)
{
  putch_color = color;
}

// TEXT ZOOM
void vram_textzoom(int zoom)
{
  putch_zoom = zoom;
}

// PRINT CHARACTER
void vram_putch(unsigned char ch)
{
  char i,j;
  unsigned char bitdata;
  int idx,x,y;

  if(ch =='\n')putch_x += VRAMXMAX;
  if(putch_x > (VRAMXRANGE-(8*putch_zoom))){
    putch_x = 0;
    putch_y += 8*putch_zoom;
    y = (VRAMYRANGE-(8*putch_zoom));
    if(putch_y > y){
      vram_scroll(0,putch_y - y);
      putch_y = y;
    }
  }  
  if(ch < 0x20)return;

  ch -= 0x20;
  idx = ((int)ch * 8);
  for(i=0 ;i<8 ;i++) {
    bitdata = font[idx];
    idx++;
    for(j=0; j<8; j++){
      if(bitdata & 0x80){
        x=putch_x+(j*putch_zoom);
        y=putch_y+(i*putch_zoom);
        vram_fill(x,y,x+putch_zoom-1,y+putch_zoom-1,putch_color);
      }
      bitdata <<= 1;
    }
  }
  putch_x += 8*putch_zoom;
}

// PRINT STRING
void vram_putstr(unsigned char *p)
{
  while(*p != 0){
    vram_putch( *p++ );
  }
}

// PRINT DECIMAL
void vram_putdec(unsigned int num)
{
  unsigned char ch;
  unsigned int shift=10000;
  
  while(shift > 0){
    ch = (num / shift) % 10;
    ch += '0';
    vram_putch(ch);
    shift /= 10;
  }
}

// PRINT DECIMAL
void vram_putdec2(unsigned int num)
{
  vram_putch((num / 10)+'0');
  vram_putch((num % 10)+'0');
}

// PRINT HEXADECIMAL
void vram_puthex(unsigned char num)
{
  unsigned char ch;
  char shift=4;

  while(1){
    ch = (num >> shift) & 0xf;
    if(ch < 10){
      ch += '0';
    }else{
      ch += ('A'-10);
    }
    vram_putch(ch);
    if(shift==0)break;
    shift -= 4;
  }
}

// SCROLL
void vram_scroll(int xd,int yd)
{
  int x,y;
  unsigned int color;

  for(y=0;y<VRAMYRANGE;y++){
    for(x=0;x<VRAMXRANGE;x++){
      color = vram_point(x+xd, y+yd);
      vram_pset(x,y,color);
    }
  }
}

// BCDを10進に変換
unsigned char bcd_to_num(unsigned char num){
  return((num >> 4)*10 + (num & 0xf));
}

// 10進をBCDに変換
unsigned char num_to_bcd(unsigned char num){
  unsigned char numhigh,numlow;
  numhigh = num / 10;
  numlow = num % 10;
  return((numhigh << 4) + numlow);
}
//---- タイマ割り込み
volatile char timeover;

int64_t alarm_callback(alarm_id_t id, void *user_data) {
    timeover = 1;
    return 0;
}

// マイクロ秒ウエイト（少し不正確）
void delay_us(int microsecond)
{
	timeover = 0;
  add_alarm_in_us(microsecond-3, alarm_callback, NULL, true);
  while (timeover==0) {
  }
}
//----
#define CYBER_BITA      (1<<3)
#define CYBER_BITB      (1<<2)
#define CYBER_BITC      (1<<1)
#define CYBER_BITD      (1<<0)
#define CYBER_BITE1     (1<<3)
#define CYBER_BITE2     (1<<2)
#define CYBER_BITSTART  (1<<1)
#define CYBER_BITSELECT (1<<0)

#define BTN_GPIOA  12
#define BTN_GPIOB  13
#define BTN_GPIOC  14
#define BTN_GPIOD  15
#define BTN_GPIOSTART  16
#define BTN_GPIOE2     17
#define BTN_GPIOE1     18
#define BTN_GPIOSELECT 19

#define DOUT_GPIO0  4   // 1:bit0
#define DOUT_GPIO1  5   // 2:bit1
#define DOUT_GPIO2  6   // 3:bit2
#define DOUT_GPIO3  7   // 4:bit3
#define X68K_GPIOLH   8  // 6:LH
#define X68K_GPIOACK  9  // 7:ACK
#define X68K_GPIOREQ  10 // 8:REQ
#define X68K_GPIOGND  11 // 9:GND
#define MD_GPIOLH     8   // 6:LH 
#define MD_GPIOREQ    9   // 7:REQ
#define MD_GPIOGND    10  // 8:GND
#define MD_GPIOACK    11  // 9:ACK 

// PC mode *動作未確認
void x68k_analog(void)
{
  unsigned char sendbuf[12];
  int datanum;
  unsigned char ch0,ch1,ch2,temp,speedmode,newspeed,lh;
  unsigned int timea,timeb,timec,timed;
  volatile uint32_t *portout_reg;
  uint32_t temp32;

  portout_reg = (volatile uint32_t *)0xd0000010;
  pinMode(X68K_GPIOGND,OUTPUT);
  digitalWrite(X68K_GPIOGND,LOW);  //;
  pinMode(DOUT_GPIO0,OUTPUT);
  pinMode(DOUT_GPIO1,OUTPUT);
  pinMode(DOUT_GPIO2,OUTPUT);
  pinMode(DOUT_GPIO3,OUTPUT);
  pinMode(X68K_GPIOLH,OUTPUT);
  pinMode(X68K_GPIOACK,OUTPUT);
  digitalWrite(X68K_GPIOACK,HIGH); //ACK=H;
  digitalWrite(X68K_GPIOLH,LOW);   //LH=L;
  pinMode(X68K_GPIOREQ,INPUT);
  #if DISP_TYPE==SSD1306
    vram_putstr((unsigned char *)"PC MODE");
    disp_update();  
  #endif
  speedmode = 0;  // fast
  lh=0;
  while(1){
    temp = 0x0f;
    if(digitalRead(BTN_GPIOA)==0) temp &= ~CYBER_BITA;
    if(digitalRead(BTN_GPIOB)==0) temp &= ~CYBER_BITB;
    if(digitalRead(BTN_GPIOC)==0) temp &= ~CYBER_BITC;
    if(digitalRead(BTN_GPIOD)==0) temp &= ~CYBER_BITD;
    sendbuf[0] = temp;
    sendbuf[10] = temp; //

    temp = 0x0f;
    if(digitalRead(BTN_GPIOE1)==0) temp &= ~CYBER_BITE1;
    if(digitalRead(BTN_GPIOE2)==0) temp &= ~CYBER_BITE2;
    if(digitalRead(BTN_GPIOSTART)==0) temp &= ~CYBER_BITSTART;
    if(digitalRead(BTN_GPIOSELECT)==0) temp &= ~CYBER_BITSELECT;
    sendbuf[1] = temp;
    sendbuf[11] = temp;

    ch0 = stick_get(0);  
    ch1 = stick_get(1);  
    ch2 = stick_get(2);  
    sendbuf[2] = ch0 >> 4;    // CH0 H
    sendbuf[3] = ch1 >> 4;    // CH1 H
    sendbuf[4] = ch2 >> 4;    // CH2 H
    sendbuf[5] = 0;           // CH3 H
    sendbuf[6] = ch0 & 0x0f;  // CH0 L
    sendbuf[7] = ch1 & 0x0f;  // CH1 L
    sendbuf[8] = ch2 & 0x0f;  // CH2 L
    sendbuf[9] = 0;           // CH3 L

    if(speedmode == 0){
      timea = 12;    
      timeb = 4;
      timec = 12;
      timed = 22;
    }else if(speedmode == 1){
      timea = 26;    
      timeb = 8;
      timec = 40;
      timed = 22;
    }else if(speedmode == 2){
      timea = 50;    
      timeb = 8;
      timec = 64;
      timed = 22;
    }else{
      timea = 74;    
      timeb = 8;
      timec = 88;
      timed = 22;
    }
    newspeed = 0xff;
    while(digitalRead(X68K_GPIOREQ)==LOW);
    while(digitalRead(X68K_GPIOREQ)==HIGH);
    delay_us(4);

    for(datanum=0 ;datanum<12; datanum++){
      temp32 = *portout_reg;
      temp32 &= ~((uint32_t)0xf << DOUT_GPIO0);
      temp32 |= ((uint32_t)sendbuf[datanum] << DOUT_GPIO0);
      *portout_reg = temp32;
//      digitalWrite(DOUT_GPIO0 ,((sendbuf[datanum] & (1<<0))!= 0));
//      digitalWrite(DOUT_GPIO1 ,((sendbuf[datanum] & (1<<1))!= 0));
//      digitalWrite(DOUT_GPIO2 ,((sendbuf[datanum] & (1<<2))!= 0));
//      digitalWrite(DOUT_GPIO3 ,((sendbuf[datanum] & (1<<3))!= 0));
  
      if((datanum & 1)==0){
        digitalWrite(X68K_GPIOACK,LOW);   //ACK_L
        delay_us(timea);
        digitalWrite(X68K_GPIOACK,HIGH);  //ACK_H;
        lh ^= 1;
        digitalWrite(X68K_GPIOLH,lh); //LH_INVERT;
        delay_us(timeb);
      }else{
        digitalWrite(X68K_GPIOACK,LOW);   //ACK_L
        delay_us(timec);
        if(newspeed == 0xff){ // speed setting
          if(digitalRead(X68K_GPIOREQ)){
            newspeed = datanum/2;
          }
        }
        digitalWrite(X68K_GPIOACK,HIGH);   //ACK_H;
        lh ^= 1;
        digitalWrite(X68K_GPIOLH,lh); //LH_INVERT;
        delay_us(timed);
      }
    }
    digitalWrite(X68K_GPIOLH,LOW);  //LH_L;
    if(newspeed > 3)newspeed = 3;
    speedmode = newspeed;
  }
}

//------ MD mode
void md_analog(void)
{
  unsigned char sendbuf[12];
  int datanum;
  unsigned char ch0,ch1,ch2,temp,lh;
  volatile uint32_t *portout_reg;
  uint32_t temp32;

  portout_reg = (volatile uint32_t *)0xd0000010;
  pinMode(MD_GPIOGND,OUTPUT);
  digitalWrite(MD_GPIOGND,LOW);  // LH=L
  pinMode(DOUT_GPIO0,OUTPUT);

  pinMode(DOUT_GPIO1,OUTPUT);
  pinMode(DOUT_GPIO2,OUTPUT);
  pinMode(DOUT_GPIO3,OUTPUT);
  pinMode(MD_GPIOLH,OUTPUT);
  pinMode(MD_GPIOACK,OUTPUT);
  digitalWrite(MD_GPIOACK,HIGH); // ACK=H;
  digitalWrite(MD_GPIOLH,LOW);   // LH=L;
  pinMode(MD_GPIOREQ,INPUT);
  #if DISP_TYPE==SSD1306
    vram_putstr((unsigned char *)"MD MODE");
    disp_update();  
  #endif    
  sendbuf[11] = 0xf;        //未調査
  lh=0;
  while(1){
    temp = 0x0f;
    if(digitalRead(BTN_GPIOE1)==0) temp &= ~CYBER_BITE1;
    if(digitalRead(BTN_GPIOE2)==0) temp &= ~CYBER_BITE2;
    if(digitalRead(BTN_GPIOSTART)==0) temp &= ~CYBER_BITSTART;
    if(digitalRead(BTN_GPIOSELECT)==0) temp &= ~CYBER_BITSELECT;
    sendbuf[0] = temp;
    sendbuf[10] = temp;

    temp = 0x0f;
    if(digitalRead(BTN_GPIOA)==0) temp &= ~CYBER_BITA;
    if(digitalRead(BTN_GPIOB)==0) temp &= ~CYBER_BITB;
    if(digitalRead(BTN_GPIOC)==0) temp &= ~CYBER_BITC;
    if(digitalRead(BTN_GPIOD)==0) temp &= ~CYBER_BITD;
    sendbuf[1] = temp;

    ch1 = stick_get(0);  // LEFT/RIGHT
    ch0 = stick_get(1);  // UP/DOWN
    ch2 = stick_get(2);  // Throttle
    sendbuf[2] = ch1 >> 4;    // CH1 H
    sendbuf[3] = ch0 >> 4;    // CH0 H
    sendbuf[4] = 0;           // CH3 H
    sendbuf[5] = ch2 >> 4;    // CH2 H
    sendbuf[6] = ch1 & 0x0f;  // CH1 L
    sendbuf[7] = ch0 & 0x0f;  // CH0 L
    sendbuf[8] = 0;           // CH3 L
    sendbuf[9] = ch2 & 0x0f;  // CH2 L

    while(digitalRead(MD_GPIOREQ)==LOW);
    while(digitalRead(MD_GPIOREQ)==HIGH);
    delay_us(4);

    for(datanum=0 ;datanum<12; datanum++){
      temp32 = *portout_reg;
      temp32 &= ~((uint32_t)0xf << DOUT_GPIO0);
      temp32 |= ((uint32_t)sendbuf[datanum] << DOUT_GPIO0);
      *portout_reg = temp32;
//      digitalWrite(DOUT_GPIO0 ,((sendbuf[datanum] & (1<<0))!= 0));
//      digitalWrite(DOUT_GPIO1 ,((sendbuf[datanum] & (1<<1))!= 0));
//      digitalWrite(DOUT_GPIO2 ,((sendbuf[datanum] & (1<<2))!= 0));
//      digitalWrite(DOUT_GPIO3 ,((sendbuf[datanum] & (1<<3))!= 0));
      digitalWrite(MD_GPIOACK,LOW); // ACK=L
      delay_us(12);
      digitalWrite(MD_GPIOACK,HIGH); // ACK=H;
      lh ^= 1;
      digitalWrite(MD_GPIOLH,lh); // LH_INVERT;
      if((datanum & 1)==0){
        delay_us(4);
      }else{
        delay_us(22);
      }
    }
    digitalWrite(MD_GPIOLH,LOW); //LH=L;
  }
}

//----
#define STICK_MAX  3    // ジョイスティックの最大数
#define BUTTON_MAX  8   // ボタン最大数
#define GPIOBUTTON 12
#define GPIOSTICK  26

int stick_center[STICK_MAX];
//----
void stick_init(void)
{
  int i;
  for(i=0;i<STICK_MAX;i++){
    stick_center[i] = analogRead(GPIOSTICK + i);
  }
  for(i=0; i<BUTTON_MAX; i++){
    pinMode(GPIOBUTTON + i, INPUT_PULLUP);
  }
}

//----
unsigned char stick_get(int num)
{
  #define ADRANGE 300
  int count;
  count = analogRead(GPIOSTICK + num) - stick_center[num];
  count = (int)((long)count * (-128) / ADRANGE)+128;
  if(count < 0) count=0;
  if(count > 255)count=255;
  return((unsigned char)count);
}

#if DISP_TYPE==SSD1306
//----- (debug)adc test
void stick_test(void)
{
  int i;
  unsigned char temp;
  unsigned int color;
  int lx,ly,rx,ry;

  unsigned char strstick[3][5]={
    "  X:",
    "  Y:",
    "THR:"
  };

  unsigned char strbtn[8][7]={
    "A",
    "B",
    "C",
    "D",
    "START",
    "E2",
    "E1",
    "SELECT"
  };

  color = color16bit(255,255,255);
  vram_textcolor(color);      
  while(1){
    vram_cls();
    if(1){
      for(i=0;i<3;i++){
        vram_locate(0,16*i);
        vram_putstr((unsigned char *)strstick[i]);
        temp = stick_get(i);
        vram_puthex(temp);
      }
    }else{
      lx = stick_get(0)/(256/64);
      ly = stick_get(1)/(256/64);
      rx = 0+64+32;
      ry = stick_get(2)/(256/64);
      vram_line(lx-8,ly,lx+8,ly,color);
      vram_line(lx,ly-8,lx,ly+8,color);
      vram_line(rx-8,ry,rx+8,ry,color);
      vram_line(rx,ry-8,rx,ry+8,color);

      vram_line(0,0,127,0,color);
      vram_line(0,63,127,63,color);
      vram_line(0,0,0,63,color);
      vram_line(64,0,64,63,color);
      vram_line(127,0,127,63,color);
    }
    for(i=0;i<BUTTON_MAX;i++){
      if(digitalRead(GPIOBUTTON + i)==0){
        vram_locate(8,16*3);
        vram_putstr((unsigned char *)strbtn[i]);
        break;
      }
    }
    disp_update();
    delay(16);
  }
}
#endif

//----
void setup(void)
{
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
//  Serial.begin(115200);
  disp_init();
  stick_init();
  digitalWrite(LED_BUILTIN, LOW);
}

//----
void loop()
{
  md_analog();    // MD mode
//  x68k_analog(); // PC mode
}
