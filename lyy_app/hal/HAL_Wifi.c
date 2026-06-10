/*
 * @Author: linjunqi linjunqi@leyaoyao.org
 * @Date: 2024-11-25 17:49:46
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-12-09 14:51:10
 * @FilePath: \ameba-rtos\lyy_app\hal\HAL_Wifi.c
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#include "wifi_conf.h"
#include "wifi_ind.h"
#include "inic_dev_api.h"
#include "wifi_fast_connect.h"
#include "wifi_intf_drv_to_app_basic.h"

#include "lwip_netconf.h"
#include "dhcp/dhcps.h"

#include "HAL_Log.h"
#include "HAL_Wifi.h"
#include "HAL_System.h"

typedef struct {

    vWifiWorkSta                 xWifiWorkSta;
    struct _rtw_network_info_t   xConnectParam;
}HAL_WIFI_T;

static HAL_WIFI_T g_xHalWifi;


uint32_t HAL_ulGetBroadcastAddr(void)
{

    extern struct netif xnetif[NET_IF_NUM];
    struct netif *pxPnetif = &xnetif[STA_WLAN_INDEX];

    return pxPnetif->gw.addr;

}


int HAL_lGetWifiRssiToCsq(void)
{

    struct _rtw_phy_statistics_t xPhyStatistics;

    wifi_fetch_phy_statistic(&xPhyStatistics);

    int lCsq;

    if(xPhyStatistics.rssi < -113) 
    {
        lCsq = 0;
    }
    else if(xPhyStatistics.rssi == -111)
    {
        lCsq = 1;
    }     
    else if((xPhyStatistics.rssi >= -109) && (xPhyStatistics.rssi <= -53))
    {
        lCsq = (xPhyStatistics.rssi + 113) / 2;
    }
    else if((xPhyStatistics.rssi >= -51) && (xPhyStatistics.rssi < 0))
    {
        lCsq = 31;
    }
    else
    {
        lCsq = 99;
    }
        
    //return (0xff - xPhyStatistics.rssi + 1);
    return lCsq;

}


static void HAL_prvWifiStaEven(char *pcBuf, int lBufLen, int lFlags, void *pvUserdata)
{

    (void)lBufLen;
    (void)pvUserdata;

    WIFI_WORK_STA_T xWifiWorkSta = WIFI_WORK_IDLE;

    if(RTW_JOINSTATUS_SUCCESS == lFlags) {

        xWifiWorkSta = WIFI_STA_CONNED;

    }else if(RTW_JOINSTATUS_FAIL == lFlags) {

        /* 连接失败 */
        switch(((struct rtw_event_join_fail_info_t *)pcBuf)->fail_reason) {

            case RTW_CONNECT_SCAN_FAIL:
                LYY_LOGE("### Can not found target AP\n");
                break;

            case RTW_CONNECT_INVALID_KEY:
                LYY_LOGE("### Password format wrong\n");
                break;

            case RTW_CONNECT_AUTH_PASSWORD_WRONG:
            case RTW_CONNECT_4WAY_PASSWORD_WRONG:
                LYY_LOGE("### Password may wrong\n");
                break;

            case RTW_CONNECT_AUTH_FAIL:
                LYY_LOGE("### Auth fail\n");
                break;

            case RTW_CONNECT_ASSOC_FAIL:
                LYY_LOGE("### Assoc fail\n");
                break;

            case RTW_CONNECT_4WAY_HANDSHAKE_FAIL:
                LYY_LOGE("### 4 way handshake fail\n");
                break;

            default:
                LYY_LOGE("### fail_reason unknow: [%d]\n", 
                        ((struct rtw_event_join_fail_info_t *)pcBuf)->fail_reason);
                break;
        }

    }else if(RTW_JOINSTATUS_DISCONNECT == lFlags) {

        /* 连接后断开 */
        switch(((struct rtw_event_disconn_info_t *)pcBuf)->disconn_reason) {
            
			case WLAN_REASON_DRV_AP_LOSS:
				LYY_LOGE("### Disconnect by Driver, detect AP loss\n");
				break;

			case WLAN_REASON_DRV_AP_CHANGE:
				LYY_LOGE("### Disconnect by Driver, detect AP change\n");
				break;

			case WLAN_REASON_APP_DISCONN:
				LYY_LOGE("### Disconnect by APP call disconnect API\n");
				break;

			case WLAN_REASON_APP_CONN_WITHOUT_DISCONN:
				LYY_LOGE("### Disconnect by APP call connect without calling disconnect first\n");
				break;

            default :
                LYY_LOGE("### (disconn_reason unknow) : [%d]\n", 
                        ((struct rtw_event_disconn_info_t *)pcBuf)->disconn_reason);               

        }
  
#if 0
        if(RTW_SUCCESS == wifi_connect(&g_xHalWifi.xConnectParam, 0)) {

            LYY_LOGD(">>> WIFI RECONNECT SET SUCCESS");
        }
#endif

    }else {

        switch(lFlags) {

            case    RTW_JOINSTATUS_STARTING             :				     
            case    RTW_JOINSTATUS_SCANNING             :			
            case    RTW_JOINSTATUS_AUTHENTICATING       :	    		
            case    RTW_JOINSTATUS_AUTHENTICATED        :			    
            case    RTW_JOINSTATUS_ASSOCIATING          :		      
            case    RTW_JOINSTATUS_ASSOCIATED           :			     
            case    RTW_JOINSTATUS_4WAY_HANDSHAKING     :	    	
            case    RTW_JOINSTATUS_4WAY_HANDSHAKE_DONE  :

                    xWifiWorkSta = WIFI_STA_CONNING;
                break;   

            default : 
                break;      
        }
        
        LYY_LOGD(">>> WIFI_STA_CONNING : [%d]", lFlags);

    }

    if(g_xHalWifi.xWifiWorkSta) {

        g_xHalWifi.xWifiWorkSta(xWifiWorkSta);
    }

}

static void HAL_prvWifiApEven(char *pcBuf, int lBufLen, int lFlags, void *pvUserdata)
{

    (void)lBufLen;
    (void)pvUserdata;
    (void)pcBuf;
    
    LYY_LOGD("==== WIFI_EVENT_STA_ASSOC lfags : [%d]", lFlags);
}

void HAL_vWifiStaInit(WIFI_INFO_T *pxWifiInfo, vWifiWorkSta xFunc)
{

    (void)xFunc;

    wifi_fast_connect_enable(0);

    /* 等待系统运行稳定 */ 
    vTaskDelay(100  / portTICK_PERIOD_MS);
    
    /* 需完善此部分逻辑 */
    if(wifi_on(RTW_MODE_STA) == RTW_SUCCESS) {
        
        LYY_LOGD(">>> Wi-Fi is enabled successfully");

    }else {

        LYY_LOGE("### Failed to enable Wi-Fi");

    }

    wifi_fast_connect_enable(0);

    if (wifi_is_running(STA_WLAN_INDEX)) {

        LYY_LOGD(">>> STA interface is running.\n");
    }else {

        LYY_LOGE("### STA interface is not running");
    }

    /* 禁用低功耗模式 */ 
    wifi_set_lps_enable(FALSE);

    /* 禁用内部节能模式 */
    wifi_set_ips_internal(FALSE);

    wifi_reg_event_handler( WIFI_EVENT_JOIN_STATUS, 
                            HAL_prvWifiStaEven, 
                            NULL);

    g_xHalWifi.xConnectParam.password_len  = strlen(pxWifiInfo->pcPwd);
    g_xHalWifi.xConnectParam.password      = (unsigned char *)pxWifiInfo->pcPwd;

    g_xHalWifi.xConnectParam.ssid.len      = strlen(pxWifiInfo->pcSsid) > sizeof(g_xHalWifi.xConnectParam.ssid.val) ? 
                                        sizeof(g_xHalWifi.xConnectParam.ssid.val) : strlen(pxWifiInfo->pcSsid);
    
    memcpy( g_xHalWifi.xConnectParam.ssid.val, 
            pxWifiInfo->pcSsid, 
            g_xHalWifi.xConnectParam.ssid.len);

    g_xHalWifi.xConnectParam.security_type = g_xHalWifi.xConnectParam.password_len ? 
                                                RTW_SECURITY_WPA2_AES_PSK : RTW_SECURITY_OPEN;
    
    LYY_LOGD(">>> g_xHalWifi.xConnectParam.ssid.val : [%s]", g_xHalWifi.xConnectParam.ssid.val);
    

    /* 异步 */
    if(RTW_SUCCESS == wifi_connect(&g_xHalWifi.xConnectParam, 0)) {

        LYY_LOGD(">>> wifi connect set success");
    }


    wifi_config_autoreconnect(RTW_AUTORECONNECT_INFINITE);

}


void HAL_vWifiApInit(WIFI_INFO_T *pxWifiInfo, vWifiWorkSta xFunc)
{

    (void)xFunc;

    /* 等待系统运行稳定 */ 
    vTaskDelay(100  / portTICK_PERIOD_MS);

    wifi_set_lps_enable(FALSE);

    wifi_set_ips_internal(FALSE);

    wifi_reg_event_handler( WIFI_EVENT_STA_ASSOC, 
                            HAL_prvWifiApEven, 
                            NULL);

    struct _rtw_softap_info_t xSoftApInfo = {0x00 };

    xSoftApInfo.password_len  = strlen(pxWifiInfo->pcPwd);
    xSoftApInfo.password      = (unsigned char *)pxWifiInfo->pcPwd;

    xSoftApInfo.ssid.len      = strlen(pxWifiInfo->pcSsid) > sizeof(xSoftApInfo.ssid.val) ? 
                                        sizeof(xSoftApInfo.ssid.val) : strlen(pxWifiInfo->pcSsid);

    memcpy( xSoftApInfo.ssid.val, 
            pxWifiInfo->pcSsid, 
            xSoftApInfo.ssid.len);    

    xSoftApInfo.security_type = xSoftApInfo.password_len ? 
                                                RTW_SECURITY_WPA2_AES_PSK : RTW_SECURITY_OPEN;

    xSoftApInfo.channel = 6;

    LYY_LOGD(">>> xSoftApInfo.password_len : [%d][%04x]", xSoftApInfo.password_len, xSoftApInfo.security_type );

    wifi_start_ap(&xSoftApInfo);
    
    uint32_t                   ulTimeout    = 3;
    struct _rtw_wifi_setting_t xSetting     = {0x00 };

    wifi_fast_connect_enable(0);
    wifi_config_autoreconnect(RTW_AUTORECONNECT_DISABLE);

	while(ulTimeout) {

		wifi_get_setting(SOFTAP_WLAN_INDEX, &xSetting);

		if (strlen((char *)xSetting.ssid) > 0) {

			if (strcmp((char *)xSetting.ssid, 
                (char *)xSoftApInfo.ssid.val) == 0) {

				break;
			}
		}

		vTaskDelay(500  / portTICK_PERIOD_MS);

        ulTimeout--;

	}

    if(ulTimeout) {

        uint32_t      ulIpAddr;
        uint32_t      ulNetMask;
        uint32_t      ulGw;

        extern struct netif xnetif[NET_IF_NUM];
        struct netif *pxPnetif = &xnetif[SOFTAP_WLAN_INDEX];

        uint8_t ucApIP[4]       = {192, 168, 4, 1};
        uint8_t ucApNetmask[4]  = {255, 255, 255, 0};
        uint8_t ucApGw[4]       = {192, 168, 4, 1};

        ulIpAddr    = CONCAT_TO_UINT32( ucApIP[0], 
                                        ucApIP[1], 
                                        ucApIP[2], 
                                        ucApIP[3]);

        ulNetMask   = CONCAT_TO_UINT32( ucApNetmask[0], 
                                        ucApNetmask[1], 
                                        ucApNetmask[2], 
                                        ucApNetmask[3]);

        ulGw        = CONCAT_TO_UINT32( ucApGw[0], 
                                        ucApGw[1], 
                                        ucApGw[2], 
                                        ucApGw[3]);

        LwIP_SetIP( SOFTAP_WLAN_INDEX, 
                    ulIpAddr, 
                    ulNetMask, 
                    ulGw);

        dhcps_init(pxPnetif);

        LYY_LOGD("WIFI AP INIT.....");

    }

                                       
}


void HAL_vWifiLwipDhcpInit(void)
{
    LwIP_DHCP(WLAN0_IDX, DHCP_START);
}


void HAL_vWifiInit(WIFI_INFO_T *pxWifiInfo, vWifiWorkSta xFunc)
{


    WIFI_STA == pxWifiInfo->xMode ? HAL_vWifiStaInit(pxWifiInfo, xFunc) : 
                                                HAL_vWifiApInit(pxWifiInfo, xFunc);

    
    g_xHalWifi.xWifiWorkSta = xFunc;


    LYY_LOGI(">>> wifi init ");

}




