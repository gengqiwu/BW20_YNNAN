#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "ameba.h"
#include "os_wrapper_critical.h"

#include "HAL_Log.h"
#include "HAL_Uart.h"
#include "HAL_System.h"


#include "ringBuffer.h"

enum {

    eUartDev0,
    eUartDev1,
};

typedef struct {

    UART_TypeDef    *pxUart;
    uint8_t          ucUartId;
    uint8_t          ucUartTxPin;
    uint8_t          ucUartRxPin;
    uint8_t          ucUartTxFunc;
    uint8_t          ucUartRxFunc;
    GDMA_InitTypeDef xGDMATxInitStruct;
    GDMA_InitTypeDef xGDMARxInitStruct;
    uint32_t         (*ulUartTxIRQHandlerCb)(void *);
    uint32_t         (*ulUartRxIRQHandlerCb)(void *);
    uint16_t         ucUartRxBuffLen;
    uint8_t         *pucUartRxDmaBuff;
    uint16_t         usStackDepth;
    uint8_t          ucTxSemSta;
    uint32_t         ulRingBufferLen;
    ring_buffer_t    xRingBuffer;
    xSemaphoreHandle xTxSem;
    xSemaphoreHandle xRxSem;
    vRecvFunc        xRecvFunc;
    TaskFunction_t   xUartTask;
    char             pcTaskName[24];
}HAL_UART_CONFING_T;


#define HAL_DMA_TX_BURST_SIZE	             (8U)
#define HAL_DMA_RX_BURST_SIZE	             (16U)
#define HAL_RECV_BUF_DMA_SIZE                (512U)

#define HAL_RECV_BUF_RING_SIZE               (1024U)


static uint32_t HAL_prulUart0TxIRQHandlerCb(void *pvArg);
static uint32_t HAL_prulUart0RxIRQHandlerCb(void *pvArg);

static uint32_t HAL_prulUart1TxIRQHandlerCb(void *pvArg);
static uint32_t HAL_prulUart1RxIRQHandlerCb(void *pvArg);

static void HAL_prvUartEventMainTask(void *pvArg);
static void HAL_prvUartEventScreenTask(void *pvArg);


static HAL_UART_CONFING_T g_xHalUartConfig[HAL_UART_MAX] = {

    {   UART0_DEV,  
        eUartDev0,  
        _PA_30,     
        _PA_31,   
        PINMUX_FUNCTION_UART0_TXD,
        PINMUX_FUNCTION_UART0_RXD,
        {0x00, },
        {0x00, },
        HAL_prulUart0TxIRQHandlerCb,
        HAL_prulUart0RxIRQHandlerCb,
        HAL_RECV_BUF_DMA_SIZE,  
        NULL,
        (5 * 1024 / sizeof(portSTACK_TYPE)), 
        0,  
        HAL_RECV_BUF_RING_SIZE,
        {0x00, },
        NULL,   
        NULL, 
        NULL,
        HAL_prvUartEventMainTask,     
        "uartEventMain"},  

    {   UART1_DEV,  
        eUartDev1,  
        _PB_31,             
        _PB_30,  
        PINMUX_FUNCTION_UART1_TXD,
        PINMUX_FUNCTION_UART1_RXD,
        {0x00, },
        {0x00, },
        HAL_prulUart1TxIRQHandlerCb,
        HAL_prulUart1RxIRQHandlerCb,
        HAL_RECV_BUF_DMA_SIZE / 2,  
        NULL,         
        (4 * 1024 / sizeof(portSTACK_TYPE)), 
        0, 
        HAL_RECV_BUF_RING_SIZE / 2,
        {0x00, },
        NULL,   
        NULL, 
        NULL,
        HAL_prvUartEventScreenTask, 
        "uartEventScreen"}

};


static void HAL_prvUartXTxIRQHandlerCb(uint8_t *pucSemSta, xSemaphoreHandle xTxSem, 
                                                UART_TypeDef *pxUart, GDMA_InitTypeDef xGDMATxInitStruct)
{


    GDMA_Cmd(       xGDMATxInitStruct.GDMA_Index, 
                    xGDMATxInitStruct.GDMA_ChNum, 
                    DISABLE);


    GDMA_ClearINT(  xGDMATxInitStruct.GDMA_Index, 
                    xGDMATxInitStruct.GDMA_ChNum);

    UART_TXDMACmd(  pxUart, 
                    DISABLE);

    
    UART_INTConfig( pxUart, 
                    RUART_BIT_ERBI | RUART_BIT_ELSI | RUART_BIT_ETOI, 
                    DISABLE);	

    if(*pucSemSta) {

        portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

        if (rtos_critical_is_in_interrupt()) {

            xSemaphoreGiveFromISR((QueueHandle_t)xTxSem, &xHigherPriorityTaskWoken);
            portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);

        } else {

            xSemaphoreGive((QueueHandle_t)xTxSem);
        }

        *pucSemSta = 0x00;
    }

    

}


static void HAL_prvUartXRxIRQHandlerCb(uint8_t *pucData, uint32_t ulDataLen, ring_buffer_t *pxRingBuffer,
                                    xSemaphoreHandle xRxSem, UART_TypeDef *pxUart, GDMA_InitTypeDef xGDMATxInitStruct)
{


    UART_RXDMACmd(pxUart,      DISABLE);
    
    DCache_CleanInvalidate((u32)pucData, ulDataLen);

    uint32_t ulReadDataLen = GDMA_GetBlkSize(xGDMATxInitStruct.GDMA_Index, xGDMATxInitStruct.GDMA_ChNum);

    ring_buffer_real_write( pxRingBuffer, 
                            pucData,
                            ulReadDataLen);


	GDMA_Cmd(xGDMATxInitStruct.GDMA_Index,      xGDMATxInitStruct.GDMA_ChNum, DISABLE);
    GDMA_ClearINT(xGDMATxInitStruct.GDMA_Index, xGDMATxInitStruct.GDMA_ChNum);

    UART_INTConfig( pxUart, 
                    RUART_BIT_ERBI | RUART_BIT_ELSI | RUART_BIT_ETOI, 
                    DISABLE);	

    GDMA_Cmd(xGDMATxInitStruct.GDMA_Index,      xGDMATxInitStruct.GDMA_ChNum, ENABLE);

    UART_RXDMACmd(pxUart,      ENABLE);

    portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

    xSemaphoreGiveFromISR(xRxSem, &xHigherPriorityTaskWoken);
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

}


static uint32_t HAL_prulUart0TxIRQHandlerCb(void *pvArg)
{
    (void)pvArg;

    HAL_prvUartXTxIRQHandlerCb( &g_xHalUartConfig[eUartDev0].ucTxSemSta,
                                g_xHalUartConfig[eUartDev0].xTxSem,
                                g_xHalUartConfig[eUartDev0].pxUart, 
                                g_xHalUartConfig[eUartDev0].xGDMATxInitStruct);
        
    return 0x00;

}


static uint32_t HAL_prulUart0RxIRQHandlerCb(void *pvArg)
{

    (void)pvArg;

    HAL_prvUartXRxIRQHandlerCb( g_xHalUartConfig[eUartDev0].pucUartRxDmaBuff,
                                g_xHalUartConfig[eUartDev0].ucUartRxBuffLen,
                                &g_xHalUartConfig[eUartDev0].xRingBuffer,
                                g_xHalUartConfig[eUartDev0].xRxSem,
                                g_xHalUartConfig[eUartDev0].pxUart, 
                                g_xHalUartConfig[eUartDev0].xGDMARxInitStruct);

    return 0x00;

}



static uint32_t HAL_prulUart1TxIRQHandlerCb(void *pvArg)
{
    (void)pvArg;

    HAL_prvUartXTxIRQHandlerCb( &g_xHalUartConfig[eUartDev1].ucTxSemSta,
                                g_xHalUartConfig[eUartDev1].xTxSem,
                                g_xHalUartConfig[eUartDev1].pxUart, 
                                g_xHalUartConfig[eUartDev1].xGDMATxInitStruct);
        
    return 0x00;

}


static uint32_t HAL_prulUart1RxIRQHandlerCb(void *pvArg)
{

    (void)pvArg;

    HAL_prvUartXRxIRQHandlerCb( g_xHalUartConfig[eUartDev1].pucUartRxDmaBuff,
                                g_xHalUartConfig[eUartDev1].ucUartRxBuffLen,
                                &g_xHalUartConfig[eUartDev1].xRingBuffer,
                                g_xHalUartConfig[eUartDev1].xRxSem,
                                g_xHalUartConfig[eUartDev1].pxUart, 
                                g_xHalUartConfig[eUartDev1].xGDMARxInitStruct);

    return 0x00;

}


void HAL_vUartSetParam(uint8_t ucPort,uint32_t ulBaud)
{

#define HAL_UART_IDLE_TIMEOUT                   (30U)

    uint32_t ulTimeOut = HAL_UART_IDLE_TIMEOUT * ulBaud / 1000;

    if(ulTimeOut) {

        g_xHalUartConfig[ucPort].pxUart->RX_PATH_CTRL = 
                g_xHalUartConfig[ucPort].pxUart->RX_PATH_CTRL | RUART_R_RXTO_THRS(ulTimeOut);
    }

    UART_SetBaud(g_xHalUartConfig[ucPort].pxUart,   ulBaud);

}

void HAL_vUartRecvProc(uint8_t ucPort, vRecvFunc xRecvFunc)
{
    g_xHalUartConfig[ucPort].xRecvFunc = xRecvFunc;
}


void HAL_vUartSend(uint8_t ucPort, uint8_t *pucData, uint32_t ulLen, uint32_t ulTimeout)
{

    if (g_xHalUartConfig[ucPort].xTxSem == NULL || g_xHalUartConfig[ucPort].xRxSem == NULL)
    {
        return;
    }

    DCache_CleanInvalidate((u32)pucData, ulLen);

    GDMA_InitTypeDef xGDMATxInitStruct = g_xHalUartConfig[ucPort].xGDMATxInitStruct;

    UART_TXDMACmd(g_xHalUartConfig[ucPort].pxUart,      DISABLE);

    GDMA_Cmd(   xGDMATxInitStruct.GDMA_Index, 
                xGDMATxInitStruct.GDMA_ChNum, 
                DISABLE);

	if (((ulLen & 0x03) == 0) && (((uint32_t)(pucData) & 0x03) == 0)) {

		xGDMATxInitStruct.GDMA_SrcMsize        = MsizeOne;
		xGDMATxInitStruct.GDMA_SrcDataWidth    = TrWidthFourBytes;
		xGDMATxInitStruct.GDMA_BlockSize       = ulLen >> 2;

	} else {

		xGDMATxInitStruct.GDMA_SrcMsize        = MsizeFour;
		xGDMATxInitStruct.GDMA_SrcDataWidth    = TrWidthOneByte;
		xGDMATxInitStruct.GDMA_BlockSize       = ulLen;
	}

    xGDMATxInitStruct.GDMA_SrcAddr = (uint32_t)(pucData);

	GDMA_Init(  xGDMATxInitStruct.GDMA_Index, 
                xGDMATxInitStruct.GDMA_ChNum, 
                &xGDMATxInitStruct);

    
	GDMA_Cmd(   xGDMATxInitStruct.GDMA_Index,  
                xGDMATxInitStruct.GDMA_ChNum, 
                ENABLE);


    UART_TXDMAConfig(g_xHalUartConfig[ucPort].pxUart,   HAL_DMA_TX_BURST_SIZE);
    UART_TXDMACmd(g_xHalUartConfig[ucPort].pxUart,      ENABLE);

    if (ulTimeout)
    {

        g_xHalUartConfig[ucPort].ucTxSemSta = 0x01;
        xSemaphoreTake(g_xHalUartConfig[ucPort].xTxSem, ulTimeout);
        g_xHalUartConfig[ucPort].ucTxSemSta = 0x00;
    }
}


static void HAL_prvUartEventMainTask(void *pvArg)
{

    (void)pvArg;

    uint32_t ulDataLen  = HAL_RECV_BUF_RING_SIZE;
    uint8_t  pucData[HAL_RECV_BUF_RING_SIZE + 1]  = {0x00, };

    for( ; ; ) {

        if(xSemaphoreTake(g_xHalUartConfig[eUartDev0].xRxSem, (TickType_t)portMAX_DELAY))   {

            ulDataLen  = HAL_RECV_BUF_RING_SIZE;

            ulDataLen = ring_buffer_real_read(&g_xHalUartConfig[eUartDev0].xRingBuffer, 
                                                pucData, 
                                                ulDataLen);

            if(g_xHalUartConfig[eUartDev0].xRecvFunc && ulDataLen > 0) {
                
                g_xHalUartConfig[eUartDev0].xRecvFunc(pucData, ulDataLen);
            }

        }

    }


    vTaskDelete(NULL);

}


static void HAL_prvUartEventScreenTask(void *pvArg)
{

    (void)pvArg;

    uint32_t    ulDataLen   = HAL_RECV_BUF_RING_SIZE/2;
    uint8_t     pucData[HAL_RECV_BUF_RING_SIZE/2 + 1]   = {0x00, };

    for( ; ; ) {

        if(xSemaphoreTake(g_xHalUartConfig[eUartDev1].xRxSem, (TickType_t)portMAX_DELAY))   {

            ulDataLen   = HAL_RECV_BUF_RING_SIZE/2;

            ulDataLen = ring_buffer_real_read(&g_xHalUartConfig[eUartDev1].xRingBuffer, 
                                                pucData, 
                                                ulDataLen);

            if(g_xHalUartConfig[eUartDev1].xRecvFunc && ulDataLen > 0) {
                
                g_xHalUartConfig[eUartDev1].xRecvFunc(pucData, ulDataLen);
            }

        }

    }
    
    vTaskDelete(NULL);

}


void HAL_vUartInit(uint8_t ucPort, uint32_t ulBaud)
{


	if(!g_xHalUartConfig[ucPort].pucUartRxDmaBuff) {

		g_xHalUartConfig[ucPort].pucUartRxDmaBuff = 
                    pvPortMalloc(g_xHalUartConfig[ucPort].ucUartRxBuffLen);

		if(!g_xHalUartConfig[ucPort].pucUartRxDmaBuff){

			LYY_LOGE("### uart rx malloc error! \t %d", g_xHalUartConfig[ucPort].ucUartRxBuffLen);
			return ;

		}
	}

    g_xHalUartConfig[ucPort].xTxSem = xSemaphoreCreateBinary();
    g_xHalUartConfig[ucPort].xRxSem = xSemaphoreCreateBinary();

    if( !g_xHalUartConfig[ucPort].xTxSem ||
        !g_xHalUartConfig[ucPort].xRxSem) {

        LYY_LOGE("### uart xSemaphoreCreate error!");
        vPortFree(g_xHalUartConfig[ucPort].pucUartRxDmaBuff);
		return ;
    }

    ring_buffer_init(   &g_xHalUartConfig[ucPort].xRingBuffer, 
                        g_xHalUartConfig[ucPort].ulRingBufferLen);

    if(!g_xHalUartConfig[ucPort].xRingBuffer.buffer){

        LYY_LOGE("### uart ringbuf malloc error!  %d");

        vSemaphoreDelete(g_xHalUartConfig[ucPort].xTxSem);
        vSemaphoreDelete(g_xHalUartConfig[ucPort].xRxSem);
        vPortFree(g_xHalUartConfig[ucPort].pucUartRxDmaBuff);

        return ;

    }

    UART_InitTypeDef xUartInitStruct;

    /* 关闭引脚默认模式SWID */
    if(_PA_30 == g_xHalUartConfig[ucPort].ucUartTxPin ||
       _PA_31 == g_xHalUartConfig[ucPort].ucUartRxPin) {

        Pinmux_Swdoff();
    }

    RCC_PeriphClockCmd( APBPeriph_UARTx[g_xHalUartConfig[ucPort].ucUartId], 
                        APBPeriph_UARTx_CLOCK[g_xHalUartConfig[ucPort].ucUartId], 
                        ENABLE);

	Pinmux_Config(  g_xHalUartConfig[ucPort].ucUartTxPin, 
                    g_xHalUartConfig[ucPort].ucUartTxFunc);

	Pinmux_Config(  g_xHalUartConfig[ucPort].ucUartRxPin, 
                    g_xHalUartConfig[ucPort].ucUartRxFunc);

	PAD_PullCtrl(g_xHalUartConfig[ucPort].ucUartRxPin, GPIO_PuPd_UP); 

    UART_StructInit(&xUartInitStruct);
    
    xUartInitStruct.StopBit = RUART_STOP_BIT_1;
    xUartInitStruct.WordLen = RUART_WLS_8BITS;
	xUartInitStruct.Parity  = RUART_PARITY_DISABLE;
    
	UART_Init(g_xHalUartConfig[ucPort].pxUart,      &xUartInitStruct);
	UART_SetBaud(g_xHalUartConfig[ucPort].pxUart,   ulBaud);
	UART_RxCmd(g_xHalUartConfig[ucPort].pxUart,     ENABLE);


	g_xHalUartConfig[ucPort].xGDMATxInitStruct.MuliBlockCunt = 0;
	g_xHalUartConfig[ucPort].xGDMATxInitStruct.MaxMuliBlock  = 1;

	g_xHalUartConfig[ucPort].xGDMATxInitStruct.GDMA_DIR     = TTFCMemToPeri;
	g_xHalUartConfig[ucPort].xGDMATxInitStruct.GDMA_DstHandshakeInterface = UART_DEV_TABLE[g_xHalUartConfig[ucPort].ucUartId].Tx_HandshakeInterface;
	g_xHalUartConfig[ucPort].xGDMATxInitStruct.GDMA_DstAddr = (u32)&UART_DEV_TABLE[g_xHalUartConfig[ucPort].ucUartId].UARTx->RBR_OR_UART_THR;
	g_xHalUartConfig[ucPort].xGDMATxInitStruct.GDMA_Index   = 0;
    g_xHalUartConfig[ucPort].xGDMATxInitStruct.GDMA_ChNum   = GDMA_ChnlAlloc(0, 
                                                                            (IRQ_FUN)g_xHalUartConfig[ucPort].ulUartTxIRQHandlerCb, 
                                                                            (u32)g_xHalUartConfig[ucPort].pxUart, 
                                                                            INT_PRI5);

	g_xHalUartConfig[ucPort].xGDMATxInitStruct.GDMA_IsrType = (BlockType | TransferType | ErrType);

	g_xHalUartConfig[ucPort].xGDMATxInitStruct.GDMA_DstMsize = MsizeFour;
	g_xHalUartConfig[ucPort].xGDMATxInitStruct.GDMA_DstDataWidth = TrWidthOneByte;
	g_xHalUartConfig[ucPort].xGDMATxInitStruct.GDMA_DstInc = NoChange;
	g_xHalUartConfig[ucPort].xGDMATxInitStruct.GDMA_SrcInc = IncType;

    UART_TXDMAConfig(g_xHalUartConfig[ucPort].pxUart,   HAL_DMA_TX_BURST_SIZE);


    GDMA_Cmd(   g_xHalUartConfig[ucPort].xGDMATxInitStruct.GDMA_Index, 
                g_xHalUartConfig[ucPort].xGDMATxInitStruct.GDMA_ChNum, 
                DISABLE);

    uint32_t ulTimeOut = HAL_UART_IDLE_TIMEOUT * ulBaud / 1000;

    if(ulTimeOut) {

        g_xHalUartConfig[ucPort].pxUart->RX_PATH_CTRL = 
                g_xHalUartConfig[ucPort].pxUart->RX_PATH_CTRL | RUART_R_RXTO_THRS(ulTimeOut);
    }

#if 1
    UART_INTConfig(g_xHalUartConfig[ucPort].pxUart, (RUART_BIT_ERBI | RUART_BIT_ELSI | RUART_BIT_ETOI), DISABLE);
#endif

	UART_RXDMAConfig(g_xHalUartConfig[ucPort].pxUart,   HAL_DMA_RX_BURST_SIZE);
	UART_RXDMACmd(g_xHalUartConfig[ucPort].pxUart,      ENABLE);


    UART_RXGDMA_Init(   g_xHalUartConfig[ucPort].ucUartId, 
                        &g_xHalUartConfig[ucPort].xGDMARxInitStruct, 
                        g_xHalUartConfig[ucPort].pxUart, 
                        (IRQ_FUN)g_xHalUartConfig[ucPort].ulUartRxIRQHandlerCb,
                        (uint8_t *)g_xHalUartConfig[ucPort].pucUartRxDmaBuff, 
                        0x00);

    xTaskCreate(        g_xHalUartConfig[ucPort].xUartTask,
                        g_xHalUartConfig[ucPort].pcTaskName, 
                        g_xHalUartConfig[ucPort].usStackDepth, 
                        NULL, 
                        8,
                        NULL);



}











