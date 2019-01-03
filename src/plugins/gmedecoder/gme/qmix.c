/////////////////////////////////////////////////////////////////////////////
//
// qmix - QSound mixer
//
/////////////////////////////////////////////////////////////////////////////

#include "qmix.h"

/////////////////////////////////////////////////////////////////////////////

#define ANTICLICK_TIME        (64)
#define ANTICLICK_THRESHHOLD  (32)

/////////////////////////////////////////////////////////////////////////////

#define RENDERMAX (200)

/////////////////////////////////////////////////////////////////////////////

static const sint32 gauss_shuffled_reverse_table[1024] = {
 366,1305, 374,   0, 362,1304, 378,   0, 358,1304, 381,   0, 354,1304, 385,   0, 351,1304, 389,   0, 347,1304, 393,   0, 343,1303, 397,   0, 339,1303, 401,   0,
 336,1303, 405,   0, 332,1302, 410,   0, 328,1302, 414,   0, 325,1301, 418,   0, 321,1300, 422,   0, 318,1300, 426,   0, 314,1299, 430,   0, 311,1298, 434,   0,
 307,1297, 439,   1, 304,1297, 443,   1, 300,1296, 447,   1, 297,1295, 451,   1, 293,1294, 456,   1, 290,1293, 460,   1, 286,1292, 464,   1, 283,1291, 469,   1,
 280,1290, 473,   1, 276,1288, 477,   1, 273,1287, 482,   1, 270,1286, 486,   2, 267,1284, 491,   2, 263,1283, 495,   2, 260,1282, 499,   2, 257,1280, 504,   2,
 254,1279, 508,   2, 251,1277, 513,   2, 248,1275, 517,   3, 245,1274, 522,   3, 242,1272, 527,   3, 239,1270, 531,   3, 236,1269, 536,   3, 233,1267, 540,   4,
 230,1265, 545,   4, 227,1263, 550,   4, 224,1261, 554,   4, 221,1259, 559,   4, 218,1257, 563,   5, 215,1255, 568,   5, 212,1253, 573,   5, 210,1251, 577,   5,
 207,1248, 582,   6, 204,1246, 587,   6, 201,1244, 592,   6, 199,1241, 596,   6, 196,1239, 601,   7, 193,1237, 606,   7, 191,1234, 611,   7, 188,1232, 615,   8,
 186,1229, 620,   8, 183,1227, 625,   8, 180,1224, 630,   9, 178,1221, 635,   9, 175,1219, 640,   9, 173,1216, 644,  10, 171,1213, 649,  10, 168,1210, 654,  10,
 166,1207, 659,  11, 163,1205, 664,  11, 161,1202, 669,  11, 159,1199, 674,  12, 156,1196, 678,  12, 154,1193, 683,  13, 152,1190, 688,  13, 150,1186, 693,  14,
 147,1183, 698,  14, 145,1180, 703,  15, 143,1177, 708,  15, 141,1174, 713,  15, 139,1170, 718,  16, 137,1167, 723,  16, 134,1164, 728,  17, 132,1160, 732,  17,
 130,1157, 737,  18, 128,1153, 742,  19, 126,1150, 747,  19, 124,1146, 752,  20, 122,1143, 757,  20, 120,1139, 762,  21, 118,1136, 767,  21, 117,1132, 772,  22,
 115,1128, 777,  23, 113,1125, 782,  23, 111,1121, 787,  24, 109,1117, 792,  24, 107,1113, 797,  25, 106,1109, 802,  26, 104,1106, 806,  27, 102,1102, 811,  27,
 100,1098, 816,  28,  99,1094, 821,  29,  97,1090, 826,  29,  95,1086, 831,  30,  94,1082, 836,  31,  92,1078, 841,  32,  90,1074, 846,  32,  89,1070, 851,  33,
  87,1066, 855,  34,  86,1061, 860,  35,  84,1057, 865,  36,  83,1053, 870,  36,  81,1049, 875,  37,  80,1045, 880,  38,  78,1040, 884,  39,  77,1036, 889,  40,
  76,1032, 894,  41,  74,1027, 899,  42,  73,1023, 904,  43,  71,1019, 908,  44,  70,1014, 913,  45,  69,1010, 918,  46,  67,1005, 923,  47,  66,1001, 927,  48,
  65, 997, 932,  49,  64, 992, 937,  50,  62, 988, 941,  51,  61, 983, 946,  52,  60, 978, 951,  53,  59, 974, 955,  54,  58, 969, 960,  55,  56, 965, 965,  56,
  55, 960, 969,  58,  54, 955, 974,  59,  53, 951, 978,  60,  52, 946, 983,  61,  51, 941, 988,  62,  50, 937, 992,  64,  49, 932, 997,  65,  48, 927,1001,  66,
  47, 923,1005,  67,  46, 918,1010,  69,  45, 913,1014,  70,  44, 908,1019,  71,  43, 904,1023,  73,  42, 899,1027,  74,  41, 894,1032,  76,  40, 889,1036,  77,
  39, 884,1040,  78,  38, 880,1045,  80,  37, 875,1049,  81,  36, 870,1053,  83,  36, 865,1057,  84,  35, 860,1061,  86,  34, 855,1066,  87,  33, 851,1070,  89,
  32, 846,1074,  90,  32, 841,1078,  92,  31, 836,1082,  94,  30, 831,1086,  95,  29, 826,1090,  97,  29, 821,1094,  99,  28, 816,1098, 100,  27, 811,1102, 102,
  27, 806,1106, 104,  26, 802,1109, 106,  25, 797,1113, 107,  24, 792,1117, 109,  24, 787,1121, 111,  23, 782,1125, 113,  23, 777,1128, 115,  22, 772,1132, 117,
  21, 767,1136, 118,  21, 762,1139, 120,  20, 757,1143, 122,  20, 752,1146, 124,  19, 747,1150, 126,  19, 742,1153, 128,  18, 737,1157, 130,  17, 732,1160, 132,
  17, 728,1164, 134,  16, 723,1167, 137,  16, 718,1170, 139,  15, 713,1174, 141,  15, 708,1177, 143,  15, 703,1180, 145,  14, 698,1183, 147,  14, 693,1186, 150,
  13, 688,1190, 152,  13, 683,1193, 154,  12, 678,1196, 156,  12, 674,1199, 159,  11, 669,1202, 161,  11, 664,1205, 163,  11, 659,1207, 166,  10, 654,1210, 168,
  10, 649,1213, 171,  10, 644,1216, 173,   9, 640,1219, 175,   9, 635,1221, 178,   9, 630,1224, 180,   8, 625,1227, 183,   8, 620,1229, 186,   8, 615,1232, 188,
   7, 611,1234, 191,   7, 606,1237, 193,   7, 601,1239, 196,   6, 596,1241, 199,   6, 592,1244, 201,   6, 587,1246, 204,   6, 582,1248, 207,   5, 577,1251, 210,
   5, 573,1253, 212,   5, 568,1255, 215,   5, 563,1257, 218,   4, 559,1259, 221,   4, 554,1261, 224,   4, 550,1263, 227,   4, 545,1265, 230,   4, 540,1267, 233,
   3, 536,1269, 236,   3, 531,1270, 239,   3, 527,1272, 242,   3, 522,1274, 245,   3, 517,1275, 248,   2, 513,1277, 251,   2, 508,1279, 254,   2, 504,1280, 257,
   2, 499,1282, 260,   2, 495,1283, 263,   2, 491,1284, 267,   2, 486,1286, 270,   1, 482,1287, 273,   1, 477,1288, 276,   1, 473,1290, 280,   1, 469,1291, 283,
   1, 464,1292, 286,   1, 460,1293, 290,   1, 456,1294, 293,   1, 451,1295, 297,   1, 447,1296, 300,   1, 443,1297, 304,   1, 439,1297, 307,   0, 434,1298, 311,
   0, 430,1299, 314,   0, 426,1300, 318,   0, 422,1300, 321,   0, 418,1301, 325,   0, 414,1302, 328,   0, 410,1302, 332,   0, 405,1303, 336,   0, 401,1303, 339,
   0, 397,1303, 343,   0, 393,1304, 347,   0, 389,1304, 351,   0, 385,1304, 354,   0, 381,1304, 358,   0, 378,1304, 362,   0, 374,1305, 366,   0, 370,1305, 370,
};

static const sint32 pan_table[33] = {
   0, 724,1024,1254,1448,1619,1774,1916,
2048,2172,2290,2401,2508,2611,2709,2804,
2896,2985,3072,3156,3238,3318,3396,3473,
3547,3620,3692,3762,3831,3899,3966,4031,
4096};

/////////////////////////////////////////////////////////////////////////////
//
// Static information
//
sint32 EMU_CALL _qmix_init(void) { return 0; }

/////////////////////////////////////////////////////////////////////////////
//
// State information
//
#define QMIXSTATE ((struct QMIX_STATE*)(state))

struct QMIX_CHAN {
  uint32 on;
  uint32 startbank;
  uint32 startaddr;
  uint32 curbank;
  uint32 curaddr;
  uint32 startloop;
  uint32 startend;
  uint32 curloop;
  uint32 curend;
  uint32 phase;
  uint32 pitch;
  uint32 vol;
  uint32 pan;
  sint32 current_mix_l;
  sint32 current_mix_r;
  sint32 sample[4];
  sint32 sample_last_l;
  sint32 sample_last_r;
  sint32 sample_anticlick_l;
  sint32 sample_anticlick_r;
  sint32 sample_anticlick_remaining_l;
  sint32 sample_anticlick_remaining_r;
};

/////////////////////////////////////////////////////////////////////////////

static EMU_INLINE void get_anticlicked_samples(
  struct QMIX_CHAN *chan, sint32 *l, sint32 *r
) {
  sint32 out, remain;
  remain = chan->sample_anticlick_remaining_l;
  if(remain) {
    sint32 diff = chan->sample_last_l - chan->sample_anticlick_l;
    if(diff < 0) { diff = -diff; }
    if(diff < ANTICLICK_THRESHHOLD) {
      out = chan->sample_last_l;
      chan->sample_anticlick_remaining_l = 0;
    } else {
      out = (
        chan->sample_last_l * (ANTICLICK_TIME-remain) +
        chan->sample_anticlick_l * (remain)
      ) / ANTICLICK_TIME;
      chan->sample_anticlick_remaining_l--;
    }
  } else {
    out = chan->sample_last_l;
  }
  *l = out;

  remain = chan->sample_anticlick_remaining_r;
  if(remain) {
    sint32 diff = chan->sample_last_r - chan->sample_anticlick_r;
    if(diff < 0) { diff = -diff; }
    if(diff < ANTICLICK_THRESHHOLD) {
      out = chan->sample_last_r;
      chan->sample_anticlick_remaining_r = 0;
    } else {
      out = (
        chan->sample_last_r * (ANTICLICK_TIME-remain) +
        chan->sample_anticlick_r * (remain)
      ) / ANTICLICK_TIME;
      chan->sample_anticlick_remaining_r--;
    }
  } else {
    out = chan->sample_last_r;
  }
  *r = out;

}

/////////////////////////////////////////////////////////////////////////////

static EMU_INLINE void anticlick(struct QMIX_CHAN *chan) {
  sint32 l, r;
  get_anticlicked_samples(chan, &l, &r);
  chan->sample_anticlick_l = l;
  chan->sample_anticlick_r = r;
  chan->sample_anticlick_remaining_l = ANTICLICK_TIME;
  chan->sample_anticlick_remaining_r = ANTICLICK_TIME;
}

/////////////////////////////////////////////////////////////////////////////

struct QMIX_STATE {
  uint8 *sample_rom;
  uint32 sample_rom_size;
  uint32 pitchscaler;
  struct QMIX_CHAN chan[16];
  sint32 last_in_l;
  sint32 last_in_r;
  sint32 last_out_l;
  sint32 last_out_r;
  sint32 acc_l;
  sint32 acc_r;
};

uint32 EMU_CALL _qmix_get_state_size(void) {
  return sizeof(struct QMIX_STATE);
}

void EMU_CALL _qmix_clear_state(void *state) {
  memset(state, 0, sizeof(struct QMIX_STATE));


}

void EMU_CALL _qmix_set_sample_rom(void *state, void *rom, uint32 size) {
  QMIXSTATE->sample_rom = rom;
  QMIXSTATE->sample_rom_size = size;
}

/////////////////////////////////////////////////////////////////////////////

static EMU_INLINE void chan_advance(
  struct QMIX_STATE *state,
  struct QMIX_CHAN *chan
) {
  uint32 rom_addr = chan->curbank + chan->curaddr;
  if(rom_addr >= state->sample_rom_size) rom_addr = 0;
  chan->sample[0] = chan->sample[1];
  chan->sample[1] = chan->sample[2];
  chan->sample[2] = chan->sample[3];
  chan->sample[3] = (sint32)((sint8)(state->sample_rom[rom_addr]));
  chan->curaddr++;
// FIXME: MAME thinks this is >=, but is it > ?
  if(chan->curaddr >= chan->curend) {
//    if(!chan->curloop) {
//      chan->on = 0;
//      chan->curaddr--;
//    } else {
      chan->curaddr = chan->curend - chan->curloop;
//      chan->curaddr -= 1 + chan->curloop;
//    }
  }
  chan->curaddr &= 0xFFFF;
}

/////////////////////////////////////////////////////////////////////////////

static EMU_INLINE sint32 chan_get_resampled(
  struct QMIX_STATE *state,
  struct QMIX_CHAN *chan
) {
  sint32 sum;
  sint32 phase = chan->phase & 0xFFF;

//  sum = chan->sample[2];
//  sum <<= 8;

  const sint32 *gauss = (sint32*)
    (((sint8*)gauss_shuffled_reverse_table) + (phase & 0x0FF0));
  sum =  chan->sample[0] * gauss[0];
  sum += chan->sample[1] * gauss[1];
  sum += chan->sample[2] * gauss[2];
  sum += chan->sample[3] * gauss[3];
  sum /= 8;

//  sum  = chan->sample[1] * (0x1000-phase);
//  sum += chan->sample[2] * (       phase);
//  sum >>= 4;

  chan->phase += chan->pitch;
  while(chan->phase >= 0x1000) {
    chan_advance(state, chan);
    chan->phase -= 0x1000;
  }

  return sum;
}

static EMU_INLINE void chan_get_stereo_anticlicked(
  struct QMIX_STATE *state,
  struct QMIX_CHAN *chan,
  sint32 *l,
  sint32 *r
) {
  if(!chan->on) {
    chan->sample_last_l = 0;
    chan->sample_last_r = 0;
  } else {
    sint32 out = chan_get_resampled(state, chan);
    chan->sample_last_l = (out * chan->current_mix_l) / 0x8000;
    chan->sample_last_r = (out * chan->current_mix_r) / 0x8000;
    // if we suddenly keyed off, perform an anticlick here
    if(!chan->on) { anticlick(chan); }
  }
  get_anticlicked_samples(chan, l, r);
}

/////////////////////////////////////////////////////////////////////////////

static void recalc_mix(struct QMIX_CHAN *chan) {
  sint32 realpan = (chan->pan & 0x3F) - 0x10;
  sint32 realvol = chan->vol & 0xFFFF;
  if(realpan < 0x00) realpan = 0x00;
  if(realpan > 0x20) realpan = 0x20;

//  chan->current_mix_l = realvol << 3;
//  chan->current_mix_r = realvol << 3;
//  if(realpan < 0x10) {
//    chan->current_mix_r *= realpan;
//    chan->current_mix_r >>= 4;
//  }
//  if(realpan > 0x10) {
//    chan->current_mix_l *= (0x20-realpan);
//    chan->current_mix_l >>= 4;
//  }

//  chan->current_mix_l = ((0x20-realpan) * realvol) >> 1;
//  chan->current_mix_r = ((     realpan) * realvol) >> 1;

  chan->current_mix_l = (realvol * pan_table[0x20-realpan]) / 0x2000;
  chan->current_mix_r = (realvol * pan_table[     realpan]) / 0x2000;

  // perform anticlick
  //anticlick(chan);
}

/////////////////////////////////////////////////////////////////////////////
//
// Command handling
//
//#include <stdio.h>

void EMU_CALL _qmix_command(void *state, uint8 cmd, uint16 data) {
  struct QMIX_CHAN *chan;
  uint32 ch = 0;
  uint32 reg = 99;
//printf("qmix command 0x%02X:0x%04X\n",cmd,data);
  if(cmd < 0x80) {
    reg  = cmd & 7;
    ch   = cmd >> 3;
  } else if(cmd < 0x90) {
    reg  = 8;
    ch   = cmd - 0x80;
  } else if(cmd >= 0xBA && cmd < 0xCA) {
    reg  = 9;
    ch   = cmd - 0xBA;
  } else {
    reg  = 99;
    ch   = 0;
  }
  chan = QMIXSTATE->chan + ch;
  switch(reg) {
  case 0: // bank
    ch = (ch+1) & 0xF; chan = QMIXSTATE->chan + ch;
    //printf("qmix: bank ch%X = %04X\n",ch,data);
    chan->startbank = (((uint32)data) & 0x7F) << 16;
    break;
  case 1: // start
    //printf("qmix: start ch%X = %04X\n",ch,data);
    chan->startaddr = ((uint32)data) & 0xFFFF;
    break;
  case 2: // pitch
    //printf("qmix: pitch ch%X = %04X\n",ch,data);
    chan->pitch = (((uint32)(data & 0xFFFF)) * QMIXSTATE->pitchscaler) / 0x10000;
    if (chan->pitch == 0) {
      chan->on = 0;
      anticlick(chan);
    }
    break;
  case 3: // unknown
    break;
  case 4: // loop start
    //printf("qmix: loop ch%X = %04X\n",ch,data);
    chan->startloop = data;
    break;
  case 5: // end
    //printf("qmix: end ch%X = %04X\n",ch,data);
    chan->startend = data;
    break;
  case 6: // volume
    //printf("qmix: vol ch%X = %04X\n",ch,data);
//printf("volume=%04X\n",data);
//    if(!data) {
//      chan->on = 0;
//    } else {
//      chan->on = 1;
//      chan->address = chan->start;
//      chan->phase = 0;
//    }
      //printf("qmix: unknown reg3 ch%X = %04X\n",ch,data);
    if(data == 0) {
      chan->on = 0;
      anticlick(chan);
    } else if (chan->on == 0) {
      chan->on = 1;
      chan->curbank = chan->startbank;
      chan->curaddr = chan->startaddr;
      chan->curloop = chan->startloop;
      chan->curend  = chan->startend;
      chan->phase = 0;
      chan->sample[0] = 0;
      chan->sample[1] = 0;
      chan->sample[2] = 0;
      chan->sample[3] = 0;
      anticlick(chan);
    }

    chan->vol = data;
    recalc_mix(chan);
    break;
  case 7: // unknown
    //printf("qmix: unknown reg7 ch%X = %04X\n",ch,data);
    break;
  case 8: // pan (0x110-0x130)
    //printf("qmix: pan ch%X = %04X\n",ch,data);
//printf("pan=%04X\n",data);
    chan->pan = data;
    recalc_mix(chan);
    break;
  case 9: // ADSR?
    //printf("qmix: unknown reg9 ch%X = %04X\n",ch,data);
    break;
  default:
    //printf("qmix: unknown reg %02X = %04X\n",cmd,data);
    break;
  }
}

/////////////////////////////////////////////////////////////////////////////
//
// Rendering
//
static void render(
  struct QMIX_STATE *state,
  sint16 *buf,
  uint32 samples
) {
  sint32 buf_l[RENDERMAX];
  sint32 buf_r[RENDERMAX];
  sint32 l, r;
  uint32 s;
  int ch;
  memset(buf_l, 0, 4 * samples);
  memset(buf_r, 0, 4 * samples);
  for(ch = 0; ch < 16; ch++) {
    struct QMIX_CHAN *chan = state->chan + ch;
    for(s = 0; s < samples; s++) {
      chan_get_stereo_anticlicked(state, chan, &l, &r);
      buf_l[s] += l;
      buf_r[s] += r;
    }
  }
  if(!buf) return;
  for(s = 0; s < samples; s++) {
    sint32 diff_l = buf_l[s] - state->last_in_l;
    sint32 diff_r = buf_r[s] - state->last_in_r;
    state->last_in_l = buf_l[s];
    state->last_in_r = buf_r[s];
    l = ((state->last_out_l * 255) / 256) + diff_l;
    r = ((state->last_out_r * 255) / 256) + diff_r;
    state->last_out_l = l;
    state->last_out_r = r;
//    l /= 2;
//    r /= 2;
    l *= 8;
    r *= 8;
    if(l > ( 32767)) l = ( 32767);
    if(l < (-32768)) l = (-32768);
    if(r > ( 32767)) r = ( 32767);
    if(r < (-32768)) r = (-32768);
    *buf++ = l;
    *buf++ = r;
  }
}

/////////////////////////////////////////////////////////////////////////////

void EMU_CALL _qmix_render(void *state, sint16 *buf, uint32 samples) {
//printf("qmix render %u samples\n",samples);
  for(; samples >= RENDERMAX; samples -= RENDERMAX) {
    render(QMIXSTATE, buf, RENDERMAX);
    if(buf) buf += 2 * RENDERMAX;
  }
  if(samples) {
    render(QMIXSTATE, buf, samples);
  }
}

/////////////////////////////////////////////////////////////////////////////

void EMU_CALL _qmix_set_sample_rate(void *state, uint32 rate) {
  if(rate < 1) rate = 1;
  QMIXSTATE->pitchscaler = (65536 * 24000) / rate;
}

/////////////////////////////////////////////////////////////////////////////
