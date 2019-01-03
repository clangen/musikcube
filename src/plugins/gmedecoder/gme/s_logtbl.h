#ifndef S_LOGTBL_H__
#define S_LOGTBL_H__

#ifdef __cplusplus
extern "C" {
#endif

#define LOG_BITS 12
#define LIN_BITS 7
#define LOG_LIN_BITS 30

typedef struct
{
	void *ctx;
	void (*release)(void *ctx);
	Uint32 lineartbl[(1 << LIN_BITS) + 1];
	Uint32 logtbl[1 << LOG_BITS];
} KMIF_LOGTABLE;

KMIF_LOGTABLE *LogTableAddRef(void);

__inline static Uint32 LinToLog(KMIF_LOGTABLE *kmif_lt, Int32 l)
{
	return (l < 0) ? (kmif_lt->lineartbl[-l] + 1) : kmif_lt->lineartbl[l];
}

__inline static Int32 LogToLin(KMIF_LOGTABLE *kmif_lt, Int32 l, Uint32 sft)
{
	Int32 ret;
	Uint32 ofs;
	ofs = l + (sft << (LOG_BITS + 1));
	sft = ofs >> (LOG_BITS + 1);
	if (sft >= LOG_LIN_BITS) return 0;
	ofs = (ofs >> 1) & ((1 << LOG_BITS) - 1);
	ret = kmif_lt->logtbl[ofs] >> sft;
	return (l & 1) ? -ret : ret;
}

#ifdef __cplusplus
}
#endif

#endif /* S_LOGTBL_H__ */
