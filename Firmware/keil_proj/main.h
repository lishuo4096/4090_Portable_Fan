#ifndef __MAIN_H__
#define __MAIN_H__
#include "STC8H.H"

#define Main_FOSC 24000000UL//24MHZ
#define Baud_Rate 1000000UL
#define UART_Timer_Reload_Value (65536UL - (Main_FOSC / (4UL * Baud_Rate)))
#define SAVE_SET_ADDR 0x0000;//高级定时器触发值
#define PWM_Period 1000U//高级定时器周期值

void Delay_ms(uint16_t ms);

void GPIO_Init();
void Interupt_Init();
void ADC_Init(void);
void PWMA_Init();
void PWMA_Config();
void Timer_Init(void);
void UART1_Init(void);
void TouchKey_Init();
void RC_Wakeup_Init();

extern volatile bit UART_Buffer_Overflow;
extern volatile bit poweron;
extern volatile bit System_OK;
extern volatile bit Timer0_Sync;
extern volatile bit Timer1_Sync;
extern volatile uint16_t vbat;
extern volatile bit was_enter_sleep;//
extern volatile bit need_wakeup_sampling;//被外部中断唤醒，需要进行一次采样

#define MP3431_EN_PIN P10
#define FAN_PWR_ON (P3PU |= 0X10)//打开上拉
#define FAN_PWR_OFF (P3PU &= ~0X10)//关闭上拉
#define TK_PIN P11
#define WS2812_SPI_PIN P13
#define WS2812_PWR_ON do {P16 = 1; P17 = 1;} while(0)
#define WS2812_PWR_OFF do {P16 = 0; P17 = 0;} while(0)
#define PWM_PIN P33
#define RXD_PIN P30
#define TXD_PIN P31
#define Charging (!P32)//P32为低电平
#define Touchkey_Normal_Set do {IE2 &= ~0X80; TSCTRL &= ~0X80; TSCTRL = 0X25; TSCTRL |= 0X80;} while(0)//正常工作时TSCTRL寄存器为0xA5，睡眠状态为0x0D
#define Touchkey_Sleep_Set do {IE2 |= 0X80; TSSTA2 = 0X80; TSCTRL &= ~0X80; TSCTRL = 0X0D;} while(0)//都需要先将B7位清零，才能写入

void VBAT_CAL(void);

#define Brt_Rise 0U
#define Brt_Keep_High 1U
#define Brt_Fall 2U
#define Brt_Keep_Low 3U
extern volatile uint8_t Breath_State;//呼吸模式
extern volatile uint16_t Breath_Counter;//呼吸计数器，实现呼吸计数
void WS2812_State_Machine();
void WS2812_Init(unsigned char Pin_Sel);
void WS2812_Send_Data(unsigned char WS2812_Num,unsigned char Red,unsigned char Green,unsigned char Blue);

extern volatile uint16_t TK1_Baseline;
#define Total_TK_Num 1U
uint16_t TouchKey_Read();
extern volatile bit TK_P11_Pressed;
extern volatile uint16_t TK_P11_Data;
extern volatile uint16_t TK1_Threshold;
extern uint16_t Touchkey_Threshold_CAL(uint16_t Data);
void TK_Write_Th(uint16_t Th);
extern uint8_t WKUP_Handler();
void TouchKey_Handler();

extern volatile uint8_t BAT_Status;
#define BAT_FULL     0  // ≥4000 mV
#define BAT_HIGH     1  // ≥3800 mV
#define BAT_MEDIUM   2  // ≥3600 mV
#define BAT_LOW      3  // ≥3400 mV
#define BAT_CRITICAL 4  // ≥3000 mV
#define BAT_DRAINED  5  // <3000 mV
#define BAT_DEAD     6  // <2750 mv

void Serial_Printstr(uint8_t *str);
void Serial_Printstrln(uint8_t *str);
void Serial_SendByte(uint8_t dat);
void Serial_Printnumln_with_Word(uint8_t *str,uint16_t num);
void Serial_Printnum(uint16_t num);
void Serial_Printnumln(uint16_t num);
void Serial_Printint16ln(int16_t num);

extern volatile uint16_t Duty;
extern volatile bit Fine_Tune_Inc;   // 正在微调增加占空比
extern volatile bit Fine_Tune_Dec;   // 正在微调减小占空比
extern volatile uint8_t Duty_Level;//当前风速

extern volatile const uint16_t Duty_Table[11];
void Key_Sampling(void);
void Key_Handler(void);
void Adjust_Duty(void);
#define Duty_Reg PWMA_CCR4

#define Save_Addr 0x100
#define Save_VBAT_Addr 0x0100
#define Save_Dutylevel_Addr 0x102
void Save_Set_To_EEPROM(uint16_t addr,uint16_t Set1,uint16_t Set2);
uint16_t Read_Set_From_EEPROM(uint16_t addr);

#define PWR_ON 1
#define PWR_OFF 0
void PWR_CTRL(bit CTRL);

void If_Back_Sleep(void);
void Enter_Sleep(void);
extern volatile bit Back_Sleep;

#define MDU_CMD_NOP           0   // 无动作，待机
#define MDU_CMD_MUL32         2   // 32位乘法 (耗时3时钟)
#define MDU_CMD_DIVU32        4   // 无符号32位除法 (耗时19时钟)
#define MDU_CMD_DIVS32        6   // 有符号32位除法 (耗时21时钟)
#define MDU_CMD_NORM          8   // 数据规格化 (Normalization)
#define MDU_CMD_LOGIC_SHIFT   10  // 逻辑左移/右移 (Logical Shift)
#define MDU_CMD_ARITH_SHIFT   11  // 算术右移 (Arithmetic Shift)
#define MDU_CMD_ADD32         12  // 32位加法
#define MDU_CMD_SUB32         14  // 32位减法

#endif
