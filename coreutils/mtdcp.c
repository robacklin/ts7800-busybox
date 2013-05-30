/* Copyright 2005-2006, Unpublished Work of Technologic Systems
 * All Rights Reserved.
 *
 * THIS WORK IS AN UNPUBLISHED WORK AND CONTAINS CONFIDENTIAL,
 * PROPRIETARY AND TRADE SECRET INFORMATION OF TECHNOLOGIC SYSTEMS.
 * ACCESS TO THIS WORK IS RESTRICTED TO (I) TECHNOLOGIC SYSTEMS 
 * EMPLOYEES WHO HAVE A NEED TO KNOW TO PERFORM TASKS WITHIN THE SCOPE
 * OF THEIR ASSIGNMENTS AND (II) ENTITIES OTHER THAN TECHNOLOGIC
 * SYSTEMS WHO HAVE ENTERED INTO APPROPRIATE LICENSE AGREEMENTS.  NO
 * PART OF THIS WORK MAY BE USED, PRACTICED, PERFORMED, COPIED, 
 * DISTRIBUTED, REVISED, MODIFIED, TRANSLATED, ABRIDGED, CONDENSED, 
 * EXPANDED, COLLECTED, COMPILED, LINKED, RECAST, TRANSFORMED, ADAPTED
 * IN ANY FORM OR BY ANY MEANS, MANUAL, MECHANICAL, CHEMICAL, 
 * ELECTRICAL, ELECTRONIC, OPTICAL, *BIOLOGICAL*, OR OTHERWISE WITHOUT
 * THE PRIOR WRITTEN PERMISSION AND CONSENT OF TECHNOLOGIC SYSTEMS.
 * ANY USE OR EXPLOITATION OF THIS WORK WITHOUT THE PRIOR WRITTEN
 * CONSENT OF TECHNOLOGIC SYSTEMS COULD SUBJECT THE PERPETRATOR TO
 * CRIMINAL AND CIVIL LIABILITY.
 */

#include "libbb.h"
#include "mtd.h"

int getMTDInfo(FILE *ff,mtd_info_t *inf) {
  if (ioctl(fileno(ff), MEMGETINFO, inf) == -1) {
    perror("ioctl:MEMGETINFO");
    return 0;
  }
  return 1;
}

inline int MTDoffset(MTDFILE *f,int block,int sector) {
  return f->info.erasesize * block +  f->info.oobblock * sector;
}

inline int MTDblock(MTDFILE *f,long offset) {
  return offset / f->info.erasesize;
}

inline int MTDsector(MTDFILE *f,long offset) {
  return (offset / f->info.oobblock) % 
(f->info.erasesize/f->info.oobblock);
}

inline int MTDbyte(MTDFILE *f,long offset) {
  return (offset % f->info.oobblock);
}

inline int MTDseek(MTDFILE *f,int block,int sector,int byte) {
  return (fseek(f->f,byte+MTDoffset(f,block,sector),SEEK_SET) == 0);
}

int MTDreadOOB(MTDFILE *f,int block,int sector,char *oob) {
  struct mtd_oob_buf oob_buf;

  if (!MTDseek(f,block,sector,0)) {
    return 0;
  }

  oob_buf.length = f->info.oobsize;
  oob_buf.ptr = oob;
  oob_buf.start = MTDoffset(f,block,sector);

  if (ioctl(fileno(f->f), MEMREADOOB, &oob_buf) == -1 ||
      ferror(f->f)) {
    clearerr(f->f);
    return 0;
  }
  return 1;
}

int MTDeraseBlock(MTDFILE *f,int block) {
  erase_info_t erase;
  //char oob[f->info.oobsize];

  erase.start = block * f->info.erasesize;
  erase.length = f->info.erasesize;

  /*
  if (!MTDreadOOB(f,block,0,oob) || oob[5] != 0xFF) {
    return 0; // don't try to erase if block is marked bad
  }
  */
  if (ioctl(fileno(f->f), MEMERASE, &erase) == -1
      || ferror(f->f)) {
    clearerr(f->f);
    return 0;
  }
  return 1;
}

void MTDinvalidateBlock(MTDFILE *f,int block) {
  struct mtd_oob_buf oob_buf;
  unsigned char oob[f->info.oobsize];

  MTDeraseBlock(f,block); // try to erase it first
  oob_buf.length = f->info.oobsize;
  oob_buf.ptr = oob;
  oob_buf.start = f->info.erasesize * block;

  memset(oob,0,f->info.oobsize);
  ioctl(fileno(f->f), MEMWRITEOOB, &oob_buf);
}

int MTDreadBlock(MTDFILE *f,char *buf,int len) {
  int rc;

  if (ftell(f->f) >= f->info.size) {
    printf("MTDreadBlock past end of device, aborting\n");
    exit(3);
  }
  if ((rc=fread(buf,len,1,f->f)) != 1
      || ferror(f->f)) {
    clearerr(f->f);
    return 0;
  } else {
    return 1;
  }
}

int MTDwriteBlock(MTDFILE *f,char *buf,int len) {
  if (ftell(f->f)+len >= f->info.size) {
    printf("MTDreadBlock past end of device, aborting\n");
    exit(3);
  }
  if (fwrite(buf,len,1,f->f) != 1
      || ferror(f->f)) {
    clearerr(f->f);
    return 0;
  } else {
    return 1;
  }
}

int MTDverifyBlock(MTDFILE *f,char *buf,int len) {
  char buf2[len];

  if (fseek(f->f,-len,SEEK_CUR) != 0) {
    return 0;
  }
  if (fread(buf2,len,1,f->f) != 1) {
    return 0;
  }
  return (memcmp(buf,buf2,len) == 0);
}

int MTDread(MTDFILE *f,char *buf,int len) {
  int offset,block,sector,byte,len0;
  static int maxerr = 6;

  offset = ftell(f->f);
  block = MTDblock(f,offset);
  sector = MTDsector(f,offset);
  byte = MTDbyte(f,offset);
  if (sector != 0 || byte != 0) { // offset must be block aligned
    return 0;
  }
  while (len > 0) {
    len0 = len > f->info.erasesize ? f->info.erasesize : len;
    if (!MTDreadBlock(f,buf,len0)) {
      printf("read: skipping bad block %d\n",block);
      if (maxerr-- <= 0) {
	printf("Too many errors -- aborting\n");
	exit(3);
      }
      block++;
      if (!MTDseek(f,block,0,0)) {
	return 0;
      }
      continue;
    }
    len -= len0;
    buf += len0;
  }
  return 1;
}

int MTDwrite(MTDFILE *f,char *buf,int len) {
  int offset,block,sector,byte,len0;
  static int maxerr = 6;

  offset = ftell(f->f);
  block = MTDblock(f,offset);
  sector = MTDsector(f,offset);
  byte = MTDbyte(f,offset);
  if (sector != 0 || byte != 0) { // offset must be block aligned
    return 0;
  }
  while (len > 0) {
    //len0 = len > f->info.erasesize ? f->info.erasesize : len;
    len0 = f->info.erasesize;
    //printf("Writing %ld for %d\n",ftell(f->f),len);
    if (!MTDeraseBlock(f,block) || 
	!MTDwriteBlock(f,buf,len0) || 
	!MTDverifyBlock(f,buf,len0)) {
      printf("write: skipping bad block %d\n",block);
      if (maxerr-- <= 0) {
	printf("Too many errors -- aborting\n");
	exit(3);
      }
      MTDinvalidateBlock(f,block);
      block++;
      if (!MTDseek(f,block,0,0)) {
	return 0;
      }
      continue;
    } 
    len -= len0;
    buf += len0;
    block++;
  }
  return 1;
}

MTDFILE *MTDopen(char *name) {
  MTDFILE *f = malloc(sizeof(MTDFILE));

  f->f = fopen(name,"r+");
  if (f->f == NULL) {
    free(f);
    return 0;
  }
  if (!getMTDInfo(f->f,&f->info)) {
    fclose(f->f);
    free(f);
    return 0;
  }
  setvbuf(f->f,0,_IONBF,0);
  return f;
}

void MTDclose(MTDFILE *f) {
  if (!f) {
    return;
  }
  fclose(f->f);
  free(f);
}

/*
	Copied from app/mtd ... Changes there are not propogated here
	automatically.
*/
static void err(char *dir,char *type,FILE *f,MTDFILE *f2) {
  int ft = ftell(f);

  printf("Error %s %s: %m\n",dir,type);
  printf("Offset %X (%d)\n",ft,ft);
  printf("Block %d, sector %d, byte %d\n",
	 MTDblock(f2,ft),MTDsector(f2,ft),MTDbyte(f2,ft));
}

static void usage(char **argv) {
    fprintf(stderr,"Usage:\n");
    fprintf(stderr,"\t%s dest-partition# start-block# bytes dest-file\n",argv[0]);
    fprintf(stderr,"\t%s src-file dest-partition# start-block#\n",argv[0]);
}

int mtdcp_main(int argc, char **argv);
int mtdcp_main(int argc, char **argv) {
  MTDFILE *f;
  FILE *f2;
  char tmp[256],*buf;
  int block,len=0,len0,i;
  struct stat stat2;

  if (argc == 4) { // copy TO MTD
    sprintf(tmp,"/dev/mtd/%d",atoi(argv[2]));
    f = MTDopen(tmp);
    if (f == NULL) {
      printf("MTDopen(%s) failed: %m\n",tmp);
      return 0;
    }
    block = atoi(argv[3]);
    if (!MTDseek(f,block,0,0)) {
      printf("Can't find starting MTD block %d: %m\n",block);
      return 0;
    }
    f2 = fopen(argv[1],"r");
    if (!f2) {
      printf("Can't open source file %s: %m\n",argv[1]);
      return 0;
    }
  } else if (argc == 5) { // copy FROM MTD
    sprintf(tmp,"/dev/mtd/%d",atoi(argv[1]));
    f = MTDopen(tmp);
    if (f == NULL) {
      printf("MTDopen(%s) failed: %m\n",tmp);
      return 0;
    }
    block = atoi(argv[2]);
    if (!MTDseek(f,block,0,0)) {
      printf("Can't find starting MTD block %d: %m\n",block);
      return 0;
    }
    len = atoi(argv[3]);
    f2 = fopen(argv[4],"w");
    if (!f2) {
      printf("Can't open destination file %s: %m\n",argv[1]);
      return 0;
    }
  } else {
    usage(argv);
    return 0;
  }

  buf = malloc(f->info.erasesize);
  if (len == 0) { // copying from file to MTD
    fstat(fileno(f2),&stat2);
    len = stat2.st_size;
    if (len > f->info.size) {
      printf("File size is %d, destination can only hold %d (aborting)\n",
	     len,f->info.size);
      return 1;
    }
    if (len == 0) {
      while (!feof(f2)) {
	for (i=0;i<f->info.erasesize;i++) {
	  buf[i] = 0xFF;
	}
	if (fread(buf,1,f->info.erasesize,f2) > 0) {
	  if (!MTDwrite(f,buf,f->info.erasesize)) {
	    err("writing","MTD",f->f,f);
	    return 0;
	  }
	} else {
	  break;
	}
      }
    } else while (len > 0) {
      len0 = len > f->info.erasesize ? f->info.erasesize : len;
      if (fread(buf,len0,1,f2) != 1) {
	err("reading","file",f2,f);
	return 0;
      }
      if (!MTDwrite(f,buf,len0)) {
	err("writing","MTD",f->f,f);
	return 0;
      }
      len -= len0;
    }
  } else { // copying from MTD to file
    while (len > 0) {
      len0 = len > f->info.erasesize ? f->info.erasesize : len;
      if (!MTDread(f,buf,len0)) {
	err("reading","MTD",f->f,f);
	return 0;
      }
      if (fwrite(buf,len0,1,f2) != 1) {
	err("writing","file",f2,f);
	return 0;
      }
      len -= len0;
    }
  }
  return 0;
}
