/*
 * USB MIDI Host for STM32 - Config / Hardware Driver
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
#include "stm32f4xx_hal.h"
#include "usbh_core.h"
#include "usbh_midi.h"
#include "../config.h"

#define USBH_VBUS_CTRL_ON 0
#define USBH_VBUS_CTRL_OFF 1

// device handle
HCD_HandleTypeDef hhcd;

// set the USB VBUS power control state
void usbh_conf_set_vbus(int state) {
#ifdef USE_USB_FS     
    if(state == 0) {
        BSP_IO_WritePin(OTG_FS1_POWER_SWITCH_PIN, RESET);
    }
    else {
        BSP_IO_WritePin(OTG_FS1_POWER_SWITCH_PIN, SET);
    }
#endif
   
#ifdef USE_USB_HS_IN_FS
    if(state == 0) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, USBH_VBUS_CTRL_OFF);
    }
    else {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, USBH_VBUS_CTRL_ON);
    }
#endif    
}

/**
  * @brief  Initializes the HCD MSP.
  * @param  hhcd: HCD handle
  * @retval None
  */
void HAL_HCD_MspInit(HCD_HandleTypeDef *hhcd) {
    GPIO_InitTypeDef  GPIO_InitStruct;

    if(hhcd->Instance == USB_OTG_FS) {
        /* Configure USB FS GPIOs */
        __HAL_RCC_GPIOA_CLK_ENABLE();

        GPIO_InitStruct.Pin = (GPIO_PIN_11 | GPIO_PIN_12);
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF10_OTG_FS;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct); 

        GPIO_InitStruct.Pin = GPIO_PIN_10;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Alternate = GPIO_AF10_OTG_FS;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct); 

        /* Configure POWER_SWITCH IO pin */
        //    BSP_IO_ConfigPin(OTG_FS1_POWER_SWITCH_PIN, IO_MODE_OUTPUT);

        /* Enable USB FS Clocks */ 
        __HAL_RCC_USB_OTG_FS_CLK_ENABLE();

        /* Set USBFS Interrupt to the lowest priority */
        HAL_NVIC_SetPriority(OTG_FS_IRQn, INT_PRIO_USBH_CORE, 0);

        /* Enable USBFS Interrupt */
        HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
    } 
    else if(hhcd->Instance == USB_OTG_HS) {
#ifdef USE_USB_HS_IN_FS
//        /* Configure POWER_SWITCH IO pin */
//        BSP_IO_ConfigPin(OTG_FS2_POWER_SWITCH_PIN, IO_MODE_OUTPUT);

        __HAL_RCC_GPIOB_CLK_ENABLE();

        // power switch pin - active low
        GPIO_InitStruct.Pin = GPIO_PIN_12;
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, USBH_VBUS_CTRL_OFF);  // off by default
        
        /*Configure GPIO for HS on FS mode*/
//        GPIO_InitStruct.Pin = GPIO_PIN_12  | GPIO_PIN_13  | GPIO_PIN_14 | GPIO_PIN_15;
        GPIO_InitStruct.Pin = GPIO_PIN_13  | GPIO_PIN_14 | GPIO_PIN_15;  // pin 12 is PCTRL
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Alternate = GPIO_AF12_OTG_HS_FS;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);    
#else
        /* Configure USB FS GPIOs */
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();
        __HAL_RCC_GPIOH_CLK_ENABLE();        
        __HAL_RCC_GPIOI_CLK_ENABLE();
        
        /*CLK*/
        GPIO_InitStruct.Pin = GPIO_PIN_5;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct); 
        
        /*D0*/
        GPIO_InitStruct.Pin = GPIO_PIN_3;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct); 
        
        /*D1 D2 D3 D4 D5 D6 D7*/
        GPIO_InitStruct.Pin = GPIO_PIN_0  | GPIO_PIN_1  | GPIO_PIN_5 |\
          GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct); 
        
        /*STP*/     
        GPIO_InitStruct.Pin = GPIO_PIN_0;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
        HAL_GPIO_Init(GPIOC, &GPIO_InitStruct); 
        
        /*NXT*/ 
        GPIO_InitStruct.Pin = GPIO_PIN_4;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
        HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);   
        
        /*DIR*/
        GPIO_InitStruct.Pin = GPIO_PIN_11;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
        HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);     
        __HAL_RCC_USB_OTG_HS_ULPI_CLK_ENABLE();
#endif   
        /* Enable USB HS Clocks */ 
        __HAL_RCC_USB_OTG_HS_CLK_ENABLE();
        
        /* Set USBFS Interrupt to the lowest priority */
        HAL_NVIC_SetPriority(OTG_HS_IRQn, INT_PRIO_USBH_CORE, 0);
        
        /* Enable USBFS Interrupt */
        HAL_NVIC_EnableIRQ(OTG_HS_IRQn);
    }   
}

/**
  * @brief  DeInitializes the HCD MSP.
  * @param  hhcd: HCD handle
  * @retval None
  */
void HAL_HCD_MspDeInit(HCD_HandleTypeDef *hhcd) {
    if(hhcd->Instance == USB_OTG_FS) {  
        /* Disable USB FS Clocks */ 
        __HAL_RCC_USB_OTG_FS_CLK_DISABLE();
    
    }
    else if(hhcd->Instance == USB_OTG_HS) {  
        /* Disable USB HS Clocks */ 
        __HAL_RCC_USB_OTG_HS_CLK_DISABLE();
        __HAL_RCC_USB_OTG_HS_ULPI_CLK_DISABLE();    
    }
}

/*******************************************************************************
                       LL Driver Callbacks (HCD -> USB Host Library)
*******************************************************************************/
/**
  * @brief  SOF callback.
  * @param  hhcd: HCD handle
  * @retval None
  */
void HAL_HCD_SOF_Callback(HCD_HandleTypeDef *hhcd) {
    USBH_LL_IncTimer (hhcd->pData);
}

/**
  * @brief  Connect callback.
  * @param  hhcd: HCD handle
  * @retval None
  */
void HAL_HCD_Connect_Callback(HCD_HandleTypeDef *hhcd) {
    USBH_LL_Connect(hhcd->pData);
}

/**
  * @brief  Disconnect callback.
  * @param  hhcd: HCD handle
  * @retval None
  */
void HAL_HCD_Disconnect_Callback(HCD_HandleTypeDef *hhcd) {
    USBH_LL_Disconnect(hhcd->pData);
} 


/**
  * @brief  Notify URB state change callback.
  * @param  hhcd: HCD handle
  * @param  chnum: Channel number 
  * @param  urb_state: URB State
  * @retval None
  */
void HAL_HCD_HC_NotifyURBChange_Callback(HCD_HandleTypeDef *hhcd, uint8_t chnum, 
        HCD_URBStateTypeDef urb_state) {
}

/*******************************************************************************
                       LL Driver Interface (USB Host Library --> HCD)
*******************************************************************************/
/**
  * @brief  Initializes the Low Level portion of the Host driver.
  * @param  phost: Host handle
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_LL_Init(USBH_HandleTypeDef *phost) {
#ifdef USBH_USE_FS  // AK changed to support device and host mode
#warning USB host FS MODE
    /* Set the LL driver parameters */
    hhcd.Instance = USB_OTG_FS;
    hhcd.Init.Host_channels = 11; 
    hhcd.Init.dma_enable = 0;
    hhcd.Init.low_power_enable = 0;
    hhcd.Init.phy_itface = HCD_PHY_EMBEDDED; 
    hhcd.Init.Sof_enable = 0;
    hhcd.Init.speed = HCD_SPEED_FULL;
    /* Link the driver to the stack */
    hhcd.pData = phost;
    phost->pData = &hhcd;
    /* Initialize the LL Driver */
    HAL_HCD_Init(&hhcd);
#elif defined(USBH_USE_HS)  // AK changed to support device and host mode
#warning USB host HS MODE
    /* Set the LL driver parameters */
    hhcd.Instance = USB_OTG_HS;
    hhcd.Init.Host_channels = 11; 
    hhcd.Init.dma_enable = 0;
    hhcd.Init.low_power_enable = 0;
#ifdef USE_USB_HS_IN_FS
    hhcd.Init.phy_itface = HCD_PHY_EMBEDDED; 
#else  
    hhcd.Init.phy_itface = HCD_PHY_ULPI; 
#endif  
    hhcd.Init.Sof_enable = 0;
    hhcd.Init.speed = HCD_SPEED_HIGH;
    hhcd.Init.use_external_vbus = 1;  
    /* Link the driver to the stack */
    hhcd.pData = phost;
    phost->pData = &hhcd;
    /* Initialize the LL driver */
    HAL_HCD_Init(&hhcd);  
#endif
  USBH_LL_SetTimer(phost, HAL_HCD_GetCurrentFrame(&hhcd));
  return USBH_OK;
}


/**
  * @brief  De-Initializes the Low Level portion of the Host driver.
  * @param  phost: Host handle
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_LL_DeInit(USBH_HandleTypeDef *phost) {
    HAL_HCD_DeInit(phost->pData);
    return USBH_OK; 
}

/**
  * @brief  Starts the Low Level portion of the Host driver.   
  * @param  phost: Host handle
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_LL_Start(USBH_HandleTypeDef *phost) {
    HAL_HCD_Start(phost->pData);
    return USBH_OK; 
}

/**
  * @brief  Stops the Low Level portion of the Host driver.
  * @param  phost: Host handle
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_LL_Stop(USBH_HandleTypeDef *phost) {
    HAL_HCD_Stop(phost->pData);
    return USBH_OK; 
}

/**
  * @brief  Returns the USB Host Speed from the Low Level Driver.
  * @param  phost: Host handle
  * @retval USBH Speeds
  */
USBH_SpeedTypeDef USBH_LL_GetSpeed(USBH_HandleTypeDef *phost) {
    USBH_SpeedTypeDef speed = USBH_SPEED_FULL;
  
    switch (HAL_HCD_GetCurrentSpeed(phost->pData)) {
        case 0: 
            speed = USBH_SPEED_HIGH;
            break;
        case 1: 
            speed = USBH_SPEED_FULL;    
            break;    
        case 2: 
            speed = USBH_SPEED_LOW;   
            break;    
        default:  
            speed = USBH_SPEED_FULL;    
            break;    
    }
    return speed;
}

/**
  * @brief  Resets the Host Port of the Low Level Driver.
  * @param  phost: Host handle
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_LL_ResetPort (USBH_HandleTypeDef *phost) {
    HAL_HCD_ResetPort(phost->pData);
    return USBH_OK; 
}

/**
  * @brief  Returns the last transferred packet size.
  * @param  phost: Host handle
  * @param  pipe: Pipe index   
  * @retval Packet Size
  */
uint32_t USBH_LL_GetLastXferSize(USBH_HandleTypeDef *phost, uint8_t pipe) {
    return HAL_HCD_HC_GetXferCount(phost->pData, pipe);
}

/**
  * @brief  Opens a pipe of the Low Level Driver.
  * @param  phost: Host handle
  * @param  pipe: Pipe index
  * @param  epnum: Endpoint Number
  * @param  dev_address: Device USB address
  * @param  speed: Device Speed 
  * @param  ep_type: Endpoint Type
  * @param  mps: Endpoint Max Packet Size                 
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_LL_OpenPipe(USBH_HandleTypeDef *phost, 
                                    uint8_t pipe,
                                    uint8_t epnum,                                      
                                    uint8_t dev_address,
                                    uint8_t speed,
                                    uint8_t ep_type,
                                    uint16_t mps) {
    HAL_HCD_HC_Init(phost->pData,
                      pipe,
                      epnum,
                      dev_address,
                      speed,
                      ep_type,
                      mps);
    return USBH_OK; 
}

/**
  * @brief  Closes a pipe of the Low Level Driver.
  * @param  phost: Host handle
  * @param  pipe: Pipe index               
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_LL_ClosePipe(USBH_HandleTypeDef *phost, uint8_t pipe) {
    HAL_HCD_HC_Halt(phost->pData, pipe);
    return USBH_OK; 
}

/**
  * @brief  Submits a new URB to the low level driver.
  * @param  phost: Host handle
  * @param  pipe: Pipe index    
  *          This parameter can be a value from 1 to 15
  * @param  direction: Channel number
  *          This parameter can be one of these values:
  *           0: Output 
  *           1: Input
  * @param  ep_type: Endpoint Type
  *          This parameter can be one of these values:
  *            @arg EP_TYPE_CTRL: Control type
  *            @arg EP_TYPE_ISOC: Isochronous type
  *            @arg EP_TYPE_BULK: Bulk type
  *            @arg EP_TYPE_INTR: Interrupt type
  * @param  token: Endpoint Type
  *          This parameter can be one of these values:
  *            @arg 0: PID_SETUP
  *            @arg 1: PID_DATA
  * @param  pbuff: pointer to URB data
  * @param  length: length of URB data
  * @param  do_ping: activate do ping protocol (for high speed only)
  *          This parameter can be one of these values:
  *           0: do ping inactive 
  *           1: do ping active 
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_LL_SubmitURB(USBH_HandleTypeDef *phost, 
                                     uint8_t pipe, 
                                     uint8_t direction,
                                     uint8_t ep_type,  
                                     uint8_t token, 
                                     uint8_t* pbuff, 
                                     uint16_t length,
                                     uint8_t do_ping) {
    HAL_HCD_HC_SubmitRequest(phost->pData,
                           pipe, 
                           direction,
                           ep_type,  
                           token, 
                           pbuff, 
                           length,
                           do_ping);
    return USBH_OK;   
}

/**
  * @brief  Gets a URB state from the low level driver.
  * @param  phost: Host handle
  * @param  pipe: Pipe index
  *          This parameter can be a value from 1 to 15
  * @retval URB state
  *          This parameter can be one of these values:
  *            @arg URB_IDLE
  *            @arg URB_DONE
  *            @arg URB_NOTREADY
  *            @arg URB_NYET 
  *            @arg URB_ERROR  
  *            @arg URB_STALL      
  */
USBH_URBStateTypeDef USBH_LL_GetURBState(USBH_HandleTypeDef *phost, uint8_t pipe) {
    return (USBH_URBStateTypeDef)HAL_HCD_HC_GetURBState (phost->pData, pipe);
}

/**
  * @brief  Drives VBUS.
  * @param  phost: Host handle
  * @param  state: VBUS state
  *          This parameter can be one of these values:
  *           0: VBUS Active 
  *           1: VBUS Inactive
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_LL_DriverVBUS(USBH_HandleTypeDef *phost, uint8_t state) {
    usbh_conf_set_vbus(state);
    HAL_Delay(200);
    return USBH_OK;  
}

/**
  * @brief  Sets toggle for a pipe.
  * @param  phost: Host handle
  * @param  pipe: Pipe index   
  * @param  toggle: toggle (0/1)
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_LL_SetToggle(USBH_HandleTypeDef *phost, uint8_t pipe, uint8_t toggle)   
{
  if(hhcd.hc[pipe].ep_is_in)
  {
    hhcd.hc[pipe].toggle_in = toggle;
  }
  else
  {
    hhcd.hc[pipe].toggle_out = toggle;
  }
  return USBH_OK; 
}

/**
  * @brief  Returns the current toggle of a pipe.
  * @param  phost: Host handle
  * @param  pipe: Pipe index
  * @retval toggle (0/1)
  */
uint8_t USBH_LL_GetToggle(USBH_HandleTypeDef *phost, uint8_t pipe)   
{
  uint8_t toggle = 0;
  
  if(hhcd.hc[pipe].ep_is_in)
  {
    toggle = hhcd.hc[pipe].toggle_in;
  }
  else
  {
    toggle = hhcd.hc[pipe].toggle_out;
  }
  return toggle; 
}

/**
  * @brief  Delay routine for the USB Host Library
  * @param  Delay: Delay in ms
  * @retval None
  */
void USBH_Delay(uint32_t Delay)
{
  HAL_Delay(Delay);  
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
