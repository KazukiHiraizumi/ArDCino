#include <Arduino.h>
#include <SetTimeout.h>
#include "Algor.h"
#include "Param.h"
#include "Logger.h"
#include "Dcore.h"

//params
uint8_t algor_param[]={
  130,200,0,0,  20,100,0,0,
  15,5,20,200,  0,0,0,0,
  20,17,40,30,  40,0,0,0,
  0,155,13,159, 27,145,45,101,
  79,69,128,39, 189,23,255,19,
  0,50,10,50,  10,100,30,100,
  255,255,255,255,  255,255,255,255
};

//elapsed time
static uint32_t tusec;
//observer vars
static float wmax,wh,bh,dbh;
//profile
static uint8_t iflag;
//sigma
static uint16_t sflag;
static uint16_t stime;
static void (*scall)();  //constant interval hook
static uint16_t sspec;  //intensity
//control
static uint8_t zflag;
static uint16_t ztime;

//table
static uint8_t tbl_index;
#define MAX(a,b) ((a)>(b)? a:b)
#define MIN(a,b) ((a)<(b)? a:b)
#define ARRSZ(a) (sizeof(a)/sizeof(a[0]))

#define PRM_ReadData(n) ((uint16_t)algor_param[n])
#define PRM_ReadData10x(n) ((uint16_t)algor_param[n]*10)
#define PRM_ReadData100x(n) ((uint16_t)algor_param[n]*100)
#define PRM_ReadData1000x(n) ((uint32_t)algor_param[n]*1000)

/**************************************************************************/
static int interp(int y1,int y2,int dx,int w){
  if(w<0) return y1;
  else if(w>dx) return y2;
  else return (y2*w+y1*(dx-w))/dx;
}
static int readTbl4k(int p,int w){
  auto x1=PRM_ReadData(p)<<4;
  auto x2=PRM_ReadData(p+2)<<4;
  uint8_t y1,y2;
  if(w<0) w=0;
  if(w>4095) w=4095;
  tbl_index=0;
  while(x2<w){
    x1=x2;
    tbl_index++;
    p+=2;
    x2=PRM_ReadData(p+2)<<4;
  }
  y1=PRM_ReadData(p+1);
  y2=PRM_ReadData(p+3);
  auto y=interp(y1,y2,x2-x1,w-x1);
  return MAX(0,y);
}
static int readTblN(int p,int b,int n){
  if(b<PRM_ReadData(p)) return PRM_ReadData(p+1);
  n=(n-1)<<1;
  if(b>PRM_ReadData(p+n)) return PRM_ReadData(p+n+1);
  return readTbl4k(p,b<<4);
}
static int readProf(int prof_tbl,int prof_tmsec,uint8_t &idx){
  auto hval=readTbl4k(prof_tbl,prof_tmsec);
  idx=tbl_index;
  return hval;
}
static int readPval(int prof_tbl,int prof_idx){
  auto pval=PRM_ReadData(prof_tbl+(prof_idx<<1)+1);
  return pval;
}
static int readTval(int prof_tbl,int prof_idx){
  return PRM_ReadData(prof_tbl+(prof_idx<<1))<<4;   //as msec
}
static int smean(int twid){
  logger::ALOG *dp=logger::data+logger::length()-1;
  long sum=0;
  int tm0=(dp->stamp>>10)-twid;
  for(int dn=0;;dp--,dn++){
    sum+=dp->sigma*dp->interval;
    int tm=dp->stamp>>10;
    if(tm<stime){
      sflag=dn;
      break;
    }
    if(tm<tm0 && dn>=sflag){
      sflag=dn;
      break;
    }
  }
  return sum/(twid<<9);
}
void algor_prepare(){
  tusec=stime=ztime=sflag=iflag=zflag=0;
  scall=NULL;
}
uint16_t algor_update(int32_t dtu,int32_t otu){
  if(dtu==0) return 0;
//Measuring
  auto dt=dtu*1.0e-6;
  auto wrps=2*M_PI/dt;
  if(tusec==0){
    wmax=wh=wrps;
    dbh=bh=0;
  }
  if(wmax<wrps) wmax=wrps;
  auto tmsec=(tusec+=dtu)/1000;
  auto pole=(float)PRM_ReadData(1);
  auto h1=2*pole;
  auto h2=pole*pole;
  auto u0=(float)otu/dtu;
  auto uat=PRM_ReadData(0)/(8*M_PI); //1/Tau
  uint8_t nloop=dtu/1000;
  if(nloop<2) nloop=2;
  auto dtn=dt/nloop;
  float dbdt=0;
  for(int i=0;i<nloop;i++){
    auto werr=wrps-wh;
    auto db=werr*h2;
    wh=wh+(werr*h1+bh-wrps*uat*u0)*dtn;
    bh=bh+db*dtn;
    dbh=dbh+PRM_ReadData(2)*(db-dbh)*dtn;
    dbdt+=db;
  }
  dbdt/=nloop;
  logger::stage.omega=round(wrps);
  logger::stage.beta=round(MIN(bh,32767));

//i-Block: base profile
  auto ivalue=readProf((24),tmsec,iflag);
//s-Block: sliding mode
  auto sigv=dbdt+PRM_ReadData(5)*(bh-PRM_ReadData100x(4)); //sliding mode
  if(scall==NULL){
    if(tmsec>=PRM_ReadData10x(8)){
      stime=tmsec;
      setTimeout.set(scall=[](){
        sspec=smean(PRM_ReadData10x(10));
        if(dcore::RunLevel>=5 && logger::length()<logger::limit()) setTimeout.set(scall,PRM_ReadData10x(9));
        else sspec=0;
      },0);
    }
  }
  float sigb=(PRM_ReadData(12)==0? sigv:dbdt+PRM_ReadData(13)*(bh-PRM_ReadData100x(12)))/PRM_ReadData10x(11);
  logger::stage.sigma=MIN(255,MAX(0,sigb));
//logger
  switch(PRM_ReadData(3)){
  case 0:
    logger::stage.eval=MIN(sspec,255);
    break;
  case 1:
    logger::stage.eval=logger::stage.sigma;
    break;
  case 2:
    logger::stage.eval=sflag;
    break;
  case 3:
    logger::stage.eval=dcore::tpwm;
    break;
  }

//z-Block: output decision
  int zvalue=ivalue;
  int zlow=PRM_ReadData(19);
  switch(zflag){
    case 0:{ //brake free
      zvalue=0;
      if(wrps>PRM_ReadData100x(17)){
        zflag=1;
        ztime=tmsec;
      }
      else if(tmsec>20 && sigv<0) zflag=2;
      break;
    }
    case 1:{ //overrun brake
      zvalue=PRM_ReadData100x(16);
      auto dt=tmsec-ztime;
      if(dt>PRM_ReadData(18)) zflag=2;
      break;
    }
    case 2:{ //run level change(acceleration finished)
      zflag=3;
      dcore::shift();  //RunLevel =>4
    }
    case 3:{ //sliding mode
      if(sigv>0) zvalue=zlow;
      if(scall!=NULL){
        zflag=4;
        dcore::shift();  //RunLevel =>6
      }
      break;
    }
    case 4:{
      int zlim=zvalue*readTblN((40),wmax/100,4)/100;
      if(zlim>zlow){
        int zfb=sspec*PRM_ReadData(20)/100;
        zvalue=zlim-zfb;
        if(zvalue<zlow) zvalue=zlow;
        if(sigv>0) zvalue=zlow;
        break;
      }
      else zvalue=zlim;
    }
  }

  return zvalue;
}
