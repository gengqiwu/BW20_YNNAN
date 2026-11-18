set(DIR     ${BASEDIR}/lyy_app)
set(HAL     ${DIR}/hal)
set(APP     ${DIR}/app)
set(CFG     ${DIR}/cfg)
set(LIB     ${DIR}/LIB)
set(HW      ${DIR}/hw)
set(UTILS   ${DIR}/utils)
set(MQTT    ${BASEDIR}/component/network/mqtt/MQTTPacket)

set(
    MODULE_IFLAGS
    ${CFG}
    ${LIB}
    ${UTILS}
    ${MQTT}
    ${HAL} 
    ${APP} 
    ${DIR}
    ${HW}/lyy_screen
    ${HW}/lyy_device
    ${HW}/lyy_user
    ${HW}/lyy_thing
	${BASEDIR}/component/network/cJSON
    ${FREERTOSDIR}/include
    ${FREERTOSDIR}/portable/GCC/AmebaDplus_KM4/non_secure
)

set(
    CSRC 
    ${LIB}/sha2.c
    ${LIB}/bsp_aes.c
    ${LIB}/hmac_sha2.c
    ${UTILS}/utils.c
    ${UTILS}/http_parser.c
    ${UTILS}/ringbuf.c
    ${UTILS}/ringbuffer.c
    ${HAL}/HAL_Log.c
    ${HAL}/HAL_Ota.c
    ${HAL}/HAL_Udp.c
    ${HAL}/HAL_Tls.c
    ${HAL}/HAL_Tcp.c
    ${HAL}/HAL_Flash.c
    ${HAL}/HAL_Wifi.c
    ${HAL}/HAL_Gpio.c
    ${HAL}/HAL_Uart.c
    ${HAL}/HAL_Sem.c
    ${HAL}/HAL_Http.c
    ${HAL}/HAL_System.c
    ${APP}/APP_Led.c
    ${APP}/APP_Factory.c
    ${APP}/APP_Param.c
    ${APP}/APP_Timer.c
    ${APP}/APP_DevAp.c
    ${APP}/APP_Event.c
    ${APP}/APP_Guide.c
    ${APP}/APP_Detect.c
    ${APP}/APP_Upgrade.c
    ${APP}/APP_ApConfig.c
    ${APP}/APP_NetWork.c
    ${APP}/APP_DrillingMode.c
    ${DIR}/APP_main.c 
    ${HW}/lyy_screen/lyy_pic.c
    ${HW}/lyy_screen/lyy_tjc.c
    ${HW}/lyy_device/lyy_device.c
    ${HW}/lyy_device/lyy_zcj.c
    ${HW}/lyy_device/lyy_frame.c
    ${HW}/lyy_device/lyy_mcu_ota.c
    ${HW}/lyy_user/lyy_start.c
    ${HW}/lyy_user/lyy_para.c
    ${HW}/lyy_thing/mqtt_api.c
    ${HW}/lyy_thing/thing_init.c
    ${HW}/lyy_thing/thing_mqtt.c
    ${HW}/lyy_thing/thing_event.c
    ${HW}/lyy_thing/thing_iot.c
    ${HW}/lyy_thing/thing_property.c
    ${HW}/lyy_thing/thing_service.c
)


ameba_internal_library(lyy_app)
target_include_directories(${CURRENT_LIB_NAME} PRIVATE ${MODULE_IFLAGS})
target_sources(${CURRENT_LIB_NAME} PRIVATE ${CSRC})
