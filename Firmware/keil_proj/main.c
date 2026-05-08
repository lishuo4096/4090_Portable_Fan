#include "STC8H.H"
#include "main.h"

volatile bit poweron = 0;
volatile bit System_OK = 0;
volatile bit Timer0_Sync = 1;
volatile bit Timer1_Sync = 1;



void Delay_ms(uint16_t ms){//24MHZ
    //看门狗定时器无法软件关闭，因此循环时间不要超过看门狗复位时间
    uint8_t data i, j;
    while(--ms){
        _nop_();
        i = 32;
        j = 40;
        do{
            while (--j);
        } while (--i);
    }
}

void main(){
    uint8_t i = 0;//k = 10000U;
    Interupt_Init();//打开总中断和串口中断
    Timer_Init();
    RC_Wakeup_Init();//这个不需要初始化了，对于触摸按键来说，如果同时使用定时唤醒，会导致无限唤醒，无法进入睡眠
    ADC_Init();
    GPIO_Init();
    UART1_Init();
    PWMA_Init();
    TouchKey_Init();
    WS2812_Init(13);
    PWR_CTRL(PWR_ON);
    Serial_Printstrln("Initialize OK,Now Shutdown");
    Serial_Printstr("Touchkey Threshold is:");
    Serial_Printnumln(TK1_Threshold);
    while(1){
        if(!poweron){
            Enter_Sleep();
        }
        else if(!System_OK){//开机后只执行一次
            PWR_CTRL(PWR_ON);
        }
        if(Timer0_Sync){//30ms中断时序
            Timer0_Sync = 0;
            System_OK = 1;//这里变1之后上面的电源开启操作就只执行一次
            VBAT_CAL();
            Key_Sampling();
            Key_Handler();
            Adjust_Duty();
            if(++i >= 33){//1s回报一次参数
                if(Charging){
                    Serial_Printstrln("Now Charging...");
                }
                Serial_Printstr("Current Vbat:");
                Serial_Printnumln(vbat);
                Serial_Printstr("Current PWM Level:");
                Serial_Printnumln(Duty_Level);
                Serial_Printstr("Current PWM Duty:");
                Serial_Printnumln(Duty);
                Serial_Printstr("Current TouchKey Data:");
                Serial_Printnumln(TK_P11_Data);
                Serial_Printstr("Current TouchKey Baseline:");
                Serial_Printnumln(TK1_Baseline);
                i = 0;
            }
        }
        if(Timer1_Sync){//5ms中断时序
            Timer1_Sync = 0;
            WS2812_State_Machine();
            TouchKey_Handler();
        }
        WDT_CONTR |= 0X10;
    }
}
void INT0_ROUTINE(void) interrupt 0 {
    //仅用于休眠下的充电唤醒
}
void TIMER0_ROUTINE(void) interrupt 1 {//定时器0 30ms中断服务函数
    Timer0_Sync = 1;
    If_Back_Sleep();//为定时关机提供时序
}
void TIMER1_ROUTINE(void) interrupt 3 {//定时器5ms中断服务函数
    Timer1_Sync = 1;
}
void Touchkey_ISR(void) interrupt 13{
    //TSSTA2 |= 0X80;
    //这个中断函数仅用于给一个入口，防止程序跑飞
    //B7 TSIF为中断标志，且能唤醒CPU
}
            /*Serial_SendByte('<');
            Serial_Printnum(i);
            Serial_SendByte('>');
            Serial_Printstr("{plotter}");
            Serial_Printnum(TK_P11_Data);
            Serial_SendByte(',');
            Serial_Printnum(TK1_Baseline);
            Serial_SendByte(',');
            Serial_Printint16ln(TK_P11_Pressed * 10000U);*/