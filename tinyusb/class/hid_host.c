/*
 * hid_host.c
 *
 *  Created on: Dec 20, 2012
 *      Author: hathach
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2013, hathach (tinyusb.org)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the tiny usb stack.
 */

#include "tusb_option.h"

#if (MODE_HOST_SUPPORTED && defined HOST_CLASS_HID)

#define _TINY_USB_SOURCE_FILE_
//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "common/common.h"
#include "hid_host.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// KEYBOARD
//--------------------------------------------------------------------+
#if TUSB_CFG_HOST_HID_KEYBOARD

STATIC_ hidh_keyboard_info_t keyboard_data[TUSB_CFG_HOST_DEVICE_MAX]; // does not have addr0, index = dev_address-1

static inline hidh_keyboard_info_t* get_kbd_data(uint8_t dev_addr) ATTR_PURE ATTR_ALWAYS_INLINE ATTR_WARN_UNUSED_RESULT;
static inline hidh_keyboard_info_t* get_kbd_data(uint8_t dev_addr)
{
  return &keyboard_data[dev_addr-1];
}
//------------- KEYBOARD PUBLIC API (parameter validation required) -------------//
bool  tusbh_hid_keyboard_is_supported(uint8_t dev_addr)
{
  return tusbh_device_is_configured(dev_addr) && pipehandle_is_valid(keyboard_data[dev_addr-1].pipe_hdl);
}

tusb_error_t tusbh_hid_keyboard_get_report(uint8_t dev_addr, uint8_t instance_num, uint8_t * const report)
{
  //------------- parameters validation -------------//
  ASSERT_INT(TUSB_DEVICE_STATE_CONFIGURED, tusbh_device_get_state(dev_addr), TUSB_ERROR_DEVICE_NOT_READY);
  ASSERT_PTR(report, TUSB_ERROR_INVALID_PARA);

  (void) instance_num;

  hidh_keyboard_info_t *p_keyboard = get_kbd_data(dev_addr);

  ASSERT(TUSB_INTERFACE_STATUS_BUSY != p_keyboard->status, TUSB_ERROR_INTERFACE_IS_BUSY);

  // TODO abstract to use hidh service
  ASSERT_STATUS( hcd_pipe_xfer(p_keyboard->pipe_hdl, report, p_keyboard->report_size, true) ) ;

  p_keyboard->status = TUSB_INTERFACE_STATUS_BUSY;

  return TUSB_ERROR_NONE;
}

tusb_interface_status_t tusbh_hid_keyboard_status(uint8_t dev_addr, uint8_t instance_num)
{
  return tusbh_device_get_state(dev_addr) ? keyboard_data[dev_addr-1].status : TUSB_INTERFACE_STATUS_INVALID_PARA;
}
//------------- Internal API -------------//
static inline tusb_error_t hidh_keyboard_open(uint8_t dev_addr, tusb_descriptor_endpoint_t const *p_endpoint_desc) ATTR_ALWAYS_INLINE;
static inline tusb_error_t hidh_keyboard_open(uint8_t dev_addr, tusb_descriptor_endpoint_t const *p_endpoint_desc)
{
  hidh_keyboard_info_t *p_keyboard = get_kbd_data(dev_addr);

  p_keyboard->pipe_hdl    = hcd_pipe_open(dev_addr, p_endpoint_desc, TUSB_CLASS_HID);
  p_keyboard->report_size = p_endpoint_desc->wMaxPacketSize.size; // TODO get size from report descriptor

  ASSERT (pipehandle_is_valid(p_keyboard->pipe_hdl), TUSB_ERROR_HCD_FAILED);

  return TUSB_ERROR_NONE;
}

static inline void hidh_keyboard_close(uint8_t dev_addr) ATTR_ALWAYS_INLINE;
static inline void hidh_keyboard_close(uint8_t dev_addr)
{
  pipe_handle_t pipe_hdl = keyboard_data[dev_addr-1].pipe_hdl;
  if ( pipehandle_is_valid(pipe_hdl) )
  {
    memclr_(&keyboard_data[dev_addr-1], sizeof(hidh_keyboard_info_t));
    ASSERT_INT( TUSB_ERROR_NONE,  hcd_pipe_close(pipe_hdl), (void) 0 );
  }
}

#endif

//--------------------------------------------------------------------+
// CLASS-USBD API (don't require to verify parameters)
//--------------------------------------------------------------------+
void hidh_init(void)
{
#if TUSB_CFG_HOST_HID_KEYBOARD
  memclr_(&keyboard_data, sizeof(hidh_keyboard_info_t)*TUSB_CFG_HOST_DEVICE_MAX);
#endif

#if TUSB_CFG_HOST_HID_MOUSE
  hidh_mouse_init();
#endif

#if TUSB_CFG_HOST_HID_GENERIC
  hidh_generic_init();
#endif
}

tusb_error_t hidh_open_subtask(uint8_t dev_addr, tusb_descriptor_interface_t const *p_interface_desc, uint16_t *p_length)
{
  uint8_t const *p_desc = (uint8_t const *) p_interface_desc;

  //------------- HID descriptor -------------//
  p_desc += p_desc[DESCRIPTOR_OFFSET_LENGTH];
  tusb_hid_descriptor_hid_t const *p_desc_hid = (tusb_hid_descriptor_hid_t const *) p_desc;
  ASSERT_INT(HID_DESC_HID, p_desc_hid->bDescriptorType, TUSB_ERROR_INVALID_PARA);

  //------------- TODO skip Get Report Descriptor -------------//
  uint8_t *p_report_desc = NULL; // report descriptor has to be global & in USB RAM

  //------------- Endpoint Descriptor -------------//
  p_desc += p_desc[DESCRIPTOR_OFFSET_LENGTH];
  ASSERT_INT(TUSB_DESC_ENDPOINT, p_desc[DESCRIPTOR_OFFSET_TYPE], TUSB_ERROR_INVALID_PARA);

  if (p_interface_desc->bInterfaceSubClass == HID_SUBCLASS_BOOT)
  {
    switch(p_interface_desc->bInterfaceProtocol)
    {
      #if TUSB_CFG_HOST_HID_KEYBOARD
      case HID_PROTOCOL_KEYBOARD:
        ASSERT_STATUS ( hidh_keyboard_open(dev_addr, (tusb_descriptor_endpoint_t const *) p_desc) );
      break;
      #endif

      #if TUSB_CFG_HOST_HID_MOUSE
      case HID_PROTOCOL_MOUSE:
        ASSERT_STATUS ( hidh_keyboard_open(dev_addr, p_desc, p_length) );
      break;
      #endif

      default: // unknown protocol --> skip this interface
        return TUSB_ERROR_NONE;
    }
    *p_length = sizeof(tusb_descriptor_interface_t) + sizeof(tusb_hid_descriptor_hid_t) + sizeof(tusb_descriptor_endpoint_t);
  }else
  {
    // open generic
    *p_length = 0;
  }

  return TUSB_ERROR_NONE;
}

void hidh_isr(pipe_handle_t pipe_hdl, tusb_bus_event_t event)
{
#if TUSB_CFG_HOST_HID_KEYBOARD
  if ( pipehandle_is_equal(pipe_hdl, keyboard_data[pipe_hdl.dev_addr-1].pipe_hdl) )
  {
    keyboard_data[pipe_hdl.dev_addr-1].status = (event == BUS_EVENT_XFER_COMPLETE) ? TUSB_INTERFACE_STATUS_COMPLETE : TUSB_INTERFACE_STATUS_ERROR;
    return;
  }
#endif

#if TUSB_CFG_HOST_HID_MOUSE

#endif

#if TUSB_CFG_HOST_HID_GENERIC

#endif
}

void hidh_close(uint8_t dev_addr)
{
#if TUSB_CFG_HOST_HID_KEYBOARD
  hidh_keyboard_close(dev_addr);
#endif

#if TUSB_CFG_HOST_HID_MOUSE
  hidh_mouse_close(dev_addr);
#endif

#if TUSB_CFG_HOST_HID_GENERIC
  hidh_generic_close(dev_addr);
#endif
}



#endif
