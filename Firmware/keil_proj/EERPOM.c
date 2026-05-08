#include "STC8H.H"
#include "main.h"

static void IAP_OPERATE(unsigned int addr){//IAP操作集成，每次EEPROM操作都需要调用这个
    IAP_TPS = 0X18;//24MHZ下，24
    IAP_CONTR = 0X80;//使能eeprom操作
    IAP_ADDRL =addr;//取低8位
    IAP_ADDRH =addr >> 8;//取高8位
    IAP_TRIG = 0X5A;
    IAP_TRIG = 0XA5;//写完触发命令后，CPU会进入IDLE，读完才会恢复
    _nop_();_nop_();_nop_();_nop_();
}
static void IAP_IDLE(){//IAP关闭
    IAP_CONTR = 0X00;
    IAP_CMD = 0X00;
    IAP_TRIG = 0X00;
    IAP_ADDRH = 0X80;
    IAP_ADDRL = 0X00;
    IAP_DATA = 0X00;
}
static void EEPROM_ERASE(unsigned int addr){//擦除EEPROM
    IAP_CMD = 0X03;//擦除命令
    IAP_OPERATE(addr);
    IAP_IDLE();
}
static void EEPROM_WRITE(unsigned int addr,unsigned char A){//IAP_DATA是一个八位寄存器
    //EEPROM_ERASE(addr);//先擦除
    IAP_DATA = A;//将计数器值存储在eeprom数据寄存器中
    IAP_CMD = 0X02;//写命令
    IAP_OPERATE(addr);
    IAP_IDLE();
}
static uint8_t EEPROM_READ(unsigned int addr){
    unsigned char A;
    IAP_CMD = 0X01;//EEPROM读命令
    IAP_OPERATE(addr);
    A = IAP_DATA;
    IAP_IDLE();
    return A;
}
//该函数要求存的两个参数地址必须连续
void Save_Set_To_EEPROM(uint16_t addr,uint16_t Set1,uint16_t Set2){//向EEPROM保存电压设置值
    uint16_t addr1_H = addr;//一个地址只能存8bits
    uint16_t addr1_L = addr + 1;
    uint8_t Set1_H = Set1 >> 8;//右移八位，取Set高八位
    uint8_t Set1_L = Set1;//自动截断高八位
    uint16_t addr2_H = addr + 2;//一个地址只能存8bits
    uint16_t addr2_L = addr + 3;
    uint8_t Set2_H = Set2 >> 8;//右移八位，取Set高八位
    uint8_t Set2_L = Set2;//自动截断高八位
    EEPROM_ERASE(addr);//先擦除
    EEPROM_WRITE(addr1_H,Set1_H);
    EEPROM_WRITE(addr1_L,Set1_L);
    EEPROM_WRITE(addr2_H,Set2_H);
    EEPROM_WRITE(addr2_L,Set2_L);
}
uint16_t Read_Set_From_EEPROM(uint16_t addr){
    uint16_t addr_H = addr;//一个地址只能存8bits
    uint16_t addr_L = addr + 1;
    return (((uint16_t)EEPROM_READ(addr_H) << 8) | EEPROM_READ(addr_L));
}
