#include "STC8H.H"
#include "main.h"
/*
STC8H的触摸按键比较自由，硬件上只提供了触摸参数采样和回报，还有唤醒功能
（说的好听叫自由，说的难听就是简陋）
因此需要自己写一套完善的按键状态机，实现触摸基线校准，状态判断，错误处理等功能
基线校准上没什么好说的初始化时校准一次，之后随时校准
*/
volatile bit TK_P11_Pressed = 0;
volatile uint16_t TK1_Baseline = 0U;
volatile uint16_t TK1_Threshold = 0U;
volatile uint16_t TK_P11_Data = 0;//P33引脚所连接的触摸按键数据

static uint16_t TouchKey_Read();

#define TK_Idle 0
#define TK_Pressed 1
#define TK_Released 2
#define TK_Timeout 3

void TouchKey_Handler(){
    static uint8_t TK_Status = TK_Idle;
    static uint16_t Timeout_Counter = 0;//超时计时器
    static bit Timeout_Flag = 0;
    
    TK_P11_Data = TouchKey_Read();//读取数据
    
    switch(TK_Status){
        default:break;
        case TK_Idle://这个状态下，更新基线
            if(Timeout_Flag){
                Delay_ms(1000);
                Timeout_Flag = 0;
            }
            TK1_Baseline = (((uint32_t)TK1_Baseline * 0x7F) + TK_P11_Data) >> 7;//更新基线//等效于乘以127然后除以128，新数据权重只有1/128
            TK1_Threshold = Touchkey_Threshold_CAL(TK1_Baseline);//由于新数据权重很小，所以不用担心会干扰按键判断
            if(TK_P11_Data < TK1_Threshold) TK_Status = TK_Pressed;
            Timeout_Counter = 0;
            TK_P11_Pressed = 0;
            break;
        case TK_Pressed://这个状态下，检测数据与基线的差距
            if(TK_P11_Data < TK1_Threshold){
                TK_P11_Pressed = 1;
                if(++Timeout_Counter >= 3000U){//计时15s，超时则退出
                    Timeout_Counter = 0;
                    TK_Status = TK_Timeout;
                }
            }
            else{
                TK_Status = TK_Released;
            }
            break;
        case TK_Released:
            if(TK_P11_Data > TK1_Threshold){
                TK_P11_Pressed = 0;
                TK_Status = TK_Idle;
            }
            else{
                TK_Status = TK_Pressed;
            }
            break;
        case TK_Timeout:
            TK_P11_Pressed = 0;
            Timeout_Counter = 0;Timeout_Flag = 1;//只有空闲状态下能更新基线
            TK1_Baseline = TK_P11_Data;//强制将基线降低，防止再次进入pressed状态
            TK_Status = TK_Idle;
            Serial_Printstrln("TK errror:Timeout");
            break;
    }
}

uint16_t TouchKey_Read(){
    uint16_t TK_Data_Temp = 0;//按键数据缓存
    static uint16_t TK_Data_Restore = 30000;//存储上一次采样的数据，避免这次采样出错，导致判断出错
    if(TSSTA2 & 0X80){//B7 TSIF = 1;
        TK_Data_Temp = (TSDATH << 8) | TSDATL;
    }
    else {
        Serial_Printstrln("TK sampling interval too short");//向串口报错
        return TK_Data_Restore;//返回上一个值
    }
    if(TK_Data_Temp > 40000U){
        Serial_Printstrln("TK result invalid");//向串口报错
        return TK_Data_Restore;
    }
    if(TSSTA2 & 0X40){//溢出标志
        TSSTA2 |= 0X40;//写1清零
        Serial_Printstrln("TK result overflow");
        return TK_Data_Restore;
    }
    TSSTA2 |= 0X80;//写1清零//清零后触摸按键会立即扫描下一个按键，所以需要先读取，再清零
    if((TSSTA2 & 0X0F) == 1){
        TK_Data_Restore = TK_Data_Temp;
        return TK_Data_Temp;
    }
    else return TK_Data_Restore;
    //仅接受TK1的数据，不过扫描其实也只扫描TK1
}
uint16_t Touchkey_Threshold_CAL(uint16_t Data){//在这里设置触摸唤醒的阈值
    return ((uint32_t)Data * 970UL) / 1000UL;
}
void TK_Write_Th(uint16_t Baseline){
    bit Working = 0;
    if(TSCTRL & 0X80){//if tsgo = 1
        TSCTRL &= ~0X80;//TSGO = 0;
        Working = 1;
    }
    
    TK1_Threshold = Touchkey_Threshold_CAL(Baseline);
    TSTH01H = TK1_Threshold >> 8;//填入高八位
    TSTH01L = TK1_Threshold & 0xFF;//仅在TSGO为0时可写
    
    if(Working){
        TSCTRL |= 0X80;//写完阈值寄存器之后，恢复正常工作
    }
}
uint8_t WKUP_Handler(){
    uint8_t i = 0;
    TK_P11_Data = (TSDATH << 8) | TSDATL;//读取数据//这个数据是休眠之前的数据
    if(TSSTA2 & 0X80){//检测是否产生了中断，如果有中断，则说明是被触摸按键唤醒的
        TSSTA2 |= 0X80;//清除中断标志，然后在循环中检测按键
        Serial_Printstrln("Wakeup By Touchkey");
        return 0;
    }
    else if(Charging){//如果不是由触摸按键唤醒的，那就是被充电唤醒或者定时唤醒的，检测是否由充电唤醒
        Serial_Printstrln("Wakeup By Charging");
        return 1;
    }
    else{
        Serial_Printstrln("Wakeup By IRC32kTimer");
        for(i = 0; i < 100; i++){
            TK1_Baseline = (((uint32_t)TK1_Baseline * 0x7F) + TK_P11_Data) >> 7;//模拟1.8秒唤醒时间内执行360次，同步基线
        }//执行这段循环只需要1ms
        return 2;//返回main函数，重新休眠
    }
}
/*static void Touchkey_Slope_CAL(uint16_t TK_Data){
    static int16_t Data0 = 0,Data1 = 0;
    if((Data0 == 0) && (Data1 == 0)){
        Data0 = TK_Data;//初始化
        Data1 = TK_Data;
    }
    else{
        Data0  = TK_Data;//Data0为新数据
        TK_P11_Slope = (Data0 - Data1 + 1)/2;
        Data1 = Data0;//记录旧数据
    }
    //if(TK_P11_Slope < -50) TK_P11_Status = 0;
    //if(TK_P11_Slope > 50) TK_P11_Status =  1;
}*/

/*static void TouchKey_Judge(){
    TK_P11_Data = TouchKey_Read();
    if(TK_P11_Data > TK1_Baseline){
        TK_P11_Status = 0;//模拟按键的按下拉低的效果
    }
    else if(TK_P11_Data < (TK1_Baseline - 1000U)){
        TK_P11_Status = 1;//加迟滞
    }
}*/
/*void Touchkey_Slope_CAL(uint16_t TK_Data){//传入触摸按键数据
    static bit Buffer_Full = 0;//是否已经填满了两个缓冲区标志，填满后才会输出边缘沿数据
    static bit Buffer_Exchange = 0;//交换缓冲区的新旧关系标志
    static uint8_t xdata i = 0,j = 0;
    static uint16_t xdata TK_Data_Buffer0[5] = {0};//采样数据缓冲区0
    static uint16_t xdata TK_Data_Buffer1[5] = {0};//采样数据缓冲区1
    int16_t sum0 = 0,sum1 = 0;
    if(!Buffer_Full){
        if(i < 5){//执行0~4，总共5次
            TK_Data_Buffer0[i] = TK_Data;//向buffer0填入数据
            i++;
        }
        else if(j < 5){
            TK_Data_Buffer1[j] = TK_Data;//向buffe1填入数据
            j++;
            if((i == 5) && (j == 5)){
                Buffer_Full = 1;//buffer0和1全部填满之后就不再进入此程序段
                i = 0;j = 0;
            }
        }
        return;//buffer填满之前退出，不给出斜率值
    }
    else{
        for(i = 0;i < 5; i++){//读取0~4共五位数据相加求和(实测只要不是没按天线，否则触摸数据必然小于10000，因此不会溢出
            sum0 = sum0 + TK_Data_Buffer0[i];
        }
        for(j = 0; j < 5; j++){//读取0~4共五位数据相加求和
            sum1 = sum1 + TK_Data_Buffer1[j];
        }
        i = 0; j = 0;
        if(!Buffer_Exchange){//在填满缓冲区之后还会再次采样一次，也就是在第11次采样才给出边缘沿斜率
            TK_P11_Slope = ((sum1 - sum0) + 2)/5;//这个数据是缓冲区新旧交替之前的数据，认为SUM1是新数据，sum0是旧数据，乘了10倍
        }
        else{
            TK_P11_Slope = ((sum0 - sum1) + 2)/5;//这个数据是缓冲区新旧交替之前的数据，认为SUM1是新数据，sum0是旧数据
            //Slope = (100UL * (sum0 - sum1) + 25U)/50U;//这个数据是缓冲区新旧交替之前的数据，认为SUM1是新数据，sum0是旧数据
        }
        
        if(i < 5){//执行0~4，总共5次
            TK_Data_Buffer0[i] = TK_Data;//向buffer0填入数据,buffer0为新数据
            i++;
            Buffer_Exchange = 1;
        }
        else if(j < 5){
            TK_Data_Buffer1[j] = TK_Data;//向buffe1填入数据,buffer1为新数据
            j++;
            Buffer_Exchange = 0;
            if((i == 5) && (j == 5)){
                i = 0;j = 0;//重新开始循环
            }
        }
    }
}*/
/*void Touchkey_Status_Judege(){//5ms时序执行，通过触摸按键回报的数据斜率判断，不能太快，不然读取函数因为没采样完而返回65535
    static uint8_t i = 0;
    static uint16_t xdata TK_Data_Buffer[6] = {0};//统计6次数据，然后求斜率
    int16_t xdata Slope = 0;//斜率
    if(!(i >= 6)){//如果没有计数6次
        TK_P11_Data = TouchKey_Read();
        TK_Data_Buffer[i] = TK_P11_Data;
        i++;
    }
    else{//30ms内的斜率
        i = 0;
        Slope = (((uint32_t)TK_Data_Buffer[0] + TK_Data_Buffer[1] + TK_Data_Buffer[2]) - ((uint32_t)TK_Data_Buffer[3] + TK_Data_Buffer[4] + TK_Data_Buffer[5]))/30UL;
        Serial_Printint16ln(Slope);
    }
}*/
/*void TouchKey_Threshold_Init(){//这里不能用阻断循环，开机后主循环执行触摸按键状态机就可以了
    uint8_t i = 0;
    if(Timer0_Sync){
        Timer0_Sync = 0;
        if(++i >= 5) break;//计时超过150ms之后退出
    }
    TK_P11_Data = TouchKey_Read();//读取按键数据
    //TSTH01H = TK1_Threshold >> 8;//填入阈值
    //TSTH01L = TK1_Threshold;
}*/
/*
这里的两个缓冲区的先后关系，会随着采样进行不断交换。
填满第一个缓冲区，那就换另一个缓冲区，另一个也填满了，那就换回第一个。

*/
/*
if(TK1_Baseline == 0U) TK1_Baseline = TK_P11_Data;//初始化基线
            else if(!((TK_P11_Data < (TK1_Baseline - 100)))){//如果数据范围合理//只单向判断是不是否太小
                TK1_Baseline = (((uint32_t)TK1_Baseline * 0x7F) + TK_P11_Data) >> 7;//更新基线//等效于乘以127然后除以128，新数据权重只有1/128
            }
*/
    //Touchkey_Slope_CAL(TK_P11_Data);//计算斜率
    
    /*if(!(TK_P11_Data < TK1_Threshold)){
        if(TK_P11_Slope < -500) TK_Status = TK_Pressed;//判断边缘沿
        else if(TK_P11_Slope > 500) TK_Status = TK_Released;
    }*/
//实际测试发现，小于-50的斜率，相当于按键按下，大于50的斜率相当于抬起
            /*if(!((TK_P11_Data < (TK1_Baseline - 500)))){//如果数据范围合理//只单向判断是不是否太小
                TK1_Baseline = (((uint32_t)TK1_Baseline * 0x7F) + TK_P11_Data) >> 7;//更新基线//等效于乘以127然后除以128，新数据权重只有1/128
                TK1_Threshold = Touchkey_Threshold_CAL(TK1_Baseline);
            }*/