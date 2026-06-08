/*
 * @author: e.he
 * @Date: 2022-02-10 14:42:59
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-12-06 17:41:21
 * @Description: 
 */


#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "HAL_Log.h"
#include "HAL_System.h"
#include "HAL_Flash.h"

#include "APP_Param.h"

#include "thing_init.h"
#include "thing_service.h"
#include "thing_property.h"

#include "lyy_para.h"


PAY_PARAM_DEF               pay_param_t;
//ndj_add
LANE_INFOR_TABLE_DEF        lane_net = {0};         //线上仓位参数主要禁止状态




/**
 * @brief 
 * 设备类型
 * 返回值：
 * DEVICE_TYPE_PULSE：脉冲版本 
 * DEVICE_TYPE_LEVEL：电平版本
 * DEVICE_TYPE_UART_DBJ：兑币机
 * DEVICE_TYPE_UART_WWJ：娃娃机
 * DEVICE_TYPE_UART_AMY：按摩椅
 * DEVICE_TYPE_UART_OPPOSITE:反向协议
 * DEVICE_TYPE_UART_NDJ: 扭蛋机
 * DEVICE_TYPE_UART_NOTE: 纸钞机
 * DEVICE_TYPE_DEFAULT :缺省工厂测试模式
 * @return lyy_uint8_t 
 * < 0 无匹配
 * 其他成功
 */
int get_dev_type_code(void)
{

    DEV_TYPE_DEF dev_type_t[] =
        {
            {"dev_type_default"},
            {"dev_type_pulse"},
            {"dev_type_level"},
            {"dev_type_uart_custom"},
            {"dev_type_uart_wwj_lyy"},
            {"dev_type_uart_dbj_lyy"},
            {"dev_type_uart_ndj_lyy"},
            {"dev_type_uart_amy_lyy"},
            {"dev_type_uart_xyj_lyy"},
            {"dev_type_uart_note_lyy"},
            {"dev_type_uart_qhxyj_lyy"}, /*乔禾洗衣机 add by dxw,20241010*/
            {"dev_type_uart_qhhgj_lyy"}, /*乔禾烘干机 add by dxw,20241010*/
            {"dev_type_uart_ljxyj_lyy"}, /*蓝鲸洗衣机 add by dxw,20241210*/
            {"dev_type_uart_ljhgj_lyy"}, /*蓝鲸烘干机 add by dxw,20241210*/
            {"dev_type_uart_cwxyj_lyy"}, /*创维滚筒洗衣机 add by zyl,20250605*/
            {"dev_type_uart_mshgj_lyy"},
            {"dev_type_uart_cwhgj_lyy"},  /*创维烘干机*/
            {"dev_type_uart_cwxhj_lyy"},  /*创维洗烘机*/
            {"dev_type_uart_tclxyj_lyy"}, /*TCL洗衣机*/
            {"dev_type_uart_dfssj_lyy"},  /*德锋售水机*/
            {"dev_type_uart_thxcj_lyy"},  /*探虎洗车机*/
        };

    for(uint8_t i=0;i<sizeof(dev_type_t)/sizeof(dev_type_t[0]);i++)
    {
        if(strstr(dev_type_t[i].name, APP_pcGetDevType()) != NULL) //一样
            return i;
    }
    
    return DEVICE_TYPE_DEFAULT;

}


/**
 * @brief Get the money unit object
 * 获取货币单位
 * @return char 
 */
char *get_money_unit(void)
{
    PAY_PARAM_DEF *pay_param = get_pay_param();

    //ndj_add    
    if(get_dev_type_code()==DEVICE_TYPE_UART_NDJ)
    {
        return pay_param->ndj_pkgs.currency_name;
    }
    else 
    {
        return pay_param->pkgs.currency_name;
    }    

}
/**
 * @brief 
 * 读取海外故障信息
 * @param arg 
 */
int lyy_hw_err_msg_read(uint8_t * arg)
{
    (void)arg;
    //todo..
#if 0
	return HAL_lFlashRead(FILE_NAME_ERR_MSG, arg, sizeof(DEV_FAULT));
#else
    return 1;
#endif
}
/**
 * @brief 
 * 保存海外故障信息
 * @param arg 
 */
int lyy_hw_err_msg_write(uint8_t * arg)
{
    (void)arg;
    //todo..
#if 0
	return HAL_lFlashWrite(FILE_NAME_ERR_MSG, arg, sizeof(DEV_FAULT));
#else
    return 1;
#endif
}

/**
 * @brief Get the pay param infor object
 * 获取支付参数信息
 * @return PAY_PARAM_DEF* 
 */
PAY_PARAM_DEF *get_pay_param(void)
{
    return &pay_param_t;
}

/**
 * @brief Get the pay package object
 * 扭蛋机套餐
 * @return PAY_PACKAGES_NDJ_DEF* 
 */
PAY_PACKAGES_NDJ_DEF *get_pay_ndj_packages(void)
{
    //ndj_add
    return &pay_param_t.ndj_pkgs;

}

/**
 * @brief 
 * 获取货道禁用状态
 * @param x  仓位号
 * @return uint8_t 
 * true 开启  false 禁用
 */
uint8_t ndj_get_lane_enable(uint8_t x)
{
    //ndj_add
    return lane_net.lane[x].enable;

}

/**
 * @brief 
 * 返回线上仓位信息参数
 * @return LANE_INFOR_TABLE_DEF 
 */
PLANE_INFOR_TABLE_DEF ndj_net_lane_infor(void)
{
    //ndj_add
    return &lane_net;   

} 


/**
 * @brief Get the pay channle object
 * 获取套餐信息
 * @return PAY_PACKAGES_DEF* 
 */
PAY_PACKAGES_DEF *get_pay_packages(void)
{
    return &pay_param_t.pkgs;
}





/**
 * @brief 
 * 业务信息
 * @return PDEV_PACKAGE_DEF 
 */
INFOR_BUSINESS *get_business_infor(void)
{
    return &pay_param_t.business;
}



