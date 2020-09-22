#include <M5StickC.h>
#define LGFX_M5STICKC         // M5Stick C / CPlus
#include <LovyanGFX.hpp>
#include "image.h"

static LGFX lcd;                 // LGFXのインスタンスを作成。
static LGFX_Sprite sprite(&lcd); // スプライトを使う場合はLGFX_Spriteのインスタンスを作成。

// define
#define BGWIDTH 80
#define BGHEIGHT 95 // RAWSIZE * RAWMAX + 3
#define COLMAX 8
#define RAWMAX 13
#define MARGIN_COL7 4
#define RAWSIZE 7
#define HALFBBLSIZE (BBLSIZE/2)

#define STARTX 35
#define STARTY 84 // RAWSIZE * (RAWMAX -1)
#define BBLSPEED 2

#define MODE_INIT 0
#define MODE_CTRL 1
#define MODE_FIRE 2
#define MODE_BLNK 3  //No Used
#define MODE_FALLING 4

// typedef
typedef struct{
	char kind;
	double posX;
	double posY;
	double dX;
	double dY;
}T_BUBBLE;

typedef struct{
  short mode;      /* 0:init 1:ctrl 2:fire 3:blink 4:falling */
  uint tick;
  uint score;
  char mat[COLMAX * RAWMAX];
  short firstcol8;  /* first col = 0->8 1->7 */
  short nxtBBL;
  short cntBBL[BBLKIND];
} T_GAME;

typedef struct{
  short n;
  int tick;
  char mat[COLMAX * RAWMAX];
}T_FALLING;

typedef struct{
  double arg;    // -18 <-> +18
}T_LAUNCHER;

PROGMEM double tbl_pitch[] = {
  0, 0.09, 0.18, 0.27, 0.36, 0.47 ,0.58, 0.70, 0.84,
  1.00, 1.19, 1.43, 1.73, 2.14, 2.74, 3.72, 5.65, 11.33,
};

PROGMEM double tbl_sin[] = {
  0, 0.09, 0.17, 0.26, 0.34, 0.42, 0.50, 0.57, 0.64,
  0.71, 0.77, 0.82, 0.87, 0.91, 0.94, 0.97, 0.98, 1.00,
};

PROGMEM double tbl_cos[] = {
  1.00, 1.00, 0.98, 0.97, 0.94, 0.91, 0.87, 0.82, 0.77,
  0.71, 0.64, 0.57, 0.50, 0.42, 0.34, 0.26, 0.17, 0.09,
};

// variable
static T_GAME gl_game;
static T_BUBBLE gl_currentBBL;
static T_FALLING gl_falling;
static T_BUBBLE gl_fallingBBL[COLMAX * RAWMAX];
static T_LAUNCHER gl_launcher;

void swap(int16_t & a, int16_t & b)
{
  int temp = a;
  a = b;
  b = temp;
}

void charset(char *p, char val, short n)
{
  for(short i = 0; i < n; i++){*p++ = val;}
}

short get_posx(short col, short raw)
{
  short posx = col * BBLSIZE;
  if( (raw & 1) !=  gl_game.firstcol8){
    posx -= MARGIN_COL7;
  }
  return posx;
}

void drawBubble(short x, short y, short kind)
{
  if(kind < 1){return;}
  kind--;
  sprite.pushImage(x, y, BBLSIZE, BBLSIZE, (uint16_t *)(&img_bubble[kind*BBLSIZE*BBLSIZE*2]), 0);
}

void drawFixedBubble()
{
  short col, raw;
  short x, y;
  for(raw = 0; raw < RAWMAX; raw++){
  for(col = 0; col < COLMAX; col++){
  y = raw * 7;
  x  = get_posx(col, raw);
  drawBubble(x, y, gl_game.mat[col + raw * COLMAX]);
  }}
}

void setNextBubble()
{
  short i, n = 0, c[BBLKIND];
  //gl_game.currentBBL.kind = gl_game.nxtBBL;
  //gl_game.currentBBL.posX = STARTX;
  //gl_game.currentBBL.posY = STARTY;
  for(i = 0; i < BBLKIND; i++)c[i] = 0;
  for(i = 0; i < BBLKIND; i++){
    if(gl_game.cntBBL[i]){c[n++] = i;} 
  }
  gl_game.nxtBBL = c[random(0, n)] + 1;
  drawBubble(STARTX, STARTY, gl_game.nxtBBL);
}

void drawScore(void)
{
  lcd.setCursor(20,10);
  lcd.printf("%06d", gl_game.score);
}

void initGame(void)
{
  int xx, yy, b;

  gl_game.firstcol8 = 0;

  charset(gl_game.mat, 0, COLMAX * RAWMAX);
  for(yy =  0; yy < 3; yy++){
  for(xx = (yy & 1); xx < COLMAX; xx++){
      b = random(0, BBLKIND);
      gl_game.cntBBL[b]++;
      gl_game.mat[xx + yy * COLMAX] = b + 1;
  }}
  drawFixedBubble();
  setNextBubble();
  gl_launcher.arg = 0;
  gl_game.tick = 0;
  gl_game.score = 0;
  drawScore();
  gl_game.mode = MODE_INIT;
}

int chkCollision(short idx, short x, short y){
    if(!gl_game.mat[idx]){return 0;}

    short y2 = idx/COLMAX;
    short x2 = get_posx(idx%COLMAX, y2) + HALFBBLSIZE;
    y2 = y2 * RAWSIZE + HALFBBLSIZE;
    if(((x-x2)*(x-x2)+(y-y2)*(y-y2)) < 90){return 1;}
    return 0;
}

int chkFixed(short x, short y)
{
    int posx;
    int posy = y/RAWSIZE;
    int idx;

    if(posy <= 0){return 1;}
    
    if((posy & 1) != gl_game.firstcol8){
        posx = (x + HALFBBLSIZE)/BBLSIZE;
        if(posx < 1){posx = 1;}
        if(posx > (COLMAX-1)){posx=(COLMAX-1);}
        idx = posx + (posy-1)*COLMAX;
        if(chkCollision(idx, x, y)){return 1;}
        if(posx < (COLMAX-1)){
            idx = (posx + 1) + posy  *COLMAX;
            if(chkCollision(idx, x, y)){return 1;}
        }
        idx = (posx - 1) + (posy-1)*COLMAX;
        if(chkCollision(idx, x, y)){return 1;}
        if(posx > 1){
            idx = (posx - 1) + posy*COLMAX;
            if(chkCollision(idx,x,y)){return 1;}
        }
    }
    else{
        posx = x/BBLSIZE;
        idx = posx + (posy-1)*COLMAX;
        if(chkCollision(idx, x, y)){return 1;}
        if(posx > 0){
            idx = (posx - 1) + posy*COLMAX;
            if(chkCollision(idx, x, y)){return 1;}    
        }
        if(posx < (COLMAX-1)){
            idx = (posx + 1) + posy*COLMAX;
            if(chkCollision(idx,x,y)){return 1;}
            idx = (posx + 1) + (posy-1)*COLMAX;
            if(chkCollision(idx,x,y)){return 1;}
        }  
    }
    return 0;
}

int chkFallingSub(short x, short y, char *p){
    if(p[x+y*COLMAX] != 1){return 0;}
    p[x+y*COLMAX] = 2;
    
    if(y > 0){chkFallingSub(x, y-1, p);}
    if(x > 0){
        chkFallingSub(x-1, y, p);
        if( (y & 1) != gl_game.firstcol8 ){
            if(y > 0){chkFallingSub(x-1, y-1, p);}    
            if(y < 12){chkFallingSub(x-1, y+1, p);}    
        }
    }
    if(x < (COLMAX-1)){
        chkFallingSub(x+1, y, p);
        if((y & 1) == gl_game.firstcol8){
            if(y > 0){chkFallingSub(x+1, y-1, p);}    
            if(y < 12){chkFallingSub(x+1, y+1, p);}   
        }
    }
    if(y < 12){chkFallingSub(x, y+1, p);}
    return 0;
}

int chkFalling(short idx){
    short x = idx%COLMAX;
    short y = idx/COLMAX;
    short i; short n;

    //Check Jump
    for (i = 0; i < COLMAX * RAWMAX; i++){
      gl_falling.mat[i] = ((gl_game.mat[i] && gl_game.mat[i]==gl_game.mat[idx]) ? 1:0);
    }
    chkFallingSub(x, y, gl_falling.mat);
    n = 0;for (i = 0; i < COLMAX * RAWMAX; i++){if(gl_falling.mat[i] > 1){n++;}}
    if(n < 3){return 0;}

    n = 0;
    for (i = 0; i < COLMAX * RAWMAX; i++){
        if(gl_falling.mat[i] > 1){
            x = i%COLMAX; y = i/COLMAX;
            gl_fallingBBL[n].kind = gl_game.mat[i];
            gl_fallingBBL[n].posY = y * RAWSIZE;
            gl_fallingBBL[n].posX = get_posx(x, y);
            gl_fallingBBL[n].dX = -2 + random(4);
            gl_fallingBBL[n].dY = -2 - random(4);
            gl_game.mat[i] = 0;
            n++;
        }
    }

    //Check Fall
    for (i = 0; i < COLMAX * RAWMAX; i++){
      gl_falling.mat[i] = (gl_game.mat[i] ? 1:0);
    }
    for (i = 0; i < COLMAX; i++){chkFallingSub(i, 0, gl_falling.mat);}
    for (i = 0; i < COLMAX*RAWMAX; i++){
        if(gl_falling.mat[i] == 1){
            x = i%COLMAX; y = i/COLMAX;
            gl_fallingBBL[n].kind = gl_game.mat[i];
            gl_fallingBBL[n].posY = y * RAWSIZE;
            gl_fallingBBL[n].posX = get_posx(x, y);
            gl_fallingBBL[n].dX = 0;
            gl_fallingBBL[n].dY = .75 + random(3);
            gl_game.mat[i] = 0;
            n++;
        }
    }
    gl_game.score += (10 + (n-3)*10);
    drawScore();
    gl_falling.n = n;
    gl_falling.tick = 0;
    return 1;
}

int moveBubble(void){
  double nx = gl_currentBBL.posX;
	double ny = gl_currentBBL.posY;
  short x, y;

	nx += gl_currentBBL.dX;
	ny += gl_currentBBL.dY;

  if(nx < 0){nx = 0; gl_currentBBL.dX = -(gl_currentBBL.dX);}
	else if( nx >= (COLMAX - 1)*BBLSIZE ){nx = (COLMAX - 1)*BBLSIZE; gl_currentBBL.dX = -(gl_currentBBL.dX);}
	
	if(ny < 0){ny = 0;}
	if(chkFixed(nx + HALFBBLSIZE, ny + HALFBBLSIZE) ){
        y = (short)((ny + HALFBBLSIZE)/RAWSIZE);
        if((y & 1) != gl_game.firstcol8){
          x = (short)((nx + BBLSIZE)/BBLSIZE);
          if(x < 1){x = 1;}
        }
        else{x = (short)((nx + HALFBBLSIZE)/BBLSIZE);}
        if(x > 7){x=7;}
        gl_game.mat[x + y*COLMAX] = gl_currentBBL.kind;
        gl_currentBBL.kind = 0;
        if(chkFalling(x + y*COLMAX)){
            return 2;
        }
		return 1;		//Fixed.
	}
  //  pushImage((int)nx, (int)ny,
	//	BBLSIZE, BBLSIZE, (uint16_t *)imgBubble[m_mvBubble.bubble]);	
	gl_currentBBL.posX = nx;
	gl_currentBBL.posY = ny;
	return 0;
}

int fallingBubble(void){
    int i; int  n;
    T_BUBBLE *p;

    for (i = 0; i < gl_falling.n; i++){
        p =  &gl_fallingBBL[i];
        p->posY += p->dY;
        p->posX += p->dX;
        if (p->posY <= 0){p->posY = 0; p->dY = -(p->dY);}
        if (p->posX <= 0){p->posX = 0; p->dX = -(p->dX);}
        if (p->posX >= BBLSIZE*(COLMAX-1)){p->posX = BBLSIZE*(COLMAX-1); p->dX = -(p->dX);}
        if (p->posY > BGHEIGHT){
            if ( i < gl_falling.n-1 ){
                memcpy(p, &gl_fallingBBL[gl_falling.n-1], sizeof(T_BUBBLE));
                i--;
            }
            if (--gl_falling.n < 1){
                gl_falling.n = 0;
                /* Check rest bubbles */
                for (i = 0; i < BBLKIND; i++){gl_game.cntBBL[i] = 0;}
                n = 0; for (i = 0; i < COLMAX * RAWMAX; i++){
                    if (gl_game.mat[i]){n++; gl_game.cntBBL[gl_game.mat[i]-1]++;}
                } if(n){return 1;}
                /* Go to Clear! */
                //pushImageBG(240);update();
                //m_winnerPanel = 5;
                return 2;
            }
            continue;
        }
        p->dY += 1;
        //sprite.pushImage(p->posX, p->posY,
		    //BBLSIZE, BBLSIZE, (uint16_t *)img_bubble[p->kind]);
        drawBubble(p->posX, p->posY, p->kind);
    }
    return 0;
}

float accX = 0.0F;
float accY = 0.0F;
float accZ = 0.0F;


double getIMU(void)
{
  M5.IMU.getAccelData(&accX,&accY,&accZ);
  if( accX > 0.35 ){
    //if(accX > 0.8){return -4;}
    //if(accX > 0.5){return -2;}
    return -1;
  }
  if( accX < -0.35 ){
    //if(accX < -0.8){return 4;}
    //if(accX < -0.5){return 2;}
    return 1;
  }
  return 0;  
}

int chkExistBBL(short x, short y)
{
  short raw = y/RAWSIZE;
  short col;
  if(y > RAWSIZE * RAWMAX){return 0;}
  if((raw & 1) != gl_game.firstcol8){
    if(x < (BBLSIZE - MARGIN_COL7)){return 0;}
    x += MARGIN_COL7;
  }
  col = x/BBLSIZE;
  if(gl_game.mat[col + raw * COLMAX]){return 1;}
  return 0;
}

void ctrlLauncher(void)
{
  double x, y, dx;
  short px, py;
  double arg = gl_launcher.arg;
  arg += (getIMU()/4);
  if(arg < -17){arg = -17;}
  if(arg > 17){arg = 17;}
  gl_launcher.arg = arg;
    
  double pitch = tbl_pitch[(int)abs(gl_launcher.arg)];
  if(pitch < 1){
    y = 0;
    while( y < STARTY ){
      dx = y * pitch;
      if(dx > 39){dx = 40;}
      if(gl_launcher.arg < 0){dx = -dx;}
      px = (short)(STARTX + HALFBBLSIZE + dx);
      py = (short)(STARTY - y);
      if(chkExistBBL(px, py)){break;}
      sprite.drawPixel(px, py, WHITE);
      if(abs(dx) > 39){break;}
      y++;
    }
  }
  else{
    x = 0;
    while( x < 40 ){
      dx = x;
      y = dx / pitch;
      if(y >= STARTY){y = STARTY;}
      if(gl_launcher.arg < 0){dx = -x;}
      px = (short)(STARTX + HALFBBLSIZE + dx);
      py = (short)(STARTY - y);
      if(chkExistBBL(px, py)){break;}
      sprite.drawPixel(px, py, WHITE);
      if(y >= STARTY){break;}
      x++;
    }
  }  
}

void getInput(void)
{
  if(gl_game.mode != MODE_CTRL){return;}  
  ctrlLauncher();
  // fire !
  if(digitalRead(M5_BUTTON_HOME) == LOW){
    gl_currentBBL.kind = gl_game.nxtBBL;
    gl_currentBBL.posX = STARTX;
    gl_currentBBL.posY = STARTY;
    gl_currentBBL.dX = tbl_sin[(int)abs(gl_launcher.arg)] * BBLSPEED;
    if(gl_launcher.arg < 0){gl_currentBBL.dX *= -1;}
    gl_currentBBL.dY = -tbl_cos[(int)abs(gl_launcher.arg)] * BBLSPEED;
    gl_game.mode = MODE_FIRE;
    setNextBubble();
  }
}

void downBBL()
{
  short col, raw;
  char b;
  for(raw = 0; raw < (RAWMAX - 1); raw++){
  for(col = 0; col < COLMAX; col++){
    gl_game.mat[col + (RAWMAX - 1 - raw) * COLMAX] =  gl_game.mat[col + (RAWMAX - 2 - raw) * COLMAX];
  }}
  gl_game.firstcol8 ^= 1;
  for(col = 0; col < COLMAX; col++){
    if(col == 0 && gl_game.firstcol8){
      gl_game.mat[0] = 0;
    }else{
      b = random(0, BBLKIND);
      gl_game.cntBBL[b]++;
      gl_game.mat[col] = b + 1;
    }
  } 
}

int chkGameLose()
{
  for(short col = 0; col < COLMAX; col++){
    if(gl_game.mat[col + (RAWMAX-1)*COLMAX]){return 1;}  
  }
  return 0;
}

void gamelose()
{
  for(short i = 0; i < 4; i++){
    lcd.fillRect(0, 60 - i*5, 90, (i+1)*10, 0);
    delay(500);
  }
  lcd.setCursor(5, 52);
  lcd.print(" GAME OVER ");
  delay(1000);
  for(;;){
    if(digitalRead(M5_BUTTON_HOME) == LOW){break;}
    delay(100);
  }
  initGame();
}

void gameloop()
{
  //int ret;
  getInput();
  drawFixedBubble();
  drawBubble(STARTX, STARTY, gl_game.nxtBBL);
  switch(gl_game.mode){
  case MODE_INIT:
    sprite.pushSprite(0, 30);
    lcd.setCursor(20, 80);
    lcd.print("G O !");
    delay(1000);
    gl_game.mode = MODE_CTRL;
    break;

  case MODE_FIRE:
    switch(moveBubble()){
    case 0: drawBubble(gl_currentBBL.posX, gl_currentBBL.posY, gl_currentBBL.kind); break;
    case 1: gl_game.mode = MODE_CTRL; break;
    case 2: gl_game.mode = MODE_FALLING; break;
    }
    break;

  case MODE_FALLING:
    switch(fallingBubble()){
    case 1: gl_game.mode = MODE_CTRL; break;
    }
    break;  
  }
  if(++gl_game.tick > 1000){
    downBBL();
    gl_game.tick = 0;  
  }
}

void setup(void)
{
  M5.begin();
  M5.IMU.Init();
  lcd.init();
  lcd.setRotation(0);
  lcd.setBrightness(128);
  lcd.clear(DARKGREY);


// createSprite
  sprite.createSprite(BGWIDTH, BGHEIGHT); // 80 x 91
  sprite.setSwapBytes(true);

  sprite.fillRect(0, 0, BGWIDTH, BGHEIGHT, 0);
  //sprite.pushImage(0, 0, 80, 10, (uint16_t *)img_data, 0);
  initGame();
  sprite.pushSprite(0, 30);
}

void loop(void)
{
  sprite.fillRect(0, 0, BGWIDTH, BGHEIGHT, 0);
  gameloop();
  sprite.pushSprite(0, 30);
  if(chkGameLose()){gamelose();}
  delay(30);
  M5.update();
}
