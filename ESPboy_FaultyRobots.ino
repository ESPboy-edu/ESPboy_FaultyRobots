//v1.0 17.10.2021 initial version
//by Shiru
//shiru@mail.ru
//https://www.patreon.com/shiru8bit

#include "ESPboyInit.h"


#include "glcdfont.c"

#include "gfx/title.h"

#include "mus/music_1.h"
#include "mus/music_2.h"
#include "mus/music_3.h"
#include "mus/music_4.h"
#include "mus/music_5.h"
#include "mus/music_6.h"
#include "mus/music_7.h"
#include "mus/music_8.h"

volatile unsigned int sound_out;
volatile unsigned int sound_shift;
volatile unsigned int sound_shift_copy;
volatile unsigned int sound_acc;
volatile unsigned int sound_add;
volatile unsigned int sound_frame_cnt;
volatile int sound_note;

ESPboyInit myESPboy;

#define SAMPLE_RATE     96000
#define FRAME_RATE      60

#define PLAYLIST_HEIGHT 8
#define PLAYLIST_LEN    8

const uint8_t* const playlist[PLAYLIST_LEN * 2] = {
  (const uint8_t*)"FAULTY ROBOTS"    , music_1,
  (const uint8_t*)"RUSTY GEARS"      , music_2,
  (const uint8_t*)"CONVEYOR BELT"    , music_3,
  (const uint8_t*)"OLD MODEL"        , music_4,
  (const uint8_t*)"CROSSWIRED"       , music_5,
  (const uint8_t*)"SCRAPLORD"        , music_6,
  (const uint8_t*)"BOSSTOWN DYNAMICS", music_7,
  (const uint8_t*)"NOT OBSOLETE"     , music_8,
};

int playlist_cur;

#define SPEC_BANDS        21
#define SPEC_HEIGHT       32
#define SPEC_RANGE        650
#define SPEC_SX           1
#define SPEC_SY           64
#define SPEC_BAND_WIDTH   6
#define SPEC_DECAY        1

volatile int spec_levels[SPEC_BANDS];



struct chStruct {
  const uint8_t* data;
  int skip;
  int wave;
  int base_note;
  int note;
  int detune;
};

struct envStruct {
  const uint8_t* data;
  const uint8_t* loop;
  int mode;
  int out;
};

#define MUSIC_CH_ALL  3
#define MUSIC_ENV_ALL (MUSIC_CH_ALL*2)


struct musStruct {
  bool active;
  const uint8_t* data;
  int tempo;
  int tempo_acc;
  int pattern_len;
  int row_counter;
  int instrument_off;
  int pattern_off;
  int order_ptr;
  int order_loop;

  struct chStruct ch[MUSIC_CH_ALL];
  struct envStruct env[MUSIC_ENV_ALL];
};

struct musStruct music;

unsigned int note_table[5 * 12];



void set_sound(unsigned int add, unsigned int wave)
{
  if (sound_shift_copy != wave)
  {
    sound_shift = wave;
    sound_shift_copy = wave;
  }

  sound_add = add;
}



void music_start(const uint8_t* data)
{
  music.active = false;

  memset(&music.ch, 0, sizeof(music.ch));
  memset(&music.env, 0, sizeof(music.env));

  music.data = data;

  music.tempo = pgm_read_byte(&data[0]);
  music.pattern_len = pgm_read_byte(&data[1]);
  music.instrument_off = pgm_read_byte(&data[2]) + pgm_read_byte(&data[3]) * 256;
  music.pattern_off = pgm_read_byte(&data[4]) + pgm_read_byte(&data[5]) * 256;
  music.order_ptr = pgm_read_byte(&data[6]) + pgm_read_byte(&data[7]) * 256;
  music.order_loop = pgm_read_byte(&data[8]) + pgm_read_byte(&data[9]) * 256;

  music.tempo_acc = music.tempo;  //force first row update
  music.row_counter = 0;

  music.active = true;
}



void music_stop()
{
  music.active = false;

  set_sound(0, 0);
}



void music_frame()
{
  if (!music.active) return;

  if (music.tempo_acc >= music.tempo)
  {
    music.tempo_acc -= music.tempo;

    if (music.row_counter == 0)  //order list update
    {
      while (1)
      {
        bool reread = false;

        for (int i = 0; i < MUSIC_CH_ALL; ++i)
        {
          int n = pgm_read_byte(&music.data[music.order_ptr + i]);

          if (n == 255) //take loop
          {
            //music.order_ptr = music.order_loop;
            //reread = true;
            //break;
            music_stop(); //just stop playing, as the album songs not looped
            return;
          }

          int ptn_off = pgm_read_byte(&music.data[music.pattern_off + n * 2 + 0]) + pgm_read_byte(&music.data[music.pattern_off + n * 2 + 1]) * 256;

          music.ch[i].data = &music.data[music.pattern_off + ptn_off];
          music.ch[i].skip = 0;
        }

        if (!reread) break;
      }

      music.order_ptr += 3;
      music.row_counter = music.pattern_len;
    }

    for (int i = 0; i < MUSIC_CH_ALL; ++i)
    {
      if (music.ch[i].skip)
      {
        --music.ch[i].skip;

        if (music.ch[i].skip) continue;
      }

      if (!music.ch[i].data) continue;

      int n = pgm_read_byte(music.ch[i].data);

      ++music.ch[i].data;

      if (n <= 30)
      {
        music.ch[i].skip = n;
        continue;
      }

      music.ch[i].base_note = n; //complete with instrument bits
      music.ch[i].detune = 0;

      int ins = ((n >> 5) & 7) + i * 7;

      int ins_off = music.instrument_off + ins * 4;

      for (int j = 0; j < 2; ++j)
      {
        music.env[i * 2 + j].data = &music.data[music.instrument_off + pgm_read_byte(&music.data[ins_off + j * 2 + 0]) + pgm_read_byte(&music.data[ins_off + j * 2 + 1]) * 256];
        music.env[i * 2 + j].loop = music.env[i * 2 + j].data;
        music.env[i * 2 + j].mode = 0;
      }
    }

    --music.row_counter;
  }

  music.tempo_acc += 14;  //PAL speed, 16 for NTSC

  for (int i = 0; i < MUSIC_ENV_ALL; ++i)
  {
    while (1)
    {
      if (!music.env[i].data) break;

      int n = pgm_read_byte(music.env[i].data);

      ++music.env[i].data;

      switch (n)
      {
        case 128: music.env[i].data = music.env[i].loop; continue;
        case 129: music.env[i].loop = music.env[i].data; continue;
        case 130: music.env[i].mode ^= 1; continue;
      }

      if (n > 128) n = n - 256;

      music.env[i].out = n;

      break;
    }
  }


  for (int i = 0; i < MUSIC_CH_ALL; ++i)
  {
    if (music.ch[i].base_note < 31) continue;

    if (music.ch[i].base_note == 31)
    {
      music.ch[i].wave = 0;
      continue;
    }

    music.ch[i].wave = music.env[i * 2 + 0].out;

    music.ch[i].note = music.ch[i].base_note & 0x1f;

    if (music.env[i * 2 + 1].mode == 0)
    {
      music.ch[i].note += music.env[i * 2 + 1].out;
    }
    else
    {
      music.ch[i].detune = music.env[i * 2 + 1].out * 8;
    }

    if (i == 1) music.ch[i].note += 12;
  }

  //PET-like single channel output

  for (int i = MUSIC_CH_ALL - 1; i >= 0; --i)
  {
    if (music.ch[i].wave || i == 0)
    {
      set_sound(note_table[music.ch[i].note] + music.ch[i].detune, music.ch[i].wave);
      sound_note = music.ch[i].note * 2 / 3;
      break;
    }
  }
}



uint8_t checkKey() {
  static uint8_t keysGet, prevKeys;
  keysGet = myESPboy.getKeys();
  if (prevKeys == keysGet) return (0);
  else {
    prevKeys = keysGet;
    return (keysGet);
  }
};


//0 no timeout, otherwise timeout in ms

void wait_any_key(int timeout) {
  timeout /= 100;
  while (1) {
    if (myESPboy.getKeys()) break;
    if (timeout) {
      --timeout;
      if (timeout <= 0) break;
    }
    delay(100);
  }
}


void spec_add()
{
  const int curve[5] = {SPEC_HEIGHT / 16, SPEC_HEIGHT / 8, SPEC_HEIGHT / 2, SPEC_HEIGHT / 8, SPEC_HEIGHT / 16};

  if (sound_shift_copy)
  {
    int off = sound_note;

    if (off < 0) off = 0;
    if (off > SPEC_BANDS - 1) off = SPEC_BANDS - 1;

    for (int i = 0; i < 5; ++i)
    {
      if (off >= 0 && off < SPEC_BANDS)
      {
        spec_levels[off] += curve[i];

        if (spec_levels[off] > SPEC_HEIGHT) spec_levels[off] = SPEC_HEIGHT;
      }

      ++off;
    }
  }
}

void spec_update()
{
  for (int i = 0; i < SPEC_BANDS; ++i)
  {
    spec_levels[i] -= SPEC_DECAY;
    if (spec_levels[i] < 0) spec_levels[i] = 0;
  }
}



void IRAM_ATTR sound_ISR()
{
  if (sound_out)
  {
    GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, _BV(SOUNDPIN));   //clear
  }
  else
  {
    GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, _BV(SOUNDPIN));   //set
  }

  ++sound_frame_cnt;

  if (sound_frame_cnt >= SAMPLE_RATE / FRAME_RATE)
  {
    sound_frame_cnt = 0;

    music_frame();

    spec_add();
    spec_update();
  }

  unsigned int n = sound_acc;

  sound_acc += sound_add;

  if (sound_acc < n)
  {
    sound_shift = (sound_shift << 1) | ((sound_shift & 0x80) ? 0x01 : 0x00);
    sound_out = sound_shift & 0x80;
  }
}



//render part of a 8-bit uncompressed BMP file
//no clipping
//uses line buffer to draw it much faster than through writePixel

void drawBMP8Part(int16_t x, int16_t y, const uint8_t bitmap[], int16_t dx, int16_t dy, int16_t w, int16_t h)
{
  static uint16_t buf[128];

  int32_t bw = pgm_read_dword(&bitmap[0x12]);
  int32_t bh = pgm_read_dword(&bitmap[0x16]);
  int32_t wa = (bw + 3) & ~3;

  if (w >= h)
  {
    for (int32_t i = 0; i < h; ++i)
    {
      int32_t off = 54 + 256 * 4 + (bh - 1 - (i + dy)) * wa + dx;

      for (int32_t j = 0; j < w; ++j)
      {
        int32_t col = pgm_read_byte(&bitmap[off++]);
        int32_t rgb = pgm_read_dword(&bitmap[54 + col * 4]);
        buf[j] = (((rgb & 0xf8) >> 3) | ((rgb & 0xfc00) >> 5) | ((rgb & 0xf80000) >> 8));
      }

      myESPboy.tft.pushImage(x, y + i, w, 1, buf);
    }
  }
  else
  {
    for (int32_t i = 0; i < w; ++i)
    {
      int32_t off = 54 + 256 * 4 + (bh - 1 - dy) * wa + i + dx;

      for (int32_t j = 0; j < h; ++j)
      {
        int32_t col = pgm_read_byte(&bitmap[off]);
        int32_t rgb = pgm_read_dword(&bitmap[54 + col * 4]);
        buf[j] = (((rgb & 0xf8) >> 3) | ((rgb & 0xfc00) >> 5) | ((rgb & 0xf80000) >> 8));
        off -= wa;
      }

      myESPboy.tft.pushImage(x + i, y, 1, h, buf);
    }
  }
}



void drawCharFast(int x, int y, int c, int16_t color, int16_t bg)
{
  static uint16_t buf[6 * 8];

  for (int i = 0; i < 6; ++i)
  {
    int line = i < 5 ? pgm_read_byte(&font[c * 5 + i]) : 0;

    for (int j = 0; j < 8; ++j)
    {
      buf[j * 6 + i] = ((line & 1) ? color : bg);
      line >>= 1;
    }
  }

  myESPboy.tft.pushImage(x, y, 6, 8, buf);
}



void printFast(int x, int y, const char* str, int16_t ink, int16_t paper)
{
  while (1)
  {
    char c = *str++;

    if (!c) break;

    drawCharFast(x, y, c, ink, paper);
    x += 6;
  }
}



bool title_screen_effect(int out)
{
  int16_t order[32 * 32];

  int wh = 8;

  for (int i = 0; i < 32 * 32; ++i) order[i] = i;

  for (int i = 0; i < 32; i += wh)
  {
    for (int j = 0; j < 32 * wh; ++j)
    {
      int off1 = (i * 32) + (rand() & (32 * wh - 1));
      int off2 = (i * 32) + (rand() & (32 * wh - 1));

      if (off1 >= 32 * 32) continue;
      if (off2 >= 32 * 32) continue;

      int temp = order[off1];
      order[off1] = order[off2];
      order[off2] = temp;
    }
  }

  int i = 0;

  while (i < 32 * 32)
  {
    if (checkKey()) return false;

    set_sound(note_table[rand() % (3 * 12)], rand() & 15);

    for (int j = 0; j < (!out ? 16 : 32); ++j)
    {
      int pos = order[i++];
      int x = pos % 32 * 4;
      int y = pos / 32 * 4;

      if (!out)
      {
        drawBMP8Part(x, y, g_title, x, y, 4, 4);
      }
      else
      {
        myESPboy.tft.fillRect(x, y, 4, 4, TFT_BLACK);
      }
    }

    delay(1000 / 60);
  }

  set_sound(0, 0);

  return true;
}



void playlist_display(bool cur)
{
  int pos = playlist_cur - PLAYLIST_HEIGHT / 2;

  if (pos < 0) pos = 0;
  if (pos > PLAYLIST_LEN - PLAYLIST_HEIGHT) pos = PLAYLIST_LEN - PLAYLIST_HEIGHT;

  printFast(0, 0, "    FAULTY ROBOTS    ", TFT_BLACK, TFT_GREEN);
  printFast(16, 104, " COMMODORE PET", TFT_GREEN, TFT_BLACK);
  printFast(16, 112, "CB2 MUSIC ALBUM", TFT_GREEN, TFT_BLACK);
  printFast(16, 120, " BY SHIRU'2021 ", TFT_GREEN, TFT_BLACK);

  int sy = 24;

  for (int i = 0; i < PLAYLIST_HEIGHT; ++i)
  {
    drawCharFast(4, sy, ' ', TFT_GREEN, TFT_BLACK);

    printFast(16, sy, (char*)playlist[pos * 2], TFT_GREEN, TFT_BLACK);

    if ((pos == playlist_cur) && cur) drawCharFast(4, sy, 0xdb, TFT_GREEN, TFT_BLACK);

    ++pos;

    sy += 8;
  }
}



void playlist_move(int dx)
{
  playlist_cur += dx;

  if (playlist_cur < 0) playlist_cur = PLAYLIST_LEN - 1;
  if (playlist_cur >= PLAYLIST_LEN) playlist_cur = 0;
}



void playlist_screen()
{
  set_sound(0, 0);

  myESPboy.tft.fillScreen(TFT_BLACK);

  bool change = true;
  int frame = 0;

  while (1) {
    if (change) {
      playlist_display((frame & 32) ? true : false);
      change = false;
    }

    uint8_t keyState = checkKey();
    if (keyState & PAD_UP)
    {
      playlist_move(-1);
      set_sound(note_table[3*12],0x0a);
    }
    if (keyState & PAD_DOWN)
    {
      playlist_move(1);
      set_sound(note_table[3*12],0x0a);
    }
    if (keyState  & (PAD_ACT | PAD_ESC)) break;
    delay(5);
    set_sound(0,0);
    ++frame;
    if (!(frame & 31)) change = true;
  }
}



void playing_screen()
{
  for (int i = 0; i < SPEC_BANDS; ++i) spec_levels[i] = 0;

  myESPboy.tft.fillScreen(TFT_BLACK);

  char* title = (char*)playlist[playlist_cur * 2];

  printFast(4, 24, (char*)"NOW PLAYING...", TFT_GREEN, TFT_BLACK);
  printFast(128 - 4 - strlen(title) * 6, 32, title, TFT_GREEN, TFT_BLACK);

  int sx = SPEC_SX;
  int sy = SPEC_SY + SPEC_HEIGHT + 1;

  for (int i = 0; i < SPEC_BANDS; ++i)
  {
    myESPboy.tft.fillRect(sx, sy, SPEC_BAND_WIDTH - 1, 1, TFT_GREEN);
    sx += SPEC_BAND_WIDTH;
  }

  music_start(playlist[playlist_cur * 2 + 1]);

  while (music.active)
  {
    sx = SPEC_SX;
    sy = SPEC_SY;

    for (int i = 0; i < SPEC_BANDS; ++i)
    {
      int h = spec_levels[i];

      if (h > SPEC_HEIGHT) h = SPEC_HEIGHT;

      myESPboy.tft.fillRect(sx, sy, 5, SPEC_HEIGHT - h, TFT_BLACK);
      myESPboy.tft.fillRect(sx, sy + SPEC_HEIGHT - h, SPEC_BAND_WIDTH - 1, h, TFT_GREEN);

      sx += SPEC_BAND_WIDTH;
    }

    if (checkKey()) break;

    delay(1000 / 120);
  }

  music_stop();
}



void setup() {

  //Init ESPboy

  myESPboy.begin(((String)F("Faulty Robots")).c_str());

  sound_frame_cnt = 0;
  sound_shift = 0;
  sound_shift_copy = 0;
  sound_acc = 0;
  sound_add = 0;
  sound_out = 0;

  const float notes[12] = { 4186.01f, 4434.92f, 4698.63f, 4978.03f, 5274.04f, 5587.65f, 5919.91f, 6271.93f, 6644.88f, 7040.00f, 7458.62f, 7902.13f };

  for (int octave = 0; octave < 5; ++octave)
  {
    for (int note = 0; note < 12; ++note)
    {
      note_table[octave * 12 + note] = (unsigned int)(65535.0f * 65536.0f * notes[note] * 8.0f / SAMPLE_RATE / ((float)(1 << (6 - octave))));
    }
  }

  noInterrupts();
  timer1_attachInterrupt(sound_ISR);
  timer1_enable(TIM_DIV1, TIM_EDGE, TIM_LOOP);
  timer1_write(80000000 / SAMPLE_RATE);
  interrupts();
}



void loop() {

  playlist_cur = 0;

  //title screen (skippable)

  if (title_screen_effect(0))
  {
    wait_any_key(3000);
    title_screen_effect(1);
  }

  //main loop

  while (1)
  {
    playlist_screen();
    playing_screen();
  }
}
