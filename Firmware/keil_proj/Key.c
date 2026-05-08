#include "STC8H.H"
#include "main.h"

/*
tip:
这个代码用了大量32位运算，导致程序大小相当臃肿。
如果程序空间不足的话，最好将代码改为用多层状态机的按键代码，可以至少省出1500字节
*/
#define Button (!TK_P11_Pressed)//按键输入P32
#define Duty_Reg PWMA_CCR4//占空比寄存器，负逻辑，越小，占空比越高
#define NOKEY 0
#define SINGLEKEY 1
#define DOUBLEKEY 2
#define TRIPLEKEY 3
#define QUADRAKEY 4
#define LONGKEY 5
volatile uint16_t Duty = 100;
volatile bit Fine_Tune_Inc = 0;   // 正在微调增加占空比
volatile bit Fine_Tune_Dec = 0;   // 正在微调减小占空比
volatile uint8_t Duty_Level = 1;//当前风速
volatile uint8_t Key_Event = NOKEY;
volatile const uint16_t Duty_Table[11] = {//10挡调节,0档不算
    0, 85, 130, 203, 279, 365, 460, 596, 685, 775, 1000
};
static uint8_t Rise_Edge_Counter(unsigned long Input){
    uint32_t Mask;//掩码
    //if(Input == 0xffffffff) return 0xff;//如果程序上出现了误采样的话，防止错误给出0上升沿导致误判关机(已解决，不需要了)
    Mask = ~Input & (Input << 1);
    Mask = (Mask & 0x55555555) + ((Mask >> 1) & 0x55555555);
    Mask = (Mask & 0x33333333) + ((Mask >> 2) & 0x33333333);
    Mask = (Mask + (Mask >> 4)) & 0x0F0F0F0F;
    Mask = Mask + (Mask >> 8);
    Mask = Mask + (Mask >> 16);
    Mask = Mask & 0x0000003F;
    return Mask;
}
void Key_Sampling(void){
    //本函数自带消抖，因为不足一个定时器间隔的下降沿无法触发采样
    //在30ms定时器中断中调用本函数
    static  uint8_t Sampling_Counter = 0;//采样次数计数器
    static  uint32_t Sampling_Temp = 0;//采样结果缓存
    static bit Previous_Key = false;//之前的按键采样记录
    static bit Now_Key = false;//当前的按键采样记录
    static bit Is_Sampling = false;//正在采样标志
    static bit Start_Sampling = false;//开始采样标志，由下降沿触发
    static  uint8_t Key_Release_Counter = 0;//超时计数器，可以更快的响应单击和双击操作
    static uint8_t LK_Counter = 0;//长按计时
    uint8_t Rise_Edge_Result = 0;//上升沿计算的结果,非static变量，重入自动清零
    bit Read_Temp = Button;//按键状态缓存，每次进入函数时读取
    //采样触发(检测下降沿)
    if(!Is_Sampling){//如果没有在采样，才做判断
    Previous_Key = Now_Key;//存储上一次的结果
    Now_Key = Read_Temp;//读取本次结果
  //Key_Event = NOKEY;//未在采样状态，全局按键事件为无键
    if((!Now_Key && Previous_Key)){//上一次为1，这一次为0，是下降沿
        Start_Sampling = true;LK_Counter = 0;//开始采样
    }
    else{
        Sampling_Counter = 0;Sampling_Temp = 0;Key_Release_Counter = 0;
      return;}//未触发采样，清空变量，返回
    }
    //开始采样与结果处理
    if(Start_Sampling){
        if(Sampling_Counter < 32){
            Is_Sampling = true;
            Sampling_Temp <<= 1;//整体左移一位,低位补0
            if (Read_Temp){//采样引脚，如果为高电平，最低位置1
            Sampling_Temp |= 0x01;//如果引脚为高才计1，如果为低就继续左移一位，也就是补0
            Key_Release_Counter++;
                if(Key_Release_Counter >= 8){//超过160ms没有按键输入
                    Key_Release_Counter = 0;
                    //采样超时，提前结束，未采样的部分全部补1
                    Sampling_Temp = (Sampling_Temp << (31-Sampling_Counter)) | ((1UL << (31-Sampling_Counter)) - 1);
                    Sampling_Counter = 31;//标记为采样结束
               }
           }
           else{//如果按键为低电平
                Key_Release_Counter = 0;
            }
            Sampling_Counter++;
        }
        else{//采满32次，采样结束,开始计算
            Is_Sampling = 0;
            Start_Sampling = 0;
            Sampling_Counter = 0;//清空计数器
            Rise_Edge_Result = Rise_Edge_Counter(Sampling_Temp);//计算上升沿数量
            switch(Rise_Edge_Result){
              case 0: if(++LK_Counter >= 3){//连续采样3次
                         Key_Event = LONGKEY;
                     }
                     else{
                         Is_Sampling = 1;
                         Start_Sampling = 1;
                     }
                  break;
              case 1: Key_Event = SINGLEKEY;break;
              case 2: Key_Event = DOUBLEKEY;break;
              case 3: Key_Event = TRIPLEKEY;break;
            }
        }
    }
}
#define LONGKEY_TIME 60 //在32x30ms的采样之后，继续等待60x30ms
void Key_Handler(void){
    static bit Is_Max_Speed = 0;    //当前是否处于“三击最高速”模式
    static unsigned char Saved_Duty_Level = 0; // 用于三击前保存原有档位
    
    //如果当前处于三击最大速模式，且出现了单击或双击，先恢复到设置前的档位
    if (Is_Max_Speed && (Key_Event == SINGLEKEY || Key_Event == DOUBLEKEY)) {
        Duty_Level = Saved_Duty_Level;
        Is_Max_Speed = 0;
        Duty = Duty_Table[Duty_Level]; //同步原有占空比
    }
    switch(Key_Event){
        default:break;
        case NOKEY:break;
        case LONGKEY:
            poweron ^= 1;
            break;
        case SINGLEKEY:
            if(!Button && poweron){ //单击后按键未抬起
                Fine_Tune_Inc = 1; //启动微调增加模式
            }
            else{//正常单击，增加档位
                Duty_Level = (Duty_Level % 10) + 1; // 0~7循环
                Duty = Duty_Table[Duty_Level];
            }
            break;
        case DOUBLEKEY:
            if(!Button && poweron){ //双击后按键未抬起
                Fine_Tune_Dec = 1;//启动微调减小模式
            }
            else{
                if(Duty_Level == 1){//1~10循环
                    Duty_Level = 10;
                }
                else{
                    Duty_Level = (Duty_Level - 2 + 10) % 10 + 1;
                    Duty = Duty_Table[Duty_Level];
                }
            }
            break;
        case TRIPLEKEY:
            //三击处理
            if(!Is_Max_Speed && poweron){
                Saved_Duty_Level = Duty_Level; //保存现在的档位
                Duty_Level = 10;                //强制最高档
                Is_Max_Speed = 1;
            }
            else{//再次三击，恢复
                Duty_Level = Saved_Duty_Level;
                Is_Max_Speed = 0;
            }
            Duty = Duty_Table[Duty_Level];
            break;
    }
    //电池即将耗尽时，限制最大风速
    if((BAT_Status == BAT_DRAINED) && (Duty_Level > 8)){
        Duty_Level = 8;
        Duty = Duty_Table[Duty_Level];
    }
    Key_Event = NOKEY; //清空事件
}
void Adjust_Duty(void){
    //如果检测到按键已经抬起，终止微调模式
    if (Button) { 
        Fine_Tune_Inc = 0;
        Fine_Tune_Dec = 0;
    }
    //持续微调 Duty(即使是低电量状态，也可以通过微调来使占空比超过低电量限制)
    if (Fine_Tune_Inc) {
        Duty = (Duty < 1000U) ? Duty + 5 : 1000U;
    } else if (Fine_Tune_Dec) {
        Duty = (Duty > 0) ? Duty - 5 : 0;
    }
    //写入寄存器
    Duty_Reg = (1000U - Duty); 
}