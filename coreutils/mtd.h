#include "mtd-user.h"

typedef struct MTDFILE {
  FILE *f;
  mtd_info_t info;
} MTDFILE;

inline int MTDoffset(MTDFILE *f,int block,int sector);
inline int MTDblock(MTDFILE *f,long offset);
inline int MTDsector(MTDFILE *f,long offset);
inline int MTDbyte(MTDFILE *f,long offset);
inline int MTDseek(MTDFILE *f,int block,int sector,int byte);
int MTDreadOOB(MTDFILE *f,int block,int sector,char *oob);
int MTDeraseBlock(MTDFILE *f,int block);
void MTDinvalidateBlock(MTDFILE *f,int block);
int MTDwriteBlock(MTDFILE *f,char *buf,int len);
int MTDverifyBlock(MTDFILE *f,char *buf,int len);
int MTDread(MTDFILE *f,char *buf,int len);
int MTDwrite(MTDFILE *f,char *buf,int len);
MTDFILE *MTDopen(char *name);
void MTDclose(MTDFILE *f);

