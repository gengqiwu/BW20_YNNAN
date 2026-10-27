/*
 * @author: e.he
 * @Date: 2024-07-10 16:30:26
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2024-07-11 16:12:27
 * @Description:
 */
#ifndef __LYY_QHHGJ_H__
#define __LYY_QHHGJ_H__

#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
// #define TRUE 1
// #define FALSE 0
/*故障码转义*/
typedef enum
{
    ErrCode_HGJ_communication_station_not_connect = 0x01, //: 通信失败
    ErrCode_HGJ_emergency,                                // 急停报警
    ErrCode_HGJ_moter_overload,                           // 过载
    ErrCode_HGJ_electrical_overheat,                      // 电过热
    ErrCode_HGJ_air_outlet_temperature_overheat,          // 出风口温度过高
    ErrCode_HGJ_temperature_sensor_open_circuit,          // 温度探头断开
    ErrCode_HGJ_temperature_sensor_short_circuit,         // 温度探头短路
    ErrCode_HGJ_inlet_temperature_sensor_broken,          // 进风口温度探头损坏
    ErrCode_HGJ_air_inlet_temperature_overheat,           // 进风口温度过高
    ErrCode_HGJ_flame_fault,                              // 点火失败
    ErrCode_HGJ_fandoor_open,                             // 风门开
    ErrCode_HGJ_temperature_fault,                        // 温度探头故障
    ErrCode_HGJ_sprinkler_sensor_fault,                   // 喷淋探头故障
    ErrCode_HGJ_cylinder_temperature_too_high,            // 内筒温度过高
    ErrCode_HGJ_check_no_cylinder_run,                    // 未检测到内筒运行
} HGJ_FAULT_TYPE;

/*报警类型*/
typedef enum
{
    Alarm_HGJ_communication_station_not_connect = 0x00000001, //: 通信失败
    Alarm_HGJ_emergency = 0x00000002,                         // 急停报警
    Alarm_HGJ_moter_overload = 0x00000004,                    // 过载
    Alarm_HGJ_electrical_overheat = 0x00000008,               // 电过热
    Alarm_HGJ_air_outlet_temperature_overheat = 0x00000010,   // 出风口温度过高
    Alarm_HGJ_temperature_sensor_open_circuit = 0x00000020,   // 温度探头断开
    Alarm_HGJ_temperature_sensor_short_circuit = 0x00000040,  // 温度探头短路
    Alarm_HGJ_inlet_temperature_sensor_broken = 0x00000080,   // 进风口温度探头损坏
    Alarm_HGJ_air_inlet_temperature_overheat = 0x00000100,    // 进风口温度过高
    Alarm_HGJ_flame_fault = 0x00000200,                       // 点火失败
    Alarm_HGJ_fandoor_open = 0x00000400,                      // 风门开
    Alarm_HGJ_temperature_fault = 0x00000800,                 // 温度探头故障
    Alarm_HGJ_sprinkler_sensor_fault = 0x00001000,            // 喷淋探头故障
    Alarm_HGJ_cylinder_temperature_too_high = 0x00002000,     // 内筒温度过高
    Alarm_HGJ_check_no_cylinder_run = 0x00004000,             // 内筒温度过高
} HGJ_ALARM_TYPE;

typedef struct
{
    uint16_t coinPerPulse;     // 单脉冲币数
    uint16_t baseCoinCnt;      // 基础投币数
    uint16_t minAddCoinCnt;    // 中温烘增加币数
    uint16_t hightAddCoinCnt;  // 高温烘增加币数
    uint16_t lowCoinCnt;       // 低温投币数
    uint16_t minCoinCnt;       // 中温烘投币数
    uint16_t hightCoinCnt;     // 高温烘投币数
} TYPE_QHHGJ_DEV_FACTORY_PARM; //

typedef struct
{
    uint32_t alarmRecord; // 报警类型位记录变量
    uint8_t isRun;        // 机器是否运行 LB17
    uint8_t isAlarm;      // 机器有无故障 LB18
    uint8_t isReady;      // 机器是否准备好运行 LB19 影响是否能启动
} TYPE_QHHGJ_DEV_STA;

/*上报到后台的状态信息*/
typedef struct
{
    uint8_t isRunSta;  // 机器运行状态：未运行、在自动运行
    uint16_t time_min; // 自动程序剩余时间(分)
    uint16_t time_sec; // 自动程序剩余时间(秒)
    // uint16_t washNum;//当前运行程序编号
    // uint16_t washAllStep;
    uint32_t alarmInfo; // 机器报警信息:0无报警
} TYPE_REPORT_QHHGJ_STA_INFO;

typedef struct
{
    uint16_t cur_coin; // 当前投币数 ADDR34
    uint16_t coin_box; // 投币钱箱统计ADDR35
    uint16_t coin_all; // 投币总统计 ADDR36
} TYPE_REPORT_QHHGJ_ACCOUNT_INFO;

extern void lyy_device_qhhgj_init(void);
extern void device_qhhgj_packet_parse(uint8_t *package, uint16_t len);

extern int device_qhhgj_pay_apply(void);
extern int device_qhhgj_pay_start(uint16_t time);
extern int device_qhhgj_factory_param_set(uint8_t *data, uint16_t len);
extern int device_qhhgj_factory_param_set_forqhhgj(uint8_t *data, uint16_t len);
extern int device_qhhgj_factory_param_get(void); // done
extern uint8_t qhhgj_report_change_status(uint8_t force);
extern void qhhgj_ctlb_post_up_raw(uint8_t *data, int datalen);
extern void qhhgj_ctlb_post_up_raw_err(uint8_t errCode);
extern void qhhgj_event_post_account(void);
extern int device_qhhgj_stop(void);
#endif
