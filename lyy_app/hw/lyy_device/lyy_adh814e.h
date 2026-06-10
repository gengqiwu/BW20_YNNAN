/*
 * @author: e.he
 * @Date: 2024-07-10 16:31:10
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2024-07-29 16:41:09
 * @Description: 
 */
#ifndef __LYY_ADH814E_H__
#define __LYY_ADH814E_H__

#include <stdint.h>

#define BOARD_ADDR (0x02)   //扩展板地址  1-4

extern uint8_t checkSta_flag;

// 设备数据
typedef struct
{

    uint8_t versions[32]; // 设备版本号
    uint8_t com_state;    // 通信状态，设备是否在线，每次查询版本号更新
    uint8_t door_state[13];

} __attribute__((aligned(1), packed)) DEVICE_RUN_DATA_DEF, *PDEVICE_RUN_DATA_DEF;
extern DEVICE_RUN_DATA_DEF data_814_t;

// poll 查询售货机状态
typedef struct
{
    uint8_t state;   // 当前设备状态 0=空闲 1=出货中 2=出货结束  
    uint8_t motor;   //当前运行电机
    uint8_t result;  // 出货结果  电机操作结果: Bit0-bit1: 0= 无故障 1=过流 2=断线 3 = 超时。Bit2: 0，掉货成功；1，掉货失败。
    uint8_t max_ele[2]; // 最大电流  ma
    uint8_t average_ele[2]; // 平均电流  
    uint8_t time; // 运行时间  0.1秒
    uint8_t temp; // 实时温度  数值：-127---127; 单位：℃

} __attribute__((aligned(1), packed)) DEVICE_RUN_STATE_DEF, *PDEVICE_RUN_STATE_DEF;
extern DEVICE_RUN_STATE_DEF state_814_t;

extern void lyy_device_adh814e_init(void);
extern int device_adh814e_pay_apply(void);
extern int device_adh814e_pay_statr(uint16_t motor_num);
extern void device_adh814e_packet_parse(uint8_t *data, uint16_t len);
extern void adh814e_cmd_ack(void);
#endif
