#ifndef _MIPS_CMP_AMON_H_
#define _MIPS_CMP_AMON_H_

#define CMP_AMON_WAITCODE_IN_RAM 0xA0000D00

#ifndef __ASSEMBLY__

typedef struct {
	unsigned long	pc;
	unsigned long	gp;
	unsigned long	sp;
	unsigned long	a0;
	unsigned long	_pad[3]; /* pad to cache line size to avoid thrashing */
	unsigned long	flags;
} cpulaunch_t;

#else

#define	LAUNCH_PC	0
#define	LAUNCH_GP	4
#define	LAUNCH_SP	8
#define	LAUNCH_A0	12
#define	LAUNCH_FLAGS	28

#endif

#define LOG2CPULAUNCH	5

#define LAUNCH_FREADY	1
#define LAUNCH_FGO	2
#define LAUNCH_FGONE	4

#define SCRLAUNCH	0x00000e00
#define CPULAUNCH	0x00000f00
#define NCPULAUNCH	8

/* Polling period in count cycles for secondary CPU's */
#define LAUNCHPERIOD	10000

#endif /* _MIPS_CMP_AMON_H_ */