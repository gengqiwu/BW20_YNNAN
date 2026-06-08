/*
 * @author: e.he
 * @Date: 2024-07-10 16:31:10
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2024-07-29 16:41:09
 * @Description: 
 */
#ifndef __LYY_AMY_H__
#define __LYY_AMY_H__

#include <stdint.h>

/**
 * @brief 
 * 海外按摩椅-支付申请
 */
typedef struct
{
    char     sud_cmd;
    char     add;
    char     unique[6];
}__attribute__((aligned(1), packed))HW_AMY_PAY_APPLY_DEF,*PHW_AMY_PAY_APPLY_DEF;

/**
 * @brief 
 * 海外按摩椅-支付申请-启动-应答
 */
typedef struct
{
    char     sud_cmd;
    char     add;
    char     unique[6];
    char     status;//0x00-空闲 0x01-正在按摩 0x02-其他故障
}__attribute__((aligned(1), packed))HW_AMY_PAY_APPLY_REP_DEF,*PHW_AMY_PAY_APPLY_REP_DEF;

/**
 * @brief 
 * 海外按摩椅-启动
 */
typedef struct
{
    char        sud_cmd;
    char        add;
    char        unique[6];
    uint16_t    time;
    uint16_t    mode;//0x0001 男士 0x0002 女士 0x0003 老人
}__attribute__((aligned(1), packed))HW_AMY_START_DEF,*PHW_AMY_START_DEF;
/**
 * @brief 
 * 账目
 */
typedef struct
{
    char        sud_cmd;
    char        add;
    uint32_t    inc;
    uint32_t    total;
}__attribute__((aligned(1), packed))AMY_ACC_DEF,*PAMY_ACC_DEF;

/**
 * @brief 
 * 账目
 */
typedef struct
{
    uint8_t     sud_cmd;
    uint8_t     add;
    uint32_t    coin_inc;
    uint32_t    coin_total;
    uint32_t    cash_inc;
    uint32_t    cash_total;
}__attribute__((aligned(1), packed))AMY_ACC_DEF_V2,*PAMY_ACC_DEF_V2;

typedef struct
{
    char     sud_cmd;
    char     add;
    char     result;
}__attribute__((aligned(1), packed))AMY_ACC_REPLY_DEF,*PAMY_ACC_REPLY_DEF;


/**
 * @brief 
 * 故障，状态上报
 */
typedef struct
{
    char     sud_cmd;
    char     add;
    char     status;
}__attribute__((aligned(1), packed))AMY_STATUS_DEF,*PAMY_STATUS_DEF;

#define INT_HEAD_MARK           0xAA  	// 协议数据包头
#define INT_TAIL_MARK           0xDD  	// 协议数据包尾
#define PACK_MIN_LENGTH         0x06  	// 最小数据包大小

#define VOICE_CMD_SET_VOL		0x31		//设置音量
#define	VOICE_CMD_SET_VOICE		0x41		//播放指定语音

#define VOICE_FILE_START		0x01		//启动语音
#define VOICE_FILE_FINISH		0X02		//结束语音
#define VOICE_FILE_TIP			0x03		//占座提示

#define 	VOICE_LANGUAGE_ENGLISH	            0x00		//英语
#define		VOICE_LANGUAGE_MALAY	            0x01		//马来语
#define 	VOICE_LANGUAGE_ENGLISH_STRING 		"english"
#define 	VOICE_LANGUAGE_MALAY_STRING 		"malay"

#define AMY_ACCUPY_ON			0x01
#define AMY_ACCUPY_OFF			0x00


#define FILE_NAME_HW_PARA_VOICE "hw_para_voice"    //按摩椅音频参数


typedef struct 
{
	uint8_t			Volume;			// 语音板音量
	uint8_t			Language;		// 语音语种
	uint8_t			Accupy;			// 防久坐开关
   	uint8_t			Errbutton;	    // 故障显示开关
    uint16_t        key;            // 写入标识
}__attribute__((aligned(1), packed)) VOICE_PARAM_t;
extern VOICE_PARAM_t* pVoice_Param;

typedef struct 
{
	uint16_t	voice_state;		// 语音状态
	uint16_t	warning_state;		// 占座状态
    uint16_t	work_state;		    // 工作状态
	uint16_t	warning_gap;		// 占座提示间隔
    uint16_t	min_silence;		// 1分钟禁默
    uint16_t    idle_gap;           // 空闲间隔
	uint16_t    pay_value;			// 上分数
 	uint16_t    work_min_left;		// 剩余时间 
    uint8_t		ErrShowFlag;	    // 故障显示标志
    uint8_t		ErrShowCode;	    // 故障显示码
    uint8_t		ErrPowerOnFlag;	    // 故障开机标志
    uint8_t		OffStatusFlag;	    // 线下运行启动标志
}AMY_STATE_t;

extern AMY_STATE_t g_tAMYState;

/**
 * @brief 
 * 运行状态上报
 */
typedef struct
{
    uint8_t     sud_cmd;
    uint8_t     status; 
    uint8_t     fault;
    uint8_t     work_mode;
    uint32_t    time;
    uint32_t    time_total;
}__attribute__((aligned(1), packed))AMY_STATUS_DEF_V2,*PAMY_STATUS_DEF_V2;

extern int  device_amy_pay_apply(void);
extern int  device_amy_pay_start(uint16_t time);
extern void device_amy_packet_parse(uint8_t subCmd, uint8_t *data, uint16_t len);
extern uint8_t device_get_amy_err_Flag(void);
extern void device_get_amy_err_powerOn(void);
#endif










