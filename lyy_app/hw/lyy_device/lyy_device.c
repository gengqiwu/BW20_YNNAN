/**
 * @file lyy_device.c
 * @brief 标准主板协议代理、纸钞 D6 分发、启动总账/登录同步及旧 CD 升级。
 * @note UART 实际接收经 lyy_frame_feed() 累积完整帧，D6 业务由 lyy_zcj 模块处理。
 */
/*
 * @author: e.he
 * @Date: 2022-10-09 11:19:43
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-12-12 14:45:19
 * @Description: 
 */
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "cJSON.h"

#include "APP_Param.h"
#include "APP_Detect.h"
#include "APP_DrillingMode.h"

#include "HAL_Log.h"
#include "HAL_Ota.h"
#include "HAL_WIFI.h"
#include "HAL_Uart.h"
#include "HAL_System.h"
#include "HAL_Flash.h"

#include "USER_Config.h"

#include "thing_iot.h"
#include "thing_mqtt.h"
#include "thing_event.h"
#include "thing_property.h"

#include "lyy_device.h"
#include "lyy_para.h"
#include "lyy_zcj.h"
#include "utils.h"
#include "lyy_frame.h"


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

/**
 * @brief 封装并发送旧主板 CD 扩展帧。
 * @param[in] cmd 保留形参，当前忽略并固定发送 0xCD。
 * @param[in] packet_data 已含扩展长度/子命令的载荷。
 * @param[in] len_data 载荷字节数，调用者保证不超过工作区容量。
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
    HAL_vUartSend(HAL_UART_MAIN_BOARD, tp_packet, len, 1000);
}
/**
 * @brief 构造旧 CD 升级的一个数据块，组帧后立即释放临时工作区。
 * @param[in] file_name NUL 结尾的版本/名称，当前最多复制 19 字节。
 * @param[in] file_size 协议 16 位文件大小字段，大镜像由调用者按旧约定取 FFFF。
 * @param[in] sum 总包数。
 * @param[in] num 从 1 开始的当前包序号。
 * @param[in] len 当前数据字节数，不得超过 512。
 * @param[in] context 至少 len 字节的固件内容。
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
 * @brief 校验旧主板版本查询应答的名称/版本边界，再上报 board 模块版本。
 * @param[in] data 从名称长度字段起的协议载荷。
 * @param[in] len 可读载荷字节数。
 */
void board_ota_reply_get_infor(uint8_t *data, uint16_t len)
{
    if (!data || len < 2) return;
    uint8_t nameLen = data[0];
    if ((uint16_t)(nameLen + 2) > len) return;
    uint8_t versionLen = data[1+nameLen];
    if (versionLen >= 30 || (uint16_t)(nameLen + 2 + versionLen) > len) return;
    char ver[30] = {0};
    //数据保存到boardVerson
    memcpy(ver, &data[2+nameLen], versionLen);
    thing_ota_inform_version((char *)ver, "board");
}

/**
 * @brief 确认旧主板主动版本上报，再发送云端 board 版本信息。
 * @param[in] data 包含子命令及版本长度的载荷。
 * @param[in] len 可读字节数，短帧或版本过长时忽略。
 */
void board_ota_update_infor(uint8_t *data, uint16_t len)
{

    if (!data || len < 2 || data[1] >= 30 || (uint16_t)(data[1] + 2) > len) return;

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
 * @brief 校验旧 CD 升级 ACK 长度，结果为成功或续传时发送下一个数据块。
 * @param[in] data 含子命令、名称和 ACK 结构的载荷。
 * @param[in] len 可读字节数。
 * @note 此旧链路不采用 D6/0E 的偏移匹配状态机。
 */
void board_ota_data_ack(uint8_t *data, uint16_t len)
{
    if (!data || len < 2 || data[1] > 20 || (uint16_t)(data[1] + 2 + sizeof(BOARD_OTA_DATA_ACK)) > len) return;
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
 * @brief 分发旧主板 CD 升级子命令：数据 ACK、主动版本和版本查询。
 * @param[in] subCmd 升级子命令，未知值忽略。
 * @param[in] data 指向旧协议载荷，NULL 时忽略。
 * @param[in] len 载荷字节数。
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
 * @brief 清空升级上下文，设置版本、镜像总长和 512 字节包计数。
 * @param[in] version NUL 结尾版本，最多缓存 20 字节。
 * @param[in] file_name 兼容形参，当前未使用。
 * @param[in] file_size 下载镜像总字节数，容量限制由下载层检查。
 * @note 新 D6/0E 也复用此上下文中的版本/总长，其块长由独立发送任务决定。
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
 * @brief 读取镜像缓存中的下一个至多 512 字节块，发送后推进旧协议偏移/序号。
 * @retval 1 已调用数据块发送函数，不代表对端 ACK 或物理发送成功。
 * @retval 0 镜像已读完或缓存读取失败。
 */
int board_ota_status_send(void)
{
    //todo...

    uint8_t buffer[512]={0};
    uint32_t len;

    uint16_t size = board_ota_param_t.file_size>=0xffff ? 0xffff:board_ota_param_t.file_size;
    if (board_ota_param_t.offset >= board_ota_param_t.file_size) return 0;

    len = board_ota_param_t.file_size - board_ota_param_t.offset;
    len = (len > 512) ? 512 : len;
    
    uint32_t ulAddr =  board_ota_param_t.offset;

    LYY_LOGD("===========ulAddr= [%02x] [%d]", ulAddr, len);
    
    if (HAL_lFlashAddrRead(FILE_NAME_HW_DEVICE_OTA_LOAD + ulAddr, buffer, len)) return 0;

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
 * @brief 按普通协议 LEN 补帧头/索引/XOR/DD 并直接发送。
 * @param[in,out] pack 已设置长度、命令及载荷的工作区。
 * @return 发送帧长转换为 char 后的值，不是队列深度或可靠成功标记。
 * @note 使用 8 位普通帧长度，调用者应限制范围，不能用于大包升级扩展帧。
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

    HAL_vUartSend(HAL_UART_MAIN_BOARD, tp, len, 1000);

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

    HAL_vUartSend(HAL_UART_MAIN_BOARD, tp, len, 1000);

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
 * @brief 保留的单缓冲拆包接口，普通帧或屏幕型扩展长度校验后调用 parse。
 * @param[in,out] pbuff 当前块，回调可能修改其中的数据。
 * @param[in] len 当前块字节数，不跨调用缓存半帧。
 * @param[in] parse 同步完整帧回调，可为 NULL。
 * @retval 0 输入缓冲为 NULL。
 * @retval 1 已扫描缓冲，即使没有合法帧也返回 1。
 * @note 正式 UART 流入口使用 lyy_frame_feed()，不使用本接口处理跨 DMA 块半帧。
 */
char device_overlap(uint8_t *pbuff, uint16_t len, void (*parse)(uint8_t *, uint16_t))
{
#define PACK_END    0XDD
#define PACK_HEAD   0XAA

    uint16_t index = 0;
    if (pbuff == NULL) return 0;
    while (len - index >= 6)
    {
        uint8_t *pack = pbuff + index;
        uint32_t pack_len = pack[1];
        uint32_t frame_len;
        uint32_t check_len;
        if (pack[0] != PACK_HEAD) { index++; continue; }
        if (pack_len == 0xFF)
        {
            if (len - index < 9) break;
            pack_len = (uint16_t)pack[5] | ((uint16_t)pack[6] << 8);
            frame_len = pack_len + 9;
            check_len = pack_len + 6;
        }
        else
        {
            if (pack_len < 3) { index++; continue; }
            frame_len = pack_len + 3;
            check_len = pack_len;
        }
        if (frame_len > (uint32_t)(len - index)) { index++; continue; }
        if (pack[frame_len - 1] != PACK_END ||
            check_xor_memory(pack + 1, check_len) != pack[frame_len - 2])
        { index++; continue; }
        if (parse != NULL) parse(pack, (uint16_t)frame_len);
        index += frame_len;
    }

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
 * @brief 分发标准协议命令，并将 D6 交给纸钞模块、CD 交给旧升级解析。
 * @param[in,out] package 已校验的完整可写帧。
 * @param[in] len 完整帧长，包含 AA/XOR/DD；CD 子分发另减去外层开销。
 * @pre 调用者已完成长度累积和 XOR/帧尾校验，不能直接递交原始 UART 块。
 */
void std_packetParse(uint8_t *package, uint16_t len)
{

    if (!package || len < 6) return;
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
        case 0x05: //参数上传
        {
            lyy_thing_ctlb_up_raw(&pdevPack->cmd,pdevPack->len-2);
        }break;
        case 0x10: //获取支付二维码
        {
            device_std_get_qrcode(0x10);   
        }break;
        case 0x19: //查询信号
        {
            device_std_get_net(!lyy_mqtt_get_status()); //应答信号值
        }break;
        
        case 0xD4:  //
        {
            if(pdevPack->data[0] == 0x01)//主板获取UTC时间
            {
                device_std_get_utc();
            }
        }break;
        case 0xD6: // 纸钞机
        {
            lyy_zcj_packet_parse(package, len);
        }break;
        case 0xCD: //主板升级
        {
            board_ota_packet_parse(sub_cmd, pdevPack->data, len - 6);
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
 * @brief 正常模式 UART 接收入口，将任意接收块递交持久流式分帧状态。
 * @param[in] pdata 本次接收字节。
 * @param[in] len 字节数，可能含多帧或半帧。
 * @note 仅由同一个 UART 接收任务调用，流状态跨调用保留。
 */
void lyy_DevDataParse(uint8_t *pdata,uint32_t len)
{
    static lyy_frame_stream stream;
    lyy_frame_feed(&stream, pdata, len, HAL_ulGetSysTick(), std_packetParse);
}


/**
 * @brief 周期推进启动总账查询、主板登录及联网后的累计账目上报。
 * @param[in] arg 定时参数，未使用。
 * @note 总账未准备时只发非阻塞查询并返回，不在公共软定时循环等待 ACK。
 *       之后取得登录标识，MQTT 在线时上报增量为 0 的总账并标记已登录。
 */
void APP_PrvDevLoginTask(void *arg)
{

    (void)arg;

    static uint32_t ultimeCnt;
    ZCJ_NOTE_ACCS_TOTAL *acct_t = zcj_accounts_get();
    if (!acct_t->ch_sum)
    {
        zcj_request_total_acc();
        return;
    }
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

                event_post_account_note(0, acct_t->ch_acc[2], "offline_pay", NULL, "accounts", 1);
                g_xlyyHwMark.ucGetLoginFlag = DEV_GET_LOGINED;
            }

        }break;

        default:
            break;
    }

}


/**
 * @brief 初始化故障缓存和 D6 请求锁，再注册正常 UART 接收及 2 秒登录轮询。
 * @pre 主板 UART 已由启动流程按 115200 初始化。
 * @note 正常与配网模式均需调用一次以维持 STM32 轮询应答；不注册本地现金脉冲处理。
 */
void lyy_init_device(void)
{ 

    dev_fault_init();
    zcj_protocol_init();
    HAL_vUartRecvProc(HAL_UART_MAIN_BOARD, lyy_DevDataParse);

    g_xlyyHwMark.pxPayApply = xSemaphoreCreateBinary();

#define APP_DEV_LOGIN_TASK_TIME (2 * 1000)    //单位是10ms

    static struct HalSoftTimer DevLoginTask;

    HAL_vSoftTimeInit(&DevLoginTask, 
                      APP_PrvDevLoginTask, 
                      APP_DEV_LOGIN_TASK_TIME,
                      NULL);

}


