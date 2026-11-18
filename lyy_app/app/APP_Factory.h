/** @file APP_Factory.h @brief 工厂烧号入口与默认配网凭据。 */
#ifndef __APP_FACTORY_H__
#define __APP_FACTORY_H__

#include "USER_Config.h"


#define LYY_WIFI_SSID           "leyaoyao_config"
#define LYY_WIFI_PWD            "leyaoyao168"


/** @brief 兼容工厂入口，委托 APP_vTestInit()。 */
extern void APP_vFactoryInit(void);
/** @brief 初始化工厂接收及提示定时器，仅在无有效设备号的启动路径调用。 */
extern void APP_vTestInit(void);
/**
 * @brief 保存工厂默认参数，再写入设备号；提交方需读回确认。
 * @param[in] devId 已通过字符和 4～32 字节长度检查的 NUL 结尾设备号。
 */
extern void APP_vParamInfoInit(char *devId);

#endif




