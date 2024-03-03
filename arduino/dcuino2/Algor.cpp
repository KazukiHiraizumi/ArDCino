#include <Arduino.h>
#include <SetTimeout.h>
#include "Algor.h"
#include "Param.h"
#include "Logger.h"
#include "Dcore.h"

//params
uint8_t algor_param[]={
  0,0,0,0,  130,100,100,0,
  20,30,10,30,  20,17,50,10,
  50,100,30,0,  10,250,170,0,
  0,155,13,159,  27,145,45,101,
  79,69,128,39,  189,23,255,19,
  0,50,10,50,  10,100,30,100,
  255,255,255,255,  255,255,255,255
};

//elapsed time
static uint32_t tusec;
//observer vars
static float wmax,wh,bh,hcoef1,hcoef2,bmin,wro;
//profile
static uint8_t iflag,imaxu;
//sigma
static uint8_t sflag;
static uint16_t sst0,sst2,ssdt;
static float sbh2;
//controls
static uint8_t zflag;
static uint32_t zintg;
//table
static uint8_t tbl_index;
#define MAX(a,b) ((a)>(b)? a:b)
#define MIN(a,b) ((a)<(b)? a:b)
#define ARRSZ(a) (sizeof(a)/sizeof(a[0]))

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
void algor_prepare(){
  tusec=sflag=iflag=zflag=0;
  auto rpole=(float)PRM_ReadData(5);
  auto ipole=(float)PRM_ReadData(6);
  hcoef1=2*rpole;
  hcoef2=rpole*rpole+ipole*ipole;
}
uint16_t algor_update(int32_t dtu,int32_t otu){
  if(dtu==0) return 0;
//Measuring
  auto dt=dtu*1.0e-6;
  auto wrps=2*M_PI/dt;
  if(tusec==0){
    wmax=wh=wro=wrps;
    bh=0;
    imaxu=0;
    sbh2=sst0=sst2=ssdt=0;
  }
  if(wmax<wrps) wmax=wrps;
  auto tmsec=(tusec+=dtu)/1000;
  auto uat=(float)otu/dtu*PRM_ReadData(4)/(8*M_PI); //duty/Tau
  uint8_t nloop=dtu/1000+1;
  if(nloop<2) nloop=2;
  auto dtn=dt/nloop;
  float dbdt=0;
  float sigv=0;
  ssdt=0;
  for(int i=0;i<nloop;i++){
    auto ii=i+1;
    auto wi=(ii*wrps+(nloop-ii)*wro)/nloop;
    auto werr=wi-wh;
    auto db=werr*hcoef2;
    wh=wh+(werr*hcoef1+bh-wi*uat)*dtn;
    bh=bh+db*dtn;
    dbdt+=db;
    auto ds=db/wi+(bh-PRM_ReadData100x(8))/PRM_ReadData(9);
    sigv+=ds;
    int sigw=ds>0;
    uint16_t tnow=(tusec+dtu*ii/nloop)/1000;  //usec=>msec
    if(sigw){
      if(sst0==0) sst0=tnow;
      else if(tnow-sst0>PRM_ReadData(10)){
        sbh2=bh;
        sst2=tnow;
      }
      ssdt+=dtu/nloop;  //usec
    }
    else sst0=0;
  }
  wro=wrps;
  dbdt/=nloop;
  sigv/=nloop;
  logger::stage.omega=round(wrps);
  logger::stage.beta=round(MIN(32767,bh));

//i-Block: base profile
  auto ivalue=readProf((24),tmsec,iflag)*readTblN((40),wmax/100,4)/100;
  if(ivalue>imaxu) imaxu=ivalue;

//z-Block: output decisionlogger::stage
  int zvalue=ivalue;
  int zadap=0,zadjs=0,zmon=0;
  switch(zflag){
    case 0:{ //brake free
      zvalue=zintg=0;
      if(wrps>PRM_ReadData100x(13)){
        zflag=1;
        bmin=1.0e6;
      }
      else if(tmsec>20 && sigv<0){
        zflag=2;
      }
      break;
    }
    case 1:{ //overrun brake
      zvalue=PRM_ReadData100x(12);
      if(tmsec>PRM_ReadData(14)){
        zflag=2;
      }
      if(dbdt<0){
        if(bmin>bh) bmin=bh;
      }
      else if(bh-bmin>PRM_ReadData100x(15)){
        zflag=2;
      }
      break;
    }
    case 2:{ //run level change(acceleration finished)
      zflag=3;
      dcore::shift();  //RunLevel =>4
    }
    case 3:
       if(tmsec>PRM_ReadData(21)){   //I-control delay
         dcore::shift();  //RunLevel =>4
         zflag=4;
       }
       ssdt=0;
    case 4:{
      auto li=100000-PRM_ReadData1000x(20);   //I-control higher limit %
      zintg+=(li-zintg)*ssdt/1000*PRM_ReadData(22)/10000;  //I-control update
      zadap=zintg/1000;
      zadjs=(sbh2-PRM_ReadData100x(8))*PRM_ReadData(18)/1000; //P-control
      if(zadjs<0 || tmsec-sst2>PRM_ReadData(17)) zadjs=0;  //P-control update timeout
      zmon=MAX(zadap,zadjs);
      int zlow=PRM_ReadData(11);
      if(zvalue>zlow){
        auto vi=zvalue-zvalue*zadap/100;   //I-control
        auto vp=zvalue-zadjs;                //P-control
        auto lp=zvalue*PRM_ReadData(16)/100;   //P-control lower limit
        if(vp<lp) vp=lp;
        zvalue=MIN(vi,vp);
        if(zvalue<zlow) zvalue=zlow;
        if(sigv>0) zvalue=zlow;
      }
      break;
    }
  }

//logger
  switch(PRM_ReadData(3)){
  case 0:
    logger::stage.eval=MIN(255,MAX(0,zmon));
    break;
  case 1:
    logger::stage.eval=MIN(255,MAX(0,zadap));
    break;
  case 2:
    logger::stage.eval=MIN(255,MAX(0,zadjs));
    break;
  case 4:
    logger::stage.eval=dcore::tpwm;
    break;
  }

  return zvalue;
}
