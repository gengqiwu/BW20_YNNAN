/*
 * @author: e.he
 * @Date: 2024-07-10 16:30:26
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2024-07-11 16:12:27
 * @Description: 
 */
#ifndef __LYY_QHXYJ_H__
#define __LYY_QHXYJ_H__

#include "stdint.h" 
#include "stdio.h"
#include "stdlib.h"
#include "string.h"


// #define TRUE  1
// #define FALSE 0

/*报警类型*/
typedef enum {
    Alarm_HMI_communication_station_not_connect   = 0x00000001,//:通信失败
    Alarm_emergency                               = 0x00000002,//急停报警
    Alarm_converter_fault                         = 0x00000004,//变频器故障
    Alarm_machine_overshak                        = 0x00000008,//严重超震
    Alarm_door_station_abnormal                   = 0x00000010,//门状态异常
    Alarm_door_bolt_input_fault                   = 0x00000020,//门销输入错误
    Alarm_add_water_overtime                      = 0x00000040,//加水超时
    Alarm_heat_overtime                           = 0x00000080,//加热超时
    Alarm_drain_water_overtime                    = 0x00000100,//排水超时
    Alarm_exceed_temperature                      = 0x00000200,//加热超温
    Alarm_water_level_parameter_not_calbrication  = 0x00000400,//液位参数未校准
    Alarm_temperature_sensor_open_circuit         = 0x00000800,//温度检测器开路
    Alarm_temperature_sensor_short_circuit        = 0x00001000,//温度检测器短路
}ALARM_TYPE;

/*故障码转义*/
typedef enum {
    ErrCode_XYJ_communication_station_not_connect       = 0x01,//:通信失败
    ErrCode_XYJ_emergency                               ,//急停报警
    ErrCode_XYJ_converter_fault                          ,//变频器故障
    ErrCode_XYJ_machine_overshak                     ,//严重超震
    ErrCode_XYJ_door_station_abnormal         ,//门状态异常
    ErrCode_XYJ_door_bolt_input_fault         ,//门销输入错误
    ErrCode_XYJ_add_water_overtime        ,//加水超时
    ErrCode_XYJ_heat_overtime         ,//加热超时
    ErrCode_XYJ_drain_water_overtime          ,//排水超时
    ErrCode_XYJ_exceed_temperature                             ,//加热超温
    ErrCode_XYJ_water_level_parameter_not_calbrication  ,//液位参数未校准
    ErrCode_XYJ_temperature_sensor_open_circuit                       ,//温度检测器开路
    ErrCode_XYJ_temperature_sensor_short_circuit                  ,//温度检测器短路
}XYJ_FAULT_TYPE;


typedef struct{
    uint32_t alarmRecord;//报警类型位记录变量
    uint8_t isRun;//机器是否运行 LB51 LB50
    uint8_t isAlarm;//机器有无报警 LB52
    uint8_t isReady;//机器是否准备好运行 LB53
    uint8_t isDoorOk;//门是否关到位 LB110 影响是否能启动
    uint8_t isAutoRun;//机器是否在自动运行 LB113
}TYPE_QHXYJ_DEV_STA;


/*上报到后台的状态信息*/
typedef struct{
    uint8_t washAutoRunSta;//机器运行状态：未运行、在自动运行
    uint16_t washTime_min;//自动程序剩余时间(分)
    uint16_t washTime_sec;//自动程序剩余时间(秒)
    uint16_t washNum;//当前运行程序编号
    //uint16_t washAllStep;
    uint32_t washAlarmInfo;//机器报警信息:0无报警
}TYPE_REPORT_QHXYJ_STA_INFO;

typedef struct{
    uint16_t  cur_coin;//当前投币数 ADDR35
    uint16_t  coin_box;//投币钱箱统计ADDR36
    uint16_t  coin_all;//投币总统计 ADDR14
}TYPE_REPORT_QHXYJ_ACCOUNT_INFO;

typedef struct{
    uint16_t  P_Value[14];//P* 需要的金额
}QHXYJ_FACTORY_PAY_SETTING;

extern uint16_t ModBusCRC16_long(uint8_t *cmd, unsigned long len);
extern void lyy_device_qhxyj_init(void);
extern void device_qhxyj_packet_parse(uint8_t *package, uint16_t len);
extern int device_qhxyj_factory_param_set(uint8_t *data, uint16_t len);
extern int device_qhxyj_factory_param_get(void); // done
extern int device_qhxyj_global_param_get(void);
extern int device_qhxyj_global_param_set(uint8_t *data, uint16_t len);
extern int device_qhxyj_step_param_get(uint8_t step);
extern int device_qhxyj_step_param_set(uint8_t step, uint8_t *data, uint16_t len);
extern int device_qhxyj_pay_apply(void);
extern int device_qhxyj_pay_start(uint16_t time);
extern void qhxyj_ctlb_post_up_raw(uint8_t *data, int datalen);
extern void qhxyj_ctlb_post_up_raw_err(uint8_t errCode); //-- done;
extern uint8_t qhxyj_report_change_status(uint8_t force);
extern void qhxyj_ctlb_post_up_raw_global(uint8_t *data, int datalen);    //-- done
extern void qhxyj_ctlb_post_up_raw_step(uint8_t step, uint8_t *data, int datalen); //-- done
extern int terminal_device_data(uint8_t *buff, uint16_t len);
extern int device_qhxyj_stop(void);
#endif



