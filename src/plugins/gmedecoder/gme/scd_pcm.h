#include "mamedef.h"

struct pcm_chan_ {
	unsigned int ENV;		/* envelope register */
	unsigned int PAN;		/* pan register */
	unsigned int MUL_L;		/* envelope & pan product letf */
	unsigned int MUL_R;		/* envelope & pan product right */
	unsigned int St_Addr;	/* start address register */
	unsigned int Loop_Addr;	/* loop address register */
	unsigned int Addr;		/* current address register */
	unsigned int Step;		/* frequency register */
	unsigned int Step_B;	/* frequency register binaire */
	unsigned int Enable;	/* channel on/off register */
	int Data;				/* wave data */
	unsigned int Muted;
};

struct pcm_chip_
{
	float Rate;
	int Enable;
	int Cur_Chan;
	int Bank;

	struct pcm_chan_ Channel[8];
	
	unsigned long int RAMSize;
	unsigned char* RAM;
};

//extern struct pcm_chip_ PCM_Chip;
//extern unsigned char Ram_PCM[64 * 1024];
//extern int PCM_Enable;

//int  PCM_Init(int Rate);
//void PCM_Set_Rate(int Rate);
//void PCM_Reset(void);
//void PCM_Write_Reg(unsigned int Reg, unsigned int Data);
//int  PCM_Update(int **buf, int Length);

#ifdef __cplusplus
extern "C" {
#endif

void rf5c164_update(void *chip, stream_sample_t **outputs, int samples);
void * device_start_rf5c164(int clock);
void device_stop_rf5c164(void *chip);
void device_reset_rf5c164(void *chip);
void rf5c164_w(void *chip, offs_t offset, UINT8 data);
void rf5c164_mem_w(void *chip, offs_t offset, UINT8 data);
void rf5c164_write_ram(void *chip, offs_t DataStart, offs_t DataLength, const UINT8* RAMData);

void rf5c164_set_mute_mask(void *chip, UINT32 MuteMask);

#ifdef __cplusplus
}
#endif
