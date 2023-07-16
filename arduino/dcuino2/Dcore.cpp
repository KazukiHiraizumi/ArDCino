#include "Arduino.h"
#include <mbed.h>
#include <rtos.h>
#include <SetTimeout.h>
#include "Dcore.h"
#include "Logger.h"
#include "NRF52_MBED_TimerInterrupt.h"

static NRF52_MBED_Timer ITimer0(NRF_TIMER_3);   //PWM
static NRF52_MBED_Timer ITimer1(NRF_TIMER_4);   //Debouncer and PWM blocker

#define T_PWM_MIN 100 //usec
#define T_WAIT 200 //usec

#define LENGTH(x) (sizeof(x))/(sizeof(x[0]))

namespace dcore{//DC core
  uint8_t RunLevel,tpwm;
  static StartCB_t start_callback;
  static ContCB_t cont_callback;
  static EndCB_t end_callback;
  static void init();
}

namespace pwm{//methods for gate on(turn pwm pulse high)
  static uint8_t DO;//digital out pin
  static uint16_t Count; //Switch count
  static uint16_t Interval; //Switch interval
  static uint16_t Treq; //Turn on time cmd
  static uint16_t Tact; //actual turn on time sum
  static uint16_t Ton; //latch for Tact
//  static uint16_t Tpwm[]={1000000/1046,1000000/880,1000000/698,1000000/587};//1046Hz=C5
  static uint16_t Tpwm[]={1000000/1174,1000000/880};//1174Hz=D5,880Hz=A4
  static uint8_t Duty; //duty command
  static bool Block;
  void on(); //to turn gate on
  void off(); //to turn gate off
  static void intr_on(); //interrupt callback to turn pulse high
  static void intr_off(); //interrupt callback to turn pulse low
  void stop(); //stop pwm sequence
  void start();
  void start1(uint32_t t_rev);
  void startN(uint32_t t_rev);
}

namespace sens{
  static rtos::Thread thread(osPriorityHigh);  //rotation sensor
  static rtos::Semaphore sema;
  static long wdt;
  static volatile int32_t Tm;
  static volatile uint32_t Interval;
  static uint8_t Nint;
  static mbed::InterruptIn* irq;
  static uint8_t DI;
//methods for sensing(pin interrupt)
  static void intr();
  static void start();
  static void stop();
  static void task();
}

namespace debouncer{
  static uint16_t Tcmd;  //Duty command
  static uint16_t Ton; //Turn-on time
}

namespace pwm{//methods for pwm
  void init(){
    off();
    Ton=Tact=Treq=Count=Interval=Duty=0;
    Block=true;
  }
  void on(){ digitalWrite(DO,HIGH);}
  void off(){ digitalWrite(DO,LOW);}
  static void intr_off(){
    off();
    ITimer1.setFrequency(1,[](){});
    Tact+=Treq;
    if(Count==0){
      Block=true;
      Ton=Tact;
      Tact=0;
      sens::start();
    }
  }
  static void intr_off2(){
    off();
    ITimer1.setFrequency(1,[](){});
    Block=true;
    Ton=Tact+=Treq;
    Tact=0;
    sens::start();
  }
  static void intr_on(){
    switch(Count){
      case 0: return;
      case 101: ITimer0.setInterval(Interval,intr_on);
    }
    Count--;
    if(!Block){
      int32_t tnow=micros();
      int32_t tnex=sens::Tm+sens::Interval-T_WAIT;
      Treq=(long)Interval*Duty>>8;
      if(tnow+Treq-tnex>0){
        int dt=tnex-tnow;
        Treq=MAX(0,dt);
      }
      if(tnow+Interval-tnex>0){
        if(Treq>T_PWM_MIN){
          on();
          ITimer1.setInterval(Treq,intr_off2);
        }
        else{
          ITimer1.setFrequency(1,[](){});
          Block=true;
          Ton=Tact;
          Tact=0;
          sens::start();
        }
      }
      else{
        if(Treq<T_PWM_MIN){
          Treq=T_PWM_MIN;
        }
        on();
        ITimer1.setInterval(Treq,intr_off);
      }
    }
  }
  void stop(){ Count=0;}
  void start(){
    ITimer0.setInterval(Interval,intr_on);
    if(Duty>0){
      on();
      Treq=(long)Interval*Duty>>8;
      if(Treq<T_PWM_MIN) Treq=T_PWM_MIN;
      ITimer1.setInterval(Treq,intr_off);
    }
    else Treq=0;
  }
  void start1(uint32_t trev){
    uint16_t pcnt=trev/Tpwm[0];
    pcnt|=1; //division should be odd number
    Interval=trev/pcnt;
    start();
    Count=pcnt-1;
    Block=false;
    dcore::tpwm=Interval/10;
  }
  void startN(uint32_t trev){
    struct PWMNR{
      int16_t npwm;
      uint16_t tpwm;
    } ts[sizeof(Tpwm)/sizeof(Tpwm[0])];

    for(int i=0;i<sizeof(Tpwm)/sizeof(Tpwm[0]);i++){
      ts[i].npwm=(int)(trev*100/Tpwm[i])%200-100;
      ts[i].tpwm=Tpwm[i];
    }

    if(abs(ts[0].npwm)>50){
      qsort(ts,sizeof(ts)/sizeof(ts[0]),sizeof(ts[0]),[](const void *ts1, const void * ts2){
        int n1=abs(((PWMNR *)ts1)->npwm);
        int n2=abs(((PWMNR *)ts2)->npwm);
        if(n1>n2) return 1;
        else if(n1<n2) return -1;
        else return 0;
      });
    }

    int tw=ts[0].tpwm;
    if(Count==0){
      Interval=tw;
      start();
      Count=100;
    }
    else if(Interval!=tw){
      Interval=tw;
      Count=101;
    }
    else Count=99;
    Block=false;
    dcore::tpwm=Interval/10;
  }
}

namespace debouncer{
  void init(){ Tcmd=Ton=0;}
  void intr_ready(){
    sens::start();
    ITimer0.setFrequency(1,[](){});
  }
  void intr_off(){
    pwm::off();
    ITimer1.setFrequency(1,[](){});
  }
  void start(uint16_t dtn,uint16_t dto){
    if(Tcmd==0 || dto==0){
      ITimer0.setInterval(dtn>>1,intr_ready);
      ITimer1.setFrequency(1,[](){});
      Ton=0;
    }
    else{
      int16_t dtx=(uint32_t)dtn*dtn/dto;
      if(dtx>dtn) dtx=dtn;
      if(dtx>500){
        uint16_t tmax=dtx-200;
        ITimer0.setInterval(tmax,intr_ready);
        Ton= Tcmd<(tmax-=50)? Tcmd:tmax;
        pwm::on();
        ITimer1.setInterval(Ton,intr_off);
      }
      else{
        ITimer0.setInterval(dtn>>1,intr_ready);
        ITimer1.setFrequency(1,[](){});
        Ton=0;
      }
    }
  }
}

//utility
static int16_t log_lat;
static void log_pre();
static void log_post();

namespace sens{
  static uint8_t Nacc;
  void init(){ wdt=Interval=Nacc=0;}
  void stop(){
    irq->rise(NULL);
  }
  void start(){
    irq->rise(mbed::callback((voidFuncPtrParam)intr, (void *)NULL));
  }
  void intr(){
    stop();
    long tnow=micros();
    long dt=tnow-Tm;
    Tm=tnow;
    switch(dcore::RunLevel){
    case 0:
      debouncer::start(Interval=dt,0);
//      if(dt>20000 || dt>Interval){
      if(dt>20000){
        Nacc=0;
        break;
      }
      if(Nacc<10) Nacc++;
      if(dt<(int)3600*Nacc-5200){
        logger::start();
        dcore::RunLevel=1;
        sema.release();
      }
      break;
    case 1:
      debouncer::start(Interval=dt,0);
      dcore::RunLevel=2;
      break;
    case 2:
      debouncer::start(Interval+=dt,0);
      dcore::RunLevel=3;
      sema.release();
      break;
    case 3:
      logger::latch();
      pwm::Ton=debouncer::Ton;
      debouncer::start(dt,Interval);
      Interval=dt;
      sema.release();
      break;
    case 4:
      dcore::RunLevel=5;
    case 5:
      logger::latch();
      Interval=dt;
      pwm::start1(dt);
      sema.release();
      break;
    case 6:
      logger::latch();
      Interval=dt;
      pwm::startN(dt);
      sema.release();
      break;
    }
  }
  void task(){
  WAIT:
    sema.acquire();
    switch(dcore::RunLevel){
    case 1:
      Serial.print("RunLevel 1 / ");
      Serial.print(Interval);
      Serial.print(" / ");
      Serial.println(Nacc);
      wdt=setTimeout.set([](){
        Serial.println("sens wdt:RunLevel 1=>0");
        dcore::init();
      },30);
      (*dcore::start_callback)();
      break;
    case 3:
      log_pre();
      debouncer::Tcmd=(*dcore::cont_callback)(Interval,pwm::Ton);
      log_post();
      if(dcore::RunLevel>3){
        Serial.print("Runlevel 3=>");
        Serial.println(dcore::RunLevel);
      }
      setTimeout.clear(wdt);
      wdt=setTimeout.set([](){
        Serial.println("sens wdt:RunLevel 3=>0");
        (*dcore::end_callback)();
        dcore::init();
      },30);
      break;
    case 5:
    case 6:
      log_pre();
      pwm::Duty=(*dcore::cont_callback)(Interval,pwm::Ton);
      log_post();
      if(wdt!=0) setTimeout.clear(wdt);
      wdt=setTimeout.set([](){
        Serial.println("sens wdt:RunLevel 6=>0");
        (*dcore::end_callback)();
        pwm::Duty=128;
        stop();
//        pwm::stop();
//        pwm::start(1000000L);  //dcore::init will be invoked after this pwm sequence
        setTimeout.set(dcore::init,1000);
      },100);
      break;
    }
    goto WAIT;
  }
}

namespace dcore{
  void init(){
    pwm::init();
    sens::init();
    debouncer::init();
    sens::start();
    RunLevel=0;
    ITimer0.setFrequency(1,[](){ sens::Nacc=0;});
    ITimer1.setFrequency(1,[](){});
  }
  void shift(){
    switch(RunLevel){
    case 3:
      RunLevel=4;
      break;
    case 5:
      RunLevel=6;
      break;
    }
  }
  void run(int DI,int DO,StartCB_t cb_start,ContCB_t cb_cont,EndCB_t cb_end){
    start_callback=cb_start;
    cont_callback=cb_cont;
    end_callback=cb_end;
    sens::DI=DI;
    pwm::DO=DO;
    sens::Nint=digitalPinToInterrupt(sens::DI);
    sens::irq = new mbed::InterruptIn(digitalPinToPinName(sens::Nint));
    sens::thread.start(mbed::callback(sens::task));
    pwm::off();
    init();
    attachInterrupt(sens::Nint, sens::intr, RISING);
    pinMode(sens::DI,INPUT); //disable pullup after attachInterrupt
    
  }
  void sleep(uint16_t ms){
    long t=millis();
    do{
      ::sleep();
      // __WFI();
    }while((long)millis()-t<ms);
  }
}

void log_pre(){
  log_lat=micros();
  logger::stage.latency=log_lat-sens::Tm;
  logger::stage.interval=sens::Interval;
  logger::stage.mode=dcore::RunLevel;
  logger::stage.duty=((uint32_t)pwm::Ton<<8)/sens::Interval;
}
void log_post(){
  logger::stage.cmd=pwm::Duty;
}
