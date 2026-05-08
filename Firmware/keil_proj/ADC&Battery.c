#include "STC8H.H"
#include "main.h"
#define BGV_Channel 0X0F//ADC 15通道带隙电压
static volatile uint16_t BGV = 1100;//防止读取失败，保底值(long变量是因为后面计算乘1000会超过int限制)
volatile uint16_t vbat = 3700;
void BAT_Handler(uint16_t BatVol);
volatile uint8_t BAT_Status = BAT_FULL;//其实这个变量基本没用
static uint16_t ADC_Sampling(unsigned char channel){
    uint16_t A;
    ADC_CONTR = 0X80 | (channel & 0X0F);//切换ADC通道
    ADC_CONTR |= 0x40;       // 启动ADC转换
    _nop_(); _nop_(); _nop_(); _nop_(); // 短暂延时
    while (!(ADC_CONTR & 0x20)); // 等待转换完成
    ADC_CONTR &= ~0x20;      // 清除完成标志
    A = (ADC_RES << 8) | ADC_RESL; // 合并高8位和低8位
    return A;
}
static uint16_t BGV_Read(){
    return *(uint16_t xdata *)0xFDE7;
}
static uint16_t Code_Per_Volt(void){//计算ADC毫伏每码值
    uint16_t A,B;
    A = ADC_Sampling(BGV_Channel);
    if(BGV == 1100){//只读1次
        BGV = BGV_Read();
        Serial_Printstr("BGV Read:");
        Serial_Printnum(BGV);
        Serial_Printstrln("mV");
    }
    //计算BGV/RESULT_BGV即可得到ADC的毫伏每码比值
    if(A > 0){//防止ADC无回报
        B = (1000UL * BGV / A);//乘以1000防止丢失小数部分
    }//bgv为1190左右的值，x1000后超过16位，因此BGV为ULONG
    return B;
}
void VBAT_CAL(void){//计算电池电压，单位毫伏
    //30ms定时器中调用，每300ms更新一次电池电压
    static int8_t k = -1;//初始为位-1，因为if前置自增
    uint8_t i,j;//冒泡排序所需
    uint16_t temp = 0;//冒泡排序缓存
    static uint32_t vbat_Buffer [10] = {0};//32位数组
    uint16_t vbat_raw = ((4096UL * Code_Per_Volt()) / 1000UL);//除以1000
    if(++k >= 10){//前置自增
        k = -1;//t前置自增，t=9的下一次调用为10，数组正好填满0~9,总共10位
        //进行冒泡排序（升序）
        for (j = 0; j < 9; j++) {
            for (i = 0; i < 9 - j; i++) {
                if (vbat_Buffer[i] > vbat_Buffer[i + 1]) {
                    temp = vbat_Buffer[i];
                    vbat_Buffer[i] = vbat_Buffer[i + 1];
                    vbat_Buffer[i + 1] = temp;
                }
        }//32位数组，运算过程中间值也是32位的
        }//取中间四位，求平均
        vbat = (vbat_Buffer[3] + vbat_Buffer[4] +vbat_Buffer[5] + vbat_Buffer[6])/4;//求平均
        //不需要清零buffer，下次采样自动覆写
        BAT_Handler(vbat);//调用比较函数，判断电量
    }
    else{
        vbat_Buffer[k] = vbat_raw;}//将电压存入数组
}

static void BAT_Handler(uint16_t BatVol){//判断电池电量状态，无迟滞
    if(BatVol >= 4000){
        BAT_Status = BAT_FULL;}
    else if(BatVol >= 3800){
        BAT_Status = BAT_HIGH;}
    else if(BatVol >= 3600){
        BAT_Status = BAT_MEDIUM;}
    else if(BatVol >= 3400){
        BAT_Status = BAT_LOW;}
    else if(BatVol >= 3000){
        BAT_Status = BAT_CRITICAL;}
    else if(BatVol >= 2750){
        BAT_Status = BAT_DRAINED;}
    else if(BatVol <2750){
        BAT_Status = BAT_DEAD;
        poweron = 0;//进入睡眠(不需要延时来闪烁了)
    }
}