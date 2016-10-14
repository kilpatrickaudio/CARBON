/*
 * USB MIDI Device for STM32 - Config / Hardware Driver
 *
 * Written by: Andrew Kilpatrick
 * Copyright 2016: Kilpatrick Audio with template code from ST
 *
 * This file is part of CARBON.
 *
 * CARBON is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CARBON is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with CARBON.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include "usbd_conf.h"
#include "usbd_core.h"
#include "../config.h"

// device handle
PCD_HandleTypeDef usbdev_handle;

/**
  * @brief  Initializes the PCD MSP.
  * @param  hpcd: PCD handle
  * @retval None
  */
void HAL_PCD_MspInit(PCD_HandleTypeDef *hpcd) {
    GPIO_InitTypeDef GPIO_InitStruct;
  
    if(hpcd->Instance == USB_OTG_FS) {
        /* Configure USB FS GPIOs */
        __HAL_RCC_GPIOA_CLK_ENABLE();
    
        /* Configure DM DP Pins */
        GPIO_InitStruct.Pin = (GPIO_PIN_11 | GPIO_PIN_12);
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF10_OTG_FS;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct); 
        
    	/* Configure VBUS Pin */
        GPIO_InitStruct.Pin = GPIO_PIN_9;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
	
        /* Configure ID pin */
        GPIO_InitStruct.Pin = GPIO_PIN_10;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Alternate = GPIO_AF10_OTG_FS;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct); 
    
        /* Enable USB FS Clocks */ 
        __HAL_RCC_USB_OTG_FS_CLK_ENABLE();
        
        /* Set USBFS Interrupt priority */
        HAL_NVIC_SetPriority(OTG_FS_IRQn, INT_PRIO_USBD_CORE, 0);
        
        /* Enable USBFS Interrupt */
        HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
    }
    else if(hpcd->Instance == USB_OTG_HS) {    
        /* Configure USB FS GPIOs */
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();
        __HAL_RCC_GPIOH_CLK_ENABLE();
        __HAL_RCC_GPIOI_CLK_ENABLE();
    
        /* CLK */
        GPIO_InitStruct.Pin = GPIO_PIN_5;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct); 
    
        /* D0 */
        GPIO_InitStruct.Pin = GPIO_PIN_3;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct); 
    
        /* D1 D2 D3 D4 D5 D6 D7 */
        GPIO_InitStruct.Pin = GPIO_PIN_0  | GPIO_PIN_1  | GPIO_PIN_5 |\
                              GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct); 
    
        /* STP */    
        GPIO_InitStruct.Pin = GPIO_PIN_0;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
        HAL_GPIO_Init(GPIOC, &GPIO_InitStruct); 
        
        /* NXT */ 
        GPIO_InitStruct.Pin = GPIO_PIN_4;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
        HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);   
    
        /* DIR */
        GPIO_InitStruct.Pin = GPIO_PIN_11;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
        HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);
    
        /* Enable USB HS Clocks */
        __HAL_RCC_USB_OTG_HS_CLK_ENABLE();
        __HAL_RCC_USB_OTG_HS_ULPI_CLK_ENABLE();
    
        /* Set USBHS Interrupt to the lowest priority */
        HAL_NVIC_SetPriority(OTG_HS_IRQn, INT_PRIO_USBD_CORE, 0);
        
        /* Enable USBHS Interrupt */
        HAL_NVIC_EnableIRQ(OTG_HS_IRQn);
    }   
}

/**
  * @brief  DeInitializes the PCD MSP.
  * @param  hpcd: PCD handle
  * @retval None
  */
void HAL_PCD_MspDeInit(PCD_HandleTypeDef *hpcd) {
    if(hpcd->Instance == USB_OTG_FS) {  
        /* Disable USB FS Clocks */ 
        __HAL_RCC_USB_OTG_FS_CLK_DISABLE();
        __HAL_RCC_SYSCFG_CLK_DISABLE(); 
    }
    else if(hpcd->Instance == USB_OTG_HS) {  
        /* Disable USB FS Clocks */ 
        __HAL_RCC_USB_OTG_HS_CLK_DISABLE();
        __HAL_RCC_SYSCFG_CLK_DISABLE(); 
    }  
}

/**
  * @brief  Setup stage callback.
  * @param  hpcd: PCD handle
  * @retval None
  */
void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd) {
    USBD_LL_SetupStage(hpcd->pData, (uint8_t *)hpcd->Setup);
}

/**
  * @brief  Data Out stage callback.
  * @param  hpcd: PCD handle
  * @param  epnum: Endpoint Number
  * @retval None
  */
void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum) {
    USBD_LL_DataOutStage(hpcd->pData, epnum, hpcd->OUT_ep[epnum].xfer_buff);
}

/**
  * @brief  Data In stage callback.
  * @param  hpcd: PCD handle
  * @param  epnum: Endpoint Number
  * @retval None
  */
void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum) {
    USBD_LL_DataInStage(hpcd->pData, epnum, hpcd->IN_ep[epnum].xfer_buff);
}

/**
  * @brief  SOF callback.
  * @param  hpcd: PCD handle
  * @retval None
  */
void HAL_PCD_SOFCallback(PCD_HandleTypeDef *hpcd) {
    USBD_LL_SOF(hpcd->pData);
}

/**
  * @brief  Reset callback.
  * @param  hpcd: PCD handle
  * @retval None
  */
void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd) { 
    USBD_SpeedTypeDef speed = USBD_SPEED_FULL;

    /* Set USB Current Speed */
    switch(hpcd->Init.speed) {
        case PCD_SPEED_HIGH:
            speed = USBD_SPEED_HIGH;
            break;    
        case PCD_SPEED_FULL:
            speed = USBD_SPEED_FULL;    
            break;	
        default:
            speed = USBD_SPEED_FULL;    
            break;
    }
    USBD_LL_SetSpeed(hpcd->pData, speed);  
  
    /* Reset Device */
    USBD_LL_Reset(hpcd->pData);
}

/**
  * @brief  Suspend callback.
  * @param  hpcd: PCD handle
  * @retval None
  */
void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *hpcd) {
    USBD_LL_Suspend(hpcd->pData);
}

/**
  * @brief  Resume callback.
  * @param  hpcd: PCD handle
  * @retval None
  */
void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *hpcd) {
    USBD_LL_Resume(hpcd->pData);
}

/**
  * @brief  ISOC Out Incomplete callback.
  * @param  hpcd: PCD handle
  * @param  epnum: Endpoint Number  
  * @retval None
  */
void HAL_PCD_ISOOUTIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum) {
    USBD_LL_IsoOUTIncomplete(hpcd->pData, epnum);
}

/**
  * @brief  ISOC In Incomplete callback.
  * @param  hpcd: PCD handle
  * @param  epnum: Endpoint Number  
  * @retval None
  */
void HAL_PCD_ISOINIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum) {
    USBD_LL_IsoINIncomplete(hpcd->pData, epnum);
}

/**
  * @brief  Connect callback.
  * @param  hpcd: PCD handle
  * @retval None
  */
void HAL_PCD_ConnectCallback(PCD_HandleTypeDef *hpcd) {
    USBD_LL_DevConnected(hpcd->pData);
}

/**
  * @brief  Disconnect callback.
  * @param  hpcd: PCD handle
  * @retval None
  */
void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *hpcd) {
    USBD_LL_DevDisconnected(hpcd->pData);
}

/**
  * @brief  Initializes the Low Level portion of the Device driver.  
  * @param  pdev: Device handle
  * @retval USBD Status
  */
USBD_StatusTypeDef  USBD_LL_Init (USBD_HandleTypeDef *pdev) { 
#ifdef USBD_USE_FS  // AK changed to support device and host mode
#warning USB device FS MODE
    /*Set LL Driver parameters */
    usbdev_handle.Instance = USB_OTG_FS;
    usbdev_handle.Init.dev_endpoints = 4; 
    usbdev_handle.Init.use_dedicated_ep1 = 0;
    usbdev_handle.Init.ep0_mps = 0x40;  
    usbdev_handle.Init.dma_enable = 0;
    usbdev_handle.Init.low_power_enable = 0;
    usbdev_handle.Init.phy_itface = PCD_PHY_EMBEDDED; 
//    usbdev_handle.Init.Sof_enable = 0;
    usbdev_handle.Init.Sof_enable = 1;
    usbdev_handle.Init.speed = PCD_SPEED_FULL;
    usbdev_handle.Init.vbus_sensing_enable = 1;
    /* Link The driver to the stack */
    usbdev_handle.pData = pdev;
    pdev->pData = &usbdev_handle;
    /*Initialize LL Driver */
    HAL_PCD_Init(&usbdev_handle);
  
    HAL_PCDEx_SetRxFiFo(&usbdev_handle, 0x80);
    HAL_PCDEx_SetTxFiFo(&usbdev_handle, 0, 0x40);
    HAL_PCDEx_SetTxFiFo(&usbdev_handle, 1, 0x80); 
#elif defined(USBD_USE_HS)  // AK changed to support device and host mode
#warning USB device HS MODE
    /*Set LL Driver parameters */
    usbdev_handle.Instance = USB_OTG_HS;
    usbdev_handle.Init.dev_endpoints = 6; 
    usbdev_handle.Init.use_dedicated_ep1 = 0;
    usbdev_handle.Init.ep0_mps = 0x40;
  
    /* Be aware that enabling USB-DMA mode will result in data being sent only by
     multiple of 4 packet sizes. This is due to the fact that USB-DMA does
     not allow sending data from non word-aligned addresses.
     For this specific application, it is advised to not enable this option
     unless required. */
    usbdev_handle.Init.dma_enable = 0;
  
    usbdev_handle.Init.low_power_enable = 0;
    usbdev_handle.Init.phy_itface = PCD_PHY_ULPI; 
    usbdev_handle.Init.Sof_enable = 0;
    usbdev_handle.Init.speed = PCD_SPEED_HIGH;
    usbdev_handle.Init.vbus_sensing_enable = 1;
    /* Link The driver to the stack */
    usbdev_handle.pData = pdev;
    pdev->pData = &usbdev_handle;
    /*Initialize LL Driver */
    HAL_PCD_Init(&usbdev_handle);
  
    HAL_PCDEx_SetRxFiFo(&usbdev_handle, 0x200);
    HAL_PCDEx_SetTxFiFo(&usbdev_handle, 0, 0x80);
    HAL_PCDEx_SetTxFiFo(&usbdev_handle, 1, 0x174); 
#else
#error USBD_USE_FS or USBD_USE_FS must be defined
#endif 
    return USBD_OK;
}

/**
  * @brief  De-Initializes the Low Level portion of the Device driver. 
  * @param  pdev: Device handle
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef *pdev) {
    HAL_PCD_DeInit(pdev->pData);
    return USBD_OK; 
}

/**
  * @brief  Starts the Low Level portion of the Device driver.    
  * @param  pdev: Device handle
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef *pdev) {
    HAL_PCD_Start(pdev->pData);
    return USBD_OK; 
}

/**
  * @brief  Stops the Low Level portion of the Device driver.
  * @param  pdev: Device handle
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef *pdev) {
    HAL_PCD_Stop(pdev->pData);
    return USBD_OK; 
}

/**
  * @brief  Opens an endpoint of the Low Level Driver. 
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number
  * @param  ep_type: Endpoint Type
  * @param  ep_mps: Endpoint Max Packet Size                 
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef *pdev, 
        uint8_t  ep_addr,                                      
        uint8_t  ep_type,
        uint16_t ep_mps) {
    HAL_PCD_EP_Open(pdev->pData, ep_addr, ep_mps, ep_type);
    return USBD_OK; 
}

/**
  * @brief  Closes an endpoint of the Low Level Driver.  
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number      
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr) {
    HAL_PCD_EP_Close(pdev->pData, ep_addr);
    return USBD_OK; 
}

/**
  * @brief  Flushes an endpoint of the Low Level Driver.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number      
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr) {
    HAL_PCD_EP_Flush(pdev->pData, ep_addr);
    return USBD_OK; 
}

/**
  * @brief  Sets a Stall condition on an endpoint of the Low Level Driver.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number      
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr) {
    HAL_PCD_EP_SetStall(pdev->pData, ep_addr);
    return USBD_OK; 
}

/**
  * @brief  Clears a Stall condition on an endpoint of the Low Level Driver.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number      
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr) {
    HAL_PCD_EP_ClrStall(pdev->pData, ep_addr);  
    return USBD_OK; 
}

/**
  * @brief  Returns Stall condition.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number      
  * @retval Stall (1: yes, 0: No)
  */
uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr) {
    PCD_HandleTypeDef *hpcd = pdev->pData; 
  
    if((ep_addr & 0x80) == 0x80) {
        return hpcd->IN_ep[ep_addr & 0x7F].is_stall; 
    }
    else {
        return hpcd->OUT_ep[ep_addr & 0x7F].is_stall; 
    }
}

/**
  * @brief  Assigns an USB address to the device
  * @param  pdev: Device handle
  * @param  dev_addr: USB address      
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef *pdev, uint8_t dev_addr) {
    HAL_PCD_SetAddress(pdev->pData, dev_addr);
    return USBD_OK; 
}

/**
  * @brief  Transmits data over an endpoint 
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number
  * @param  pbuf: Pointer to data to be sent    
  * @param  size: Data size    
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef *pdev, 
        uint8_t  ep_addr,                                      
        uint8_t  *pbuf,
        uint16_t  size) {
    HAL_PCD_EP_Transmit(pdev->pData, ep_addr, pbuf, size);
    return USBD_OK;   
}

/**
  * @brief  Prepares an endpoint for reception 
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number
  * @param  pbuf:pointer to data to be received    
  * @param  size: data size              
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef *pdev, 
        uint8_t  ep_addr,                                      
        uint8_t  *pbuf,
        uint16_t  size) {
    HAL_PCD_EP_Receive(pdev->pData, ep_addr, pbuf, size);
    return USBD_OK;   
}

/**
  * @brief  Returns the last transfered packet size.    
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number
  * @retval Recived Data Size
  */
uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef *pdev, uint8_t  ep_addr) {
    return HAL_PCD_EP_GetRxCount(pdev->pData, ep_addr);
}

/**
  * @brief  Delay routine for the USB Device Library      
  * @param  Delay: Delay in ms
  * @retval None
  */
void  USBD_LL_Delay(uint32_t Delay) {
    HAL_Delay(Delay);  
}


