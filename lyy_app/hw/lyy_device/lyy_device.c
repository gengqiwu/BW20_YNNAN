/*
 * @author: e.he
 * @Date: 2022-10-09 11:19:43
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-12-12 14:45:19
 * @Description: 
 */
#include <stdio.h>
#include <string.h>


#include "APP_Param.h"
#include "APP_Pluse.h"
#include "APP_Detect.h"
#include "APP_DrillingMode.h"

#include "HAL_Log.h"
#include "HAL_Ota.h"
#include "HAL_WIFI.h"
#include "HAL_Uart.h"
#include "HAL_System.h"
#include "HAL_Flash.h"

#include "USER_Config.h"
#include "thing_init.h"

#include "thing_iot.h"
#include "thing_mqtt.h"
#include "thing_property.h"

#include "lyy_tjc.h"
#include "lyy_device.h"
#include "lyy_dbj.h"
#include "lyy_wwj.h"
#include "lyy_amy.h"
#include "lyy_ndj.h"
#include "lyy_xyj.h"
#include "lyy_xcj.h"
#include "lyy_hgj.h"
#include "lyy_cwhgj.h"
#include "lyy_cwxyj.h"
#include "lyy_cwxhj.h"
#include "lyy_mshgj.h"
#include "lyy_tclxyj.h"
#include "lyy_dfssj.h"
#include "thing_event.h"
#include "lyy_koreapos.h"

#include "utils.h"


enum {

    DEV_GET_LOGIN_LDLE,
    DEV_GET_LOGINING,
    DEV_GET_LOGIN_TIMEOUT,
    DEV_GET_LOGIN_SUCCESS,
    DEV_GET_LOGINED,
};

LyyHwMark_t     g_xlyyHwMark;
DEVICE_COM_DEF  tdevCom = {0};

BOARD_OTA_PARAM_DEF     board_ota_param_t;
APPLY_START_DEF apply_start_t;

DEVICE_ACCOUNT_DEF account_data_t = {0};
DEVICE_NTP_DEF  ntp_time_t = {0};

/**
 * @brief 
 * 扩展帧
 * @param cmd 
 * @param packet_data 
 * @param len_data 
 */
void device_packet_ext_send(uint8_t cmd,uint8_t *packet_data,uint16_t len_data)
{

    (void)cmd;

    DEVICE_PACK_DEF tDevPack;

    uint8_t *tp_packet = &tDevPack.head;
    uint16_t len = len_data + 6;
    tDevPack.head = 0xAA;
    tDevPack.end = 0xDD;
    tDevPack.index = 0x01;
    tDevPack.cmd = 0xCD;
    tDevPack.len = 0xff;
    memcpy(tDevPack.data,packet_data,len_data);
    tDevPack.check = check_xor_memory(&tDevPack.len, len-3);
    tp_packet[len - 2] = tDevPack.check;
    tp_packet[len - 1] = tDevPack.end;
    HAL_vUartSend(HAL_UART_MAIN_BOARD, tp_packet, len, 500);
}
/**
 * @brief 
 * 发送数据包 -- 主板升级
 * @param file_name 
 * @param sum 
 * @param num 
 * @param data_len 
 * @param context 
 */
void device_board_ota_data_send(char *file_name,uint16_t file_size,uint16_t sum,uint16_t num,uint16_t len,uint8_t *context)
{
    uint16_t frame_data_len = len+10; //整个数据区长度
    BOARD_OTA_DATA_DEF  packet = {0};
    memset(&packet,0x00,sizeof(BOARD_OTA_DATA_DEF));

    packet.name_len = strnlen(file_name, sizeof(packet.file_name) - 1);

    if(packet.name_len > 20) // 文件名称大于20，字节赋值最大为20
    {
       packet.name_len = 20 ;
    }
    packet.frame_len = frame_data_len + packet.name_len;
    packet.sub_cmd   = 0x01;


    memcpy(packet.file_name,file_name,packet.name_len);

    packet.file_size = file_size;
    packet.packet_sum = sum;
    packet.packet_num = num;
    packet.data_len = len;

    memcpy(packet.file_data,context,len);
    uint8_t *dataBuf = NULL;
    dataBuf = pvPortMalloc(600);   
    if(dataBuf == NULL) return;    
    memcpy(&dataBuf[0],&packet.frame_len,2);
    memcpy(&dataBuf[2],&packet.sub_cmd,1);
    memcpy(&dataBuf[3],&packet.name_len,1);
    memcpy(&dataBuf[4],packet.file_name,packet.name_len);
    memcpy(&dataBuf[4+packet.name_len],&packet.file_size,2);
    memcpy(&dataBuf[6+packet.name_len],&packet.packet_sum,2);
    memcpy(&dataBuf[8+packet.name_len],&packet.packet_num,2);
    memcpy(&dataBuf[10+packet.name_len],&packet.data_len,2);
    memcpy(&dataBuf[12+packet.name_len],packet.file_data,packet.data_len);

    device_packet_ext_send(0xCD,(uint8_t *)dataBuf, packet.frame_len+2);
    
    vPortFree(dataBuf);
}

/**
 * @brief 
 * 主板升级，盒子获取主板本地固件信息应答处理
 */
void board_ota_reply_get_infor(uint8_t *data, uint16_t len)
{
    (void)len;
    uint8_t nameLen = data[0];
    uint8_t versionLen = data[1+nameLen];
    char ver[30] = {0};
    //数据保存到boardVerson
    memcpy(ver, &data[2+nameLen], versionLen);
    thing_ota_inform_version((char *)ver, "board");
}

/**
 * @brief 
 * 上报主板固件信息
 * @param data 
 * @param len 
 */
void board_ota_update_infor(uint8_t *data, uint16_t len)
{

    (void)len;

    uint8_t senddata[2] = {0x00,};
    uint8_t nameLen = data[1];
    char ver[30] = {0};
    senddata[0] = 0x02;//sub_cmd
    senddata[1] = 0x00;//File_result
    device_packet_data_length(0xCD, senddata, sizeof(senddata));

    //数据保存到boardVerson
    memset(ver, 0x00, sizeof(ver));
    memcpy(ver, &data[2], nameLen);
    thing_ota_inform_version((char *)ver, "board");
}

/**
 * @brief 
 * 主板升级，发送升级数据包应答处理
 */
void board_ota_data_ack(uint8_t *data, uint16_t len)
{
    (void)len;
    uint8_t nameLen = data[1];
    char ver_soft[21]={0};
    BOARD_OTA_DATA_ACK ack_ota = {0};
    LYY_LOGI("name length: %d", nameLen);
    memcpy(ver_soft,&data[2],nameLen);
    LYY_LOGI("ver_soft:%s",ver_soft);
    memcpy((uint8_t *)&ack_ota.size,&data[2+nameLen],sizeof(BOARD_OTA_DATA_ACK));
    LYY_LOGI("size:%d",ack_ota.size);
    LYY_LOGI("total:%d",ack_ota.total);
    LYY_LOGI("number:%d",ack_ota.number);
    LYY_LOGI("result:%d",ack_ota.result);
    //释放信号量
    if(ack_ota.result == 0 || ack_ota.result == 1) // 0:成功 1：续传 2:重新开始传
        board_ota_status_send();                    //继续传输
}
/**
 * @brief
 * 主板升级指令解析
 * @param pBuf 数据
 * @param len 数据长度
 */
void board_ota_packet_parse(uint8_t subCmd, uint8_t *data, uint16_t len)
{
    if(data == NULL) return;

    LYY_LOGD("board_ota_packet_parse subCmd: %02X", subCmd);
        
    switch (subCmd)
    {
    case 0x01:  // 数据包应答
        board_ota_data_ack(data, len);
        break;
    case 0x02:  //主板上传本地固件信息
        board_ota_update_infor(data, len);
        break;
    case 0x03:  //获取本地固件信息应答
        board_ota_reply_get_infor(data, len);
        break;
    default:
        break;
    }
}
/**
 * @brief 
 * 
 * @param version 
 * @param file_name 
 */
void board_ota_param_init(char *version,char *file_name,uint32_t file_size)
{

    (void)file_name;

    memset(&board_ota_param_t,0,sizeof(BOARD_OTA_PARAM_DEF));
	int len = strnlen(version, sizeof(board_ota_param_t.version)-1);
  
	if(len > 20) len = 20;
    memcpy(board_ota_param_t.version,version,len);
    LYY_LOGI("board_ota_param_t.version=[%s]", board_ota_param_t.version);

#if 0
	len = strlen(file_name);
	if(len > 20) len = 20;
    strncpy(board_ota_param_t.file_name,file_name,len);
    LYY_LOGI("file_name=[%s]", board_ota_param_t.file_name);
#endif

    board_ota_param_t.file_size = file_size;
    board_ota_param_t.number = 1;
    board_ota_param_t.sum = file_size/512;
    if(file_size%512 != 0) board_ota_param_t.sum++; //不是整数需+1数据包

}

/**
 * @brief
 *
 * @param version
 * @param file_name
 */
void screen_ota_param_init(char *version, char *file_name, uint32_t file_size)
{

    (void)file_name;

    memset(&board_ota_param_t, 0, sizeof(BOARD_OTA_PARAM_DEF));
    int len = strnlen(version, sizeof(board_ota_param_t.version) - 1);

    if (len > 20)
        len = 20;
    memcpy(board_ota_param_t.version, version, len);
    LYY_LOGI("board_ota_param_t.version=[%s]", board_ota_param_t.version);

#if 0
	len = strlen(file_name);
	if(len > 20) len = 20;
    strncpy(board_ota_param_t.file_name,file_name,len);
    LYY_LOGI("file_name=[%s]", board_ota_param_t.file_name);
#endif

    board_ota_param_t.file_size = file_size;
    board_ota_param_t.number = 1;
    board_ota_param_t.sum = file_size / 4096;
    if (file_size % 4096 != 0)
        board_ota_param_t.sum++; // 不是整数需+1数据包
}

/**
 * @brief 
 * 发送主板ota数据包
 */
int board_ota_status_send(void)
{
    //todo...

    uint8_t buffer[512]={0};
    uint32_t len;

    uint16_t size = board_ota_param_t.file_size>=0xffff ? 0xffff:board_ota_param_t.file_size;

    len = board_ota_param_t.file_size - board_ota_param_t.offset;
    len = (len > 512) ? 512 : len;
    
    uint32_t ulAddr =  board_ota_param_t.offset;

    LYY_LOGD("===========ulAddr= [%02x] [%d]", ulAddr, len);
    
    HAL_vFileReadOffset(THING_IOT_BOARD_PATH, buffer, len, ulAddr);

    board_ota_param_t.offset += len; //中间帧

    device_board_ota_data_send(board_ota_param_t.version,
                                size,
                                board_ota_param_t.sum,
                                board_ota_param_t.number++,
                                len,
                                buffer);

    return 1;
}

/**
 * @brief
 * 设备数据帧组包
 * @param pack
 * @return char
 * 发送队列已有的成员个数
 */
char device_buildPacket(PDEVICE_PACK_DEF pack)
{
    uint8_t *tp = &pack->head;
    uint8_t len = pack->len + 3;

    pack->head = 0xAA;
    pack->end = 0xDD;
    pack->index = 0x01;

    pack->check = check_xor_memory(&pack->len, pack->len);
    tp[len - 2] = pack->check;
    tp[len - 1] = pack->end;

    HAL_vUartSend(HAL_UART_MAIN_BOARD, tp, len, 500);

    return len;
}

char device_packet_send(PDEVICE_PACK_DEF pack, uint16_t inlen)
{
    uint8_t *tp = &pack->head;
    uint16_t len = inlen + 3;
    pack->head = 0xAA;
    pack->end = 0xDD;
    pack->index = 0x01;

    pack->check = check_xor_memory(&pack->len, inlen);
    tp[len - 2] = pack->check;
    tp[len - 1] = pack->end;

    HAL_vUartSend(HAL_UART_MAIN_BOARD, tp, len, 500);

    return len;

}

/**
 * @brief 
 * 带数据区长度
 * @param cmd 
 * @param data 
 * @param dataLen 
 */
void device_packet_data_length(uint8_t cmd, uint8_t *data, uint16_t dataLen)
{
    DEVICE_PACK_DEF tDevPack;

    if(dataLen > sizeof(tDevPack.data)) return;

    memset(&tDevPack, 0, sizeof(tDevPack));
    tDevPack.cmd = cmd;

    if(dataLen+3 >= 0xff)
    {
        tDevPack.len = 0xff;
    }
    else
    {
        tDevPack.len = dataLen+3;
    }
    
    if((dataLen>0) && (data != NULL))
    {
        memcpy(tDevPack.data, data, dataLen);
    }
    device_packet_send(&tDevPack, dataLen+3);
}

/**
 * @brief 
 * 
 * @param cmd 
 * @param pack_data 
 * @param len 
 * @return int8_t 
 */
int8_t device_send_cmd_packet(uint8_t cmd , uint8_t *pack_data,uint16_t len)
{
    DEVICE_PACK_DEF packet = {0};
    memset(&packet,0,sizeof(DEVICE_PACK_DEF));
    packet.cmd = cmd;
    packet.len = len+3;
    memcpy(packet.data,pack_data,len);
    return device_buildPacket(&packet);
}

/**
 * @brief 德峰售水机数据封包发送
 * @param
 * @param pack_data
 * @param len
 * @return int8_t
 */
int8_t device_send_dfssj_packet(uint8_t *pack_data, uint16_t len)
{
    DEVICE_DF_PACK_DEF packet = {0};
    uint8_t check;
    // memset(&packet, 0, sizeof(DEVICE_DF_PACK_DEF));

    packet.head = 0xAB;
    packet.len = len + 2;
    packet.cmd = 0x0F;
    memcpy(packet.data, pack_data, len);
    check = check_xor_memory(&packet.len, packet.len);
    packet.data[len] = check;
    packet.data[len + 1] = 0xAD;

    HAL_vUartSend(HAL_UART_MAIN_BOARD, &packet.head, packet.len + 3, 500);
    return (packet.len + 3);
}

/**
 * @brief 发送查询链接指令
 * 1、开机主动查询获取主板登录表示
 * 2、当作主板支付申请指令
 * @param type
 * TYPE_QUREY_LOGIN_FLAG : 获取登录标识
 * TYPE_QUREY_PAY_APPLY ：支付申请
 */
void device_qureyConect(uint8_t type)
{

    DEVICE_PACK_DEF tDevPack;

    memset(&tDevPack, 0, sizeof(tDevPack));
    tdevCom.qurey_type = type;
    tDevPack.cmd = 0x01;
    tDevPack.len = 0x0B;
    memcpy(tDevPack.data, APP_pcGetParamDevId(), 8);

    device_buildPacket(&tDevPack);

}



/**
 * @brief
 * 反向协议判断格式
 * @param pack
 * @param len
 * @return char
 * TRUE :正常
 * FALSE :异常
 */
char opposite_isPacket(uint8_t *pack, uint16_t len)
{
    //todo...
    (void)pack;
    (void)len;
    return 1;
}


/**
 * @brief
 *
 * @param pack
 * @param len
 * @return char
 * PACKET_NUM_ONE   : 一包正常数据帧
 * PACKET_NUM_ERROR : 错误数据帧
 * PACKET_NUM_MORE  : 多个数据包
 */
char devcie_isPacket(uint8_t *pack, uint16_t len)
{

    //todo..
    (void)pack;
    (void)len;
    return 1;
}

/**
 * @brief
 * 叠包处理 正向才会使用
 * @param pbuff
 * @param len
 * @param parse
 * @return char
 */
char device_overlap(uint8_t *pbuff, uint16_t len, void (*parse)(uint8_t *, uint16_t))
{
#define PACK_END    0XDD
#define PACK_HEAD   0XAA

    uint8_t index = 0;
    uint8_t pack_len;
    uint8_t data_len = len;
    uint8_t *pack = NULL;

    do
    {
        LYY_LOGD("inter_overlap  data_len:%d", data_len);
        pack = pbuff + index;     //缓存帧头

        if (pack[0] != PACK_HEAD) //帧头不正确
        {
            index++;    //位置偏移
            data_len--; //处理数据减少
            LYY_LOGD("PACK_HEAD is error");
            continue;
        }
        pack_len = pack[1];          //数据帧长度字段
        if (data_len < pack_len + 3) //长度不对
        {
            index++; //位置偏移
            data_len--;
            LYY_LOGD("packet len is error");
            continue;
        }
        if (pack[pack_len + 2] != PACK_END) //帧尾不正确
        {
            index++; //位置偏移
            data_len--;
            LYY_LOGD("PACK_END is error");
            continue;
        }

        if (check_xor_memory(&pack[1], pack[1]) != pack[pack_len + 1]) //校验不正确
        {
            index++; //位置偏移
            data_len--;
            LYY_LOGD("device check is error");
            continue;
        }
        index += pack_len + 3; //位置偏移
        if (data_len >= pack_len + 3)
            data_len -= pack_len + 3; //
        else
            data_len--;
        if (parse != NULL)
        {
            parse(pack, pack_len + 3); //数据解析
        }
        else
        {
            LYY_LOGD("parse is error");
        }
    } while (data_len >= 6);

    return 1;
}

/**
 * @brief
 * 叠包处理 正向才会使用  乔禾蓝鲸使用
 * @param pbuff
 * @param len
 * @param parse
 * @return char
 */
char device_overlap_qh(uint8_t *pbuff, uint16_t len, void (*parse)(uint8_t *, uint16_t))
{

    uint8_t index = 0;
    uint8_t pack_len;
    uint8_t data_len = len;
    uint8_t *pack = NULL;
    uint16_t crc16, crc16Tmp;
    lyy_printf_hex(pbuff, len);
    do
    {
        LYY_LOGD("inter_overlap  data_len:%d", data_len);
        pack = pbuff + index; //
        if (pack[0] != 0x01)
        {

            index++; // 位置偏移
            data_len--;
            continue;
        }
        if (data_len < 3)
            return 1;

        if ((pack[1] == 0x01) || (pack[1] == 0x03)) // 指令不同，长度字段不同
        {
            pack_len = pack[2];
        }
        else
        {
            pack_len = 0x03;
        }
        if (data_len < pack_len + 5)
        {
            index++; // 位置偏移
            data_len--;
            continue;
        }
        crc16 = ModBusCRC16_long(pack, pack_len + 3);
        crc16Tmp = (pack[pack_len + 4] << 8) + pack[pack_len + 3];
        // LYY_LOGD("pack_len:%02X crc16:%04X  crc16Tmp:%04X ",pack_len, crc16,crc16Tmp);
        if (crc16 != crc16Tmp)
        {
            LYY_LOGD("device check is error");
            index++; // 位置偏移
            data_len--;
            continue;
        }
        index += pack_len + 5; // 位置偏移
        if (data_len >= pack_len + 5)
            data_len -= pack_len + 5; //
        else
            data_len--;
        if (parse != NULL)
        {
            parse(pack, pack_len + 5); // 数据解析
        }
        else
        {
            LYY_LOGD("parse is error");
        }
    } while (data_len >= 8);

    return 1;
}

/**
 * @brief
 * 串口数据帧默认处理方法
 * @param package
 * @param len
 */
void device_parse(uint8_t *package, uint16_t len)
{   
    //todo..
    (void)package;
    (void)len;
}


/**
 * @brief
 * 获取信号应答
 * @param sta
 */
void device_std_get_net(uint8_t sta)
{
    DEVICE_PACK_DEF tDevPack;

    memset(&tDevPack, 0, sizeof(tDevPack));
    tDevPack.cmd = 0x19;
    tDevPack.len = 5;
    tDevPack.data[0] = HAL_lGetWifiRssiToCsq();
    tDevPack.data[1] = sta;

    device_buildPacket(&tDevPack);

}

/**
 * @brief 
 * 获取二维码
 * @param sta 
 */
void device_std_get_qrcode(uint8_t cmd)
{

    DEVICE_PACK_DEF tDevPack;

    char PayList[] = "hw_gl_test";

    memset(&tDevPack, 0, sizeof(tDevPack));
    memcpy(tDevPack.data, PayList, strlen(PayList));
    tDevPack.cmd = cmd;
    tDevPack.len = strlen(PayList) + 3;
    device_buildPacket(&tDevPack);
}
/**
 * @brief 
 * 应答获取utc时间
 */
void device_std_get_utc(void)
{
    LYY_LOGD("device_rsp_getUTC");

    DEVICE_PACK_DEF tDevPack;

    memset(&tDevPack, 0, sizeof(tDevPack));

    tDevPack.len = 10;
    tDevPack.cmd = 0xd4;
    tDevPack.data[0] = 0x01;
    tDevPack.data[1] = 24;
    tDevPack.data[2] = 1;
    tDevPack.data[3] = 1;
    tDevPack.data[4] = 1;
    tDevPack.data[5] = 1;
    tDevPack.data[6] = 1;

    device_buildPacket(&tDevPack);  

}



/**
 * @brief 
 * 登陆标识
 * @return uint16_t 
 */
uint16_t login_mark_get(void)
{
   return g_xlyyHwMark.ucLoginMark; 
}



/**
 * @brief
 * 正向协议解析
 * @param package
 * @param len
 */
void std_packetParse(uint8_t *package, uint16_t len)
{

    PDEVICE_PACK_DEF pdevPack = (PDEVICE_PACK_DEF)package;
    uint8_t sub_cmd = package[4];

    LYY_LOGD("cmd:%02X sub_cmd:%02X",pdevPack->cmd,sub_cmd);

    switch (pdevPack->cmd) // 根据命令判断
    {
        case 0x01: //查询连接应答 或者 支付申请应答
        {

            if(g_xlyyHwMark.ucGetLoginFlag != DEV_GET_LOGINED) 
            {

                if(DEV_GET_LOGIN_LDLE == g_xlyyHwMark.ucGetLoginFlag) {

                    g_xlyyHwMark.ucGetLoginFlag = DEV_GET_LOGIN_SUCCESS;
                }
                
                if (pdevPack->index == 0x02)                       //扩展的登陆标识
                {
                    g_xlyyHwMark.ucLoginMark = data_arrayTolittle16(package + 4);
                }
                else //旧版本登录标识
                    g_xlyyHwMark.ucLoginMark = pdevPack->index;

                LYY_LOGD(">>> LOGIN MARK  : [%d]", g_xlyyHwMark.ucLoginMark);
            }
            else
            {
                LYY_LOGD("lmq_serviceReply_pay_apply");
                if(g_xlyyHwMark.ucPaySem)
                {
                    xSemaphoreGive(g_xlyyHwMark.pxPayApply);
                    g_xlyyHwMark.ucPaySem = 0x00;
                }
            }

        }break;
        
        case 0x03: //上分指令
        {
            g_xlyyHwMark.startResult = pdevPack->data[0];   //非 0 成功 0失败
            LYY_LOGD("start rsp 03 cmd :%d",g_xlyyHwMark.startResult);

            if(g_xlyyHwMark.ucPaySem)
            {
                xSemaphoreGive(g_xlyyHwMark.pxPayApply);
                g_xlyyHwMark.ucPaySem = 0x00;
            }

        }break;
       
        case 0x04: //获取娃娃机总账
        {
            
        }break;
        
        case 0x05: //参数上传
        {
            lyy_thing_ctlb_up_raw(&pdevPack->cmd,pdevPack->len-2);
        }break;
        case 0x07: //东之宇 扭蛋机定制协议
        {
            device_ndj_dzy_packet_parse(package,len);    
        }break;
        case 0x0D: //外设指令
        {
            
        }break;
        
        case 0x10: //获取支付二维码
        {
            device_std_get_qrcode(0x10);   
        }break;
        case 0x13: //账目上传 娃娃机
        {
            #ifdef ACCOUNT_LENGTH_CHECK
            if(pdevPack->len == 0x0a)
            #endif
                device_wwj_accounts_update(package, len); //转换为脉冲版本账目指令 对应A2/A3

        }break;
        case 0x14: //账目上传 兑币机
        {

            #ifdef ACCOUNT_LENGTH_CHECK
            if(pdevPack->len == 0x0f)
            #endif
                device_dbj_accounts_update(package, len); //转换为脉冲版本账目指令 对应A2/A3

        }
        break;
        case 0x16: //获取兑币机账目
        {
            // netty_rspAccDbj(pdevPack->data, pdevPack->len, login_mark_get()); //
        }break;
        
        case 0x19: //查询信号
        {
            device_std_get_net(!lyy_mqtt_get_status()); //应答信号值
        }break;
        
        case 0x20: //出币指令
        {
            device_dbj_out_coins_cmd_20(package,len);
        }break;
        
        case 0x30:  //镁金娃娃机自定义参数，账目
        {
            lyy_thing_ctlb_up_raw(&pdevPack->cmd, pdevPack->len-2);
        }
        break;
        case 0xc9: //售货机特定指令
        {
  
        }break;
        case 0xCC: //扭蛋机指令 https://leyaoyao.yuque.com/vbutb7/ozftku/tey0qx#NMBLt
        {
            device_ndj_packet_parse(package,len);
        }break;
        case 0xd1: //新兑币机协议
        {
            //海外兑币机处理
            device_dbj_packet_parse(sub_cmd, pdevPack->data, pdevPack->len);
        }break;
        case 0xD4:  //
        {
            if(pdevPack->data[0] == 0x01)//主板获取UTC时间
            {
                device_std_get_utc();
            }
        }break;
        case 0xCD: //主板升级
        {
            board_ota_packet_parse(sub_cmd, pdevPack->data, pdevPack->len);
        }
        break;
        case 0x37: //按摩椅指令
        {
            if(get_dev_type_code() == DEVICE_TYPE_UART_AMY)
            {
                device_amy_packet_parse(sub_cmd, pdevPack->data, pdevPack->len);
            }else
            {
                lyy_thing_ctlb_up_raw(&pdevPack->cmd, pdevPack->len-2);
            }
        }
        break;
        default: 
        {
            lyy_thing_ctlb_up_raw(&pdevPack->cmd, pdevPack->len-2);
        }break;
    
    }
}


/**
 * @brief
 * 反向协议解析
 * @param package
 * @param len
 */
void opposite_packetParse(uint8_t *package, uint16_t len)
{
    //todo..
    (void)package;
    (void)len;
}

/**
 * @brief
 * 乔禾洗烘机协议解析
 * @param package
 * @param len
 */
void qh_packetParse(uint8_t *package, uint16_t len)
{
    int typeCode = get_dev_type_code();
    switch (typeCode)
    {
    case DEVICE_TYPE_UART_LJXYJ:
    case DEVICE_TYPE_UART_QHXYJ:
        device_qhxyj_packet_parse(package, len);
        break;
    case DEVICE_TYPE_UART_QHHGJ:
    case DEVICE_TYPE_UART_LJHGJ:
        device_qhhgj_packet_parse(package, len);
        break;
    default:
        device_parse(package, len);
        break;
    }
}

void lyy_DevDataParse(uint8_t *pdata,uint32_t len)
{
    uint32_t pack_len = 0;
    static uint8_t recvpakg[256];
    static uint8_t recvindex = 0;
    uint8_t pkgLen = 0;

    // 针对modbus协议增加包检查
    LYY_LOGD("lyy_DevDataParse\r\n");
    lyy_printf_hex(pdata, len);

    if(len < 4) return ;

    if (get_dev_type_code() > DEVICE_TYPE_UART_NOTE)   //针对工厂协议，非乐摇摇协议
    {

        if (get_dev_type_code() < DEVICE_TYPE_UART_SKYWORTH_GTXYJ)  //针对乔禾蓝鲸洗衣机烘干机
        {
            memcpy(recvpakg + recvindex, pdata, len);
            recvindex += len;
            if (recvindex >= 3)
            {
                pkgLen = pdata[2];
                if (recvindex < (pkgLen + 5))
                {
                    return;
                }
            }
            else
                return;
        }

        LYY_LOGD("lyy_DevDataParse\r\n");
        switch (get_dev_type_code())
        {
        case DEVICE_TYPE_UART_SKYWORTH_GTXYJ:
            skyworth_xyj_packet_parse(pdata, len);
            break;
        case DEVICE_TYPE_UART_SKYWORTH_GTHGJ:
            skyworth_hgj_packet_parse(pdata, len);
            break;
        case DEVICE_TYPE_UART_SKYWORTH_GTXHJ:
            skyworth_xhj_packet_parse(pdata, len);
            break;
        case DEVICE_TYPE_UART_MSHGJ:
            mshgj_packet_parse(pdata, len);
            break;
        case DEVICE_TYPE_UART_TCLXYJ:
            tclxyj_packet_parse(pdata, len);
            break;
        case DEVICE_TYPE_UART_DFSSJ:
            device_dfssj_packet_parse(pdata, len);
            break;
        case DEVICE_TYPE_UART_THXCJ:
            xcj_packet_parse(pdata, len);
            break;
        case DEVICE_TYPE_UART_KOREAPOS:
            koreapos_packet_parse(pdata, len);
            break;
        default:
            device_overlap_qh(recvpakg, recvindex, qh_packetParse);
            break;
        }

        // 清空
        recvindex = 0;
        memset(recvpakg, 0, sizeof(recvpakg));
    }else{

        if ((pdata[0] == 0xAA) && (pdata[3] == 0x07)) // 东之宇定制
        {
            pack_len = pdata[2];
            pack_len <<= 8;
            pack_len += pdata[1];
            LYY_LOGD("len1:%d ;len2:%d", len - 4, pack_len);
            if (len < pack_len + 4) // 判断长度是否正确
            {
                LYY_LOGD("PACK_LEN is error"); // 长度不正确
                return;
            }
            if (pdata[pack_len + 3] != 0xdd) // 判断尾包是否正确
            {
                LYY_LOGD("PACK_END is error"); // 长度不正确
                return;
            }
            LYY_LOGD("check1:%d ;check2:%d", check_xor_memory(&pdata[1], pack_len + 1), pdata[pack_len + 2]);
            if (check_xor_memory(&pdata[1], pack_len + 1) != pdata[pack_len + 2])
            {
                LYY_LOGD("PACK_CHECK is error"); // 校验不正确
                return;
            }
            std_packetParse(pdata, pack_len + 4); // 数据解析
        }
        else
        {
            device_overlap(pdata, len, std_packetParse);
        }
    }

    
}


void APP_PrvDevLoginTask(void *arg)
{

    (void)arg;

    static uint32_t ultimeCnt;

    if (get_dev_type_code() > DEVICE_TYPE_UART_NOTE)
        return;

    switch (g_xlyyHwMark.ucGetLoginFlag)
    {

        case DEV_GET_LOGIN_LDLE:{

            /* 获取登录标识 */
            device_qureyConect(TYPE_QUREY_LOGIN_FLAG);
            ultimeCnt++;

        }break;
        

        case DEV_GET_LOGIN_SUCCESS : {
            
            if(lyy_mqtt_get_status()) {

                char msg[20] = {0};

                snprintf(msg, sizeof(msg), "success : %ld",ultimeCnt);
                device_login_infor_update(login_mark_get(), 0x01,   msg);
                property_post_login_flag(login_mark_get(),  0x01,   msg);

                g_xlyyHwMark.ucGetLoginFlag = DEV_GET_LOGINED;
            }

        }break;

        default:
            break;
    }

}

void APP_query_point_Task(void *arg)
{

    (void)arg;

#define APP_QUERY_POINT_TASK_TIME (10 * 60)
    static uint32_t point_time_Cnt;
    point_time_Cnt++;
    if (point_time_Cnt >= APP_QUERY_POINT_TASK_TIME)  //十分钟上报一次
    {
        point_time_Cnt = 0;
        LYY_LOGD("tjc_infor_t.point_config.point_enabled=[%d]", tjc_infor_t.point_config.point_enabled);
        if (tjc_infor_t.point_config.point_enabled)
        {
            event_post_query_marketing_point_config();
        }
    }
}


uint64_t get_timestamp(void)
{
    return ntp_time_t.timestamp;
}

void ntp_get_timestamp(void)  
{
    static uint8_t first_flag = 0;
    static uint64_t timestamp_new = 0;

    if (first_flag == 0 && lyy_mqtt_get_status()) // 首次联网获取时间
    {
        first_flag = 1;
        event_post_ntp_req(ntp_time_t.timestamp);
    }

    if ((ntp_time_t.timestamp - timestamp_new) > (10 * 60 * 1000) && lyy_mqtt_get_status()) // 超过1小时重新获取时间
    {
        timestamp_new = ntp_time_t.timestamp;
        event_post_ntp_req(ntp_time_t.timestamp);
    }
}

void APP_ntp_req_Task(void *arg)
{
    (void)arg;
    ntp_time_t.timestamp +=10;
}

// 全局文件互斥锁，防止业务/补发任务并发读写Flash文件
static SemaphoreHandle_t s_xBillFileMutex = NULL;

static void bill_cache_lock(void)
{
    if (s_xBillFileMutex == NULL)
        s_xBillFileMutex = xSemaphoreCreateMutex();
    if (s_xBillFileMutex)
        xSemaphoreTake(s_xBillFileMutex, portMAX_DELAY);
}

static void bill_cache_unlock(void)
{
    if (s_xBillFileMutex)
        xSemaphoreGive(s_xBillFileMutex);
}

// 写入头部元数据
static int bill_meta_save(CacheMeta_t *meta)
{
    bill_cache_lock();
    // 偏移0位置写入元数据
    int ret = HAL_vFileWriteOffset(OFFLINE_ACCOUNT_PATH, (uint8_t *)meta, sizeof(CacheMeta_t), 0);
    bill_cache_unlock();
    if (ret != sizeof(CacheMeta_t))
    {
        LYY_LOGE("meta write fail %d", ret);
        return -1;
    }
    return 0;
}

// 读取头部元数据
static int bill_meta_load(CacheMeta_t *meta)
{
    bill_cache_lock();
    int ret = HAL_vFileReadOffset(OFFLINE_ACCOUNT_PATH, (uint8_t *)meta, sizeof(CacheMeta_t), 0);
    bill_cache_unlock();
    if (ret != sizeof(CacheMeta_t))
        return -1;
    // 校验魔数，非法分区直接重置
    if (meta->mark != OFFLINE_ACCOUNT_MARK_INIT)
        return -2;
    return 0;
}

// 根据下标读写单条账单条目
static int bill_item_write(uint32_t idx, BillCacheItem_t *item)
{
    if (idx >= MAX_BILL_NUM)
        return -1;
    size_t offset = sizeof(CacheMeta_t) + idx * BILL_ITEM_LEN;
    bill_cache_lock();
    int ret = HAL_vFileWriteOffset(OFFLINE_ACCOUNT_PATH, (uint8_t *)item, BILL_ITEM_LEN, offset);
    bill_cache_unlock();
    return (ret == BILL_ITEM_LEN) ? 0 : -1;
}

static int bill_item_read(uint32_t idx, BillCacheItem_t *item)
{
    if (idx >= MAX_BILL_NUM)
        return -1;
    size_t offset = sizeof(CacheMeta_t) + idx * BILL_ITEM_LEN;
    bill_cache_lock();
    int ret = HAL_vFileReadOffset(OFFLINE_ACCOUNT_PATH, (uint8_t *)item, BILL_ITEM_LEN, offset);
    bill_cache_unlock();
    return (ret == BILL_ITEM_LEN) ? 0 : -1;
}

uint32_t bill_cache_get_pending(void) // 查询当前积压条数（用于云端状态上报）
{
    CacheMeta_t meta;
    if (bill_meta_load(&meta) != 0)
        return 0;
    return meta.total_pending;
}

/**
 * @brief 删除整个账单缓存区域
 *        擦除OFFLINE_ACCOUNT_PATH文件并重置元数据，调用后write_idx/read_idx/total_pending全部归零
 *        适用场景：云端下发清空指令、设备复位、文件损坏后恢复
 *        沿用bill_item_read的lock/unlock保护模式，避免与补发任务并发冲突
 * @return 0 成功；-1 失败
 */
int bill_cache_clear(void)
{
    CacheMeta_t meta;
    memset(&meta, 0, sizeof(CacheMeta_t));
    meta.mark = OFFLINE_ACCOUNT_MARK_INIT; // 重置为有效分区标记

    bill_cache_lock();
    HAL_vFileClear(OFFLINE_ACCOUNT_PATH); // 擦除整个区域：meta + 全部条目
    bill_cache_unlock();

    if (bill_meta_save(&meta) != 0)
    {
        LYY_LOGE("bill cache clear meta save fail");
        return -1;
    }

    LYY_LOGI("bill cache cleared");
    return 0;
}

/**
 * @brief 新增账单存入环形缓存（设备产生账目时调用）
 * @param item 待写入的账单条目指针，由调用方填充unique_code/time/total等字段
 * @return 0 成功；-1 失败（参数空或Flash写入失败）
 */
int bill_cache_push(BillCacheItem_t *item)
{
    if (item == NULL)
    {
        LYY_LOGE("bill push null item");
        return -1;
    }

    CacheMeta_t meta;
    // 元数据非法（首次使用 / 文件损坏 / mark不符）：重建分区
    if (bill_meta_load(&meta) != 0)
    {
       return -1;
    }

    if (meta.write_idx >= MAX_BILL_NUM)
    {
        return -1;
    }
    // 将新账单写入write_idx槽位
    LYY_LOGE("meta.write_idx = [%d]", meta.write_idx);
    if (bill_item_write(meta.write_idx, item) != 0)
    {
        LYY_LOGE("bill push write fail idx=%u", meta.write_idx);
        return -1;
    }

    meta.write_idx += 1;
    if (meta.total_pending==0 && meta.write_idx>0) // 如果之前没有积压数据，read_idx也要跟着更新
    {
        meta.read_idx = meta.write_idx - 1;
    }
    meta.total_pending++;
    account_data_t.account_infor_flag = 0; // 标记设备可以上报数据了

    return bill_meta_save(&meta);
}

void offline_accounts_save(uint32_t increment, uint32_t total,uint8_t data_type,uint8_t pay_method)
{

    if (increment <= 0 || data_type <= 0 || pay_method <= 0)
        return;

    BillCacheItem_t item;
    memset(&item, 0, sizeof(BillCacheItem_t));
    item.increment = increment;
    item.total = total;
    item.position_x = 0;
    item.position_y = 0;
    item.type = data_type;
    item.pay_method = pay_method;

    char id[6] = {0x00};
    char unique_code[13] = {0x00};
    vRandomGet(id, sizeof(id));
    utils_lhexToString(unique_code, (const unsigned char *)id, sizeof(id));
    LYY_LOGE("unique_code=%s", unique_code);
    memcpy(item.unique_code, unique_code, 6);
    LYY_LOGE("item.unique_code=%s", item.unique_code);

    item.time = get_timestamp();
    bill_cache_push(&item);
}

void offline_accounts_save_ndj(uint32_t increment, uint32_t total, uint8_t position_x, uint8_t data_type, uint8_t pay_method)
{

    if (increment <= 0 || data_type <= 0 || pay_method <= 0)
        return;

    BillCacheItem_t item;
    memset(&item, 0, sizeof(BillCacheItem_t));
    item.increment = increment;
    item.total = total;
    item.position_x = position_x;
    item.position_y = 0;
    item.type = data_type;
    item.pay_method = pay_method;

    char id[6] = {0x00};
    char unique_code[13] = {0x00};
    vRandomGet(id, sizeof(id));
    utils_lhexToString(unique_code, (const unsigned char *)id, sizeof(id));
    LYY_LOGE("unique_code=%s", unique_code);
    memcpy(item.unique_code, unique_code, 6);
    LYY_LOGE("item.unique_code=%s", item.unique_code);

    item.time = get_timestamp();
    bill_cache_push(&item);
}

/**
 * @brief
 * 获取离线账目支付方式类型
 * @param code
 * @return char*
 */
char *
get_offline_accounts_pay_method_type(uint8_t code)
{
    static char  rsl[30]={0};
    memset(rsl,0,sizeof(rsl));
    if (code == PAY_METHOD_ONLINE)
        strcpy(rsl, "online_pay");
    else if(code == PAY_METHOD_OFFLINE) strcpy(rsl,"offline_pay");
    else if(code == PAY_METHOD_POS_DEBIT_CARD)
        strcpy(rsl, "pos_debit_card_pay");
    else if(code == PAY_METHOD_POS_CREDIT_CARD)
        strcpy(rsl, "pos_credit_card_pay");
    else if(code == PAY_METHOD_POS_ALL)
        strcpy(rsl, "pos_pay");
    else if(code == PAY_METHOD_CHANGE)
        strcpy(rsl, "give_change");
    else  strcpy(rsl,"other");
    return  rsl;  
}

/**
 * @brief
 * 获取离线账目数据类型
 * @param code
 * @return char*
 */
char *get_offline_accounts_data_type(uint8_t code)
{
    static char rsl[30] = {0};
    memset(rsl, 0, sizeof(rsl));
    if (code == MSG_TYPE_COIN_OUT)
        strcpy(rsl, "out_coins");
    else if (code == MSG_TYPE_AMOUNT)
        strcpy(rsl, "money");
    else if (code == MSG_TYPE_GIFT_OUT)
        strcpy(rsl, "gift");
    else if (code == MSG_TYPE_COIN_IN)
        strcpy(rsl, "in_coins");
    else if (code == MSG_TYPE_PULSE_COIN)
        strcpy(rsl, "pulse_coins");
    else if (code == MSG_TYPE_PULSE_GIFT)
        strcpy(rsl, "pulse_gift");
    else
        strcpy(rsl, "other");
    return rsl;
}

void device_offline_account_upload(void)
{
    CacheMeta_t meta;
    int ret = bill_meta_load(&meta);

    if (ret!=0)
    {
        event_post_log("bill meta load fail");
        //event_post_log_hextostring(&meta.total_pending, 4);
        LYY_LOGE("bill meta load fail %d", ret);
        return;
    }

    LYY_LOGE("meta.total_pending =%d", meta.total_pending);
    if (meta.total_pending == 0)
    {
        account_data_t.account_infor_flag = 1;
        return;
    }

    LYY_LOGE("meta.read_idx = [%d]", meta.read_idx);
    LYY_LOGE("meta.write_idx = [%d]", meta.write_idx);

    BillCacheItem_t item;
    ret = bill_item_read(meta.read_idx, &item);

    if (ret != 0)
    {
        event_post_log("bill read fail");
        LYY_LOGE("bill read fail %d", ret);
        return;
    }

    if (strncmp(item.unique_code, account_data_t.unique_code, 6) != 0)
    {
       //上报账目
       LYY_LOGE("item.unique_code=%s", item.unique_code);
       event_post_new_offline_accounts(&item);
    }
    else
    {
        if (meta.write_idx<=0)
        {
            LYY_LOGE("meta.write_idx=0 fail");
            meta.total_pending = 0;
            bill_meta_save(&meta);
        }
        else if (meta.read_idx < meta.write_idx-1)  
        {
            meta.read_idx++;
            meta.total_pending = meta.write_idx - meta.read_idx;
            LYY_LOGE("meta.write_idx = [%d] meta.read_idx = [%d]", meta.write_idx, meta.read_idx);
            bill_meta_save(&meta);
        }
        else if (meta.write_idx > 1500)  //防止多次擦除flash  当写入超过1500条数据后，且没有积压数据了，就清空文件系统。最大为2000条数据
        {
            LYY_LOGE("bill_cache_clear()");
            bill_cache_clear();
        }else
        {
            meta.total_pending = 0;
            LYY_LOGE("clear meta.total_pending");
            bill_meta_save(&meta);
        }
    }
}
/**
 * @brief
 *
 * @param arg
 */
void thread_offine_account_infor(void *arg)
{
    (void)arg;
    while (1)
    {
        vTaskDelay(4000 / portTICK_PERIOD_MS);
        if (lyy_mqtt_get_status() == 1 && account_data_t.account_infor_flag == 0)
        {
           device_offline_account_upload();
        }
        else if (lyy_mqtt_get_status() == 0)
        {
            account_data_t.account_infor_flag = 0; // 设备离线后重置为0，等待下次设备上线后上报
        }

        //时间戳获取
        ntp_get_timestamp();

        //上报当前时间戳
        // char timestamp_str[21] = {0};
        // snprintf(timestamp_str, sizeof(timestamp_str), "%llu", get_timestamp());
        // event_post_log(timestamp_str);
    }
}

void lyy_device_offine_account_init(void)
{

    //读取元数据
    CacheMeta_t meta;
    int ret = bill_meta_load(&meta);
    if (meta.mark != OFFLINE_ACCOUNT_MARK_INIT)
    {
        //bill_cache_clear();
        LYY_LOGE("bill_meta_load fail %d, reset", ret);
        memset(&meta, 0, sizeof(CacheMeta_t));
        meta.mark = OFFLINE_ACCOUNT_MARK_INIT;
        bill_meta_save(&meta);
    }

#define ACCOUNT_THREAD_NAME "NDJ_INFOR"
#define ACCOUNT_THREAD_STACK (1024 * 4)

    xTaskCreate(thread_offine_account_infor,
                ACCOUNT_THREAD_NAME,
                ACCOUNT_THREAD_STACK,
                NULL,
                5,
                NULL);
}



/**
 * @brief
 * 初始化设备
 * 串口版本才需要初始化
 * 其他版本不初始化
 */
void lyy_init_device(void)
{ 

    dev_fault_init();

    ndj_value_init();

    HAL_vUartRecvProc(HAL_UART_MAIN_BOARD, lyy_DevDataParse);

    g_xlyyHwMark.pxPayApply = xSemaphoreCreateBinary();

#define APP_DEV_LOGIN_TASK_TIME (2 * 1000)    //单位是10ms

    static struct HalSoftTimer DevLoginTask;

    HAL_vSoftTimeInit(&DevLoginTask, 
                      APP_PrvDevLoginTask, 
                      APP_DEV_LOGIN_TASK_TIME,
                      NULL);

    static struct HalSoftTimer query_marketing_point_config_Task;

    HAL_vSoftTimeInit(&query_marketing_point_config_Task,
                      APP_query_point_Task,
                      1000,
                      NULL);

    static struct HalSoftTimer ntp_req_Task;

    HAL_vSoftTimeInit(&ntp_req_Task,
                      APP_ntp_req_Task,
                      1,
                      NULL);

    if ((get_dev_type_code() == DEVICE_TYPE_UART_QHXYJ) || ((get_dev_type_code() == DEVICE_TYPE_UART_LJXYJ)))
    {
        lyy_device_qhxyj_init(); /*add by dxw 20241009*/
    }
    else if ((get_dev_type_code() == DEVICE_TYPE_UART_QHHGJ) || ((get_dev_type_code() == DEVICE_TYPE_UART_LJHGJ)))
        lyy_device_qhhgj_init(); /*add by dxw 20241009*/
    else if (get_dev_type_code() == DEVICE_TYPE_UART_SKYWORTH_GTXYJ)
        lyy_device_cwxyj_init(); /*add by zyl 20250605*/
    else if (get_dev_type_code() == DEVICE_TYPE_UART_SKYWORTH_GTHGJ)
        lyy_device_cwhgj_init(); /*add by zyl 20250605*/
    else if (get_dev_type_code() == DEVICE_TYPE_UART_SKYWORTH_GTXHJ)
        lyy_device_cwxhj_init(); /*add by zyl 20250605*/
    else if (get_dev_type_code() == DEVICE_TYPE_UART_MSHGJ)
        lyy_device_mshgj_init(); /*add by zyl 20250605*/
    else if (get_dev_type_code() == DEVICE_TYPE_UART_DFSSJ)
        lyy_device_dfssj_task_init(); /*add by zyl 20260402*/
    else if (get_dev_type_code() == DEVICE_TYPE_UART_THXCJ)
        lyy_device_xcj_init(); /*add by zyl 20260402*/

    lyy_device_ndj_init();

    lyy_device_offine_account_init();

}


