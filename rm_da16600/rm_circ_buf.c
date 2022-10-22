/***********************************************************************************************************************
 * Copyright [2020-2022] Renesas Electronics Corporation and/or its affiliates.  All Rights Reserved.
 *
 * This software and documentation are supplied by Renesas Electronics America Inc. and may only be used with products
 * of Renesas Electronics Corp. and its affiliates ("Renesas").  No other uses are authorized.  Renesas products are
 * sold pursuant to Renesas terms and conditions of sale.  Purchasers are solely responsible for the selection and use
 * of Renesas products and Renesas assumes no liability.  No license, express or implied, to any intellectual property
 * right is granted by Renesas. This software is protected under all applicable laws, including copyright laws. Renesas
 * reserves the right to change or discontinue this software and/or this documentation. THE SOFTWARE AND DOCUMENTATION
 * IS DELIVERED TO YOU "AS IS," AND RENESAS MAKES NO REPRESENTATIONS OR WARRANTIES, AND TO THE FULLEST EXTENT
 * PERMISSIBLE UNDER APPLICABLE LAW, DISCLAIMS ALL WARRANTIES, WHETHER EXPLICITLY OR IMPLICITLY, INCLUDING WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NONINFRINGEMENT, WITH RESPECT TO THE SOFTWARE OR
 * DOCUMENTATION.  RENESAS SHALL HAVE NO LIABILITY ARISING OUT OF ANY SECURITY VULNERABILITY OR BREACH.  TO THE MAXIMUM
 * EXTENT PERMITTED BY LAW, IN NO EVENT WILL RENESAS BE LIABLE TO YOU IN CONNECTION WITH THE SOFTWARE OR DOCUMENTATION
 * (OR ANY PERSON OR ENTITY CLAIMING RIGHTS DERIVED FROM YOU) FOR ANY LOSS, DAMAGES, OR CLAIMS WHATSOEVER, INCLUDING,
 * WITHOUT LIMITATION, ANY DIRECT, CONSEQUENTIAL, SPECIAL, INDIRECT, PUNITIVE, OR INCIDENTAL DAMAGES; ANY LOST PROFITS,
 * OTHER ECONOMIC DAMAGE, PROPERTY DAMAGE, OR PERSONAL INJURY; AND EVEN IF RENESAS HAS BEEN ADVISED OF THE POSSIBILITY
 * OF SUCH LOSS, DAMAGES, CLAIMS OR COSTS.
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include <stdint.h>
#include <stdbool.h>

#include "rm_circ_buf.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private constants
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Enumerations
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/
/**********************************************************************************************************************
 *  Initialize FIFO.
 **********************************************************************************************************************/

/***********************************************************************************************************************
 *  Send and receive an AT command with testing for return.
 *
 * @param[in]  p_instance_ctrl      Pointer to array holding URL to query from DNS.
 * @param[in]  serial_ch_id           Pointer to IP address returned from look up.
 * @param[in]  p_textstring           Pointer to IP address returned from look up.
 * @param[in]  byte_timeout           Pointer to IP address returned from look up.
 * @param[in]  timeout_ms           Pointer to IP address returned from look up.
 * @param[in]  expect_code           Pointer to IP address returned from look up.
 *
 * @retval FSP_SUCCESS              Function completed successfully.
 * @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 * @retval FSP_ERR_ASSERTION        Assertion error occurred.
 **********************************************************************************************************************/
void rm_circ_buf_init(rm_circ_buf_t * p_cbuf, uint8_t *p_buf, uint32_t len)
{
    p_cbuf->p_buffer = p_buf;
    p_cbuf->size     = len;
    p_cbuf->head     = 0;
    p_cbuf->tail     = 0;
}

/***********************************************************************************************************************
 *  Send and receive an AT command with testing for return.
 *
 * @param[in]  p_instance_ctrl      Pointer to array holding URL to query from DNS.
 * @param[in]  serial_ch_id           Pointer to IP address returned from look up.
 * @param[in]  p_textstring           Pointer to IP address returned from look up.
 * @param[in]  byte_timeout           Pointer to IP address returned from look up.
 * @param[in]  timeout_ms           Pointer to IP address returned from look up.
 * @param[in]  expect_code           Pointer to IP address returned from look up.
 *
 * @retval FSP_SUCCESS              Function completed successfully.
 * @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 * @retval FSP_ERR_ASSERTION        Assertion error occurred.
 **********************************************************************************************************************/
bool rm_circ_buff_empty(rm_circ_buf_t * p_cbuf)
{
#if 0
    bool empty = false;

    if (p_cbuf->head == p_cbuf->tail)
    {
        empty = true;
    }

    return empty;
#else
    return (p_cbuf->head == p_cbuf->tail);
#endif
}

/***********************************************************************************************************************
 *  Send and receive an AT command with testing for return.
 *
 * @param[in]  p_instance_ctrl      Pointer to array holding URL to query from DNS.
 * @param[in]  serial_ch_id           Pointer to IP address returned from look up.
 * @param[in]  p_textstring           Pointer to IP address returned from look up.
 * @param[in]  byte_timeout           Pointer to IP address returned from look up.
 * @param[in]  timeout_ms           Pointer to IP address returned from look up.
 * @param[in]  expect_code           Pointer to IP address returned from look up.
 *
 * @retval FSP_SUCCESS              Function completed successfully.
 * @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 * @retval FSP_ERR_ASSERTION        Assertion error occurred.
 **********************************************************************************************************************/
bool rm_circ_buff_full(rm_circ_buf_t * p_cbuf)
{
    return (((p_cbuf->head + 1) % p_cbuf->size) == p_cbuf->tail);
}

/***********************************************************************************************************************
 *  Send and receive an AT command with testing for return.
 *
 * @param[in]  p_instance_ctrl      Pointer to array holding URL to query from DNS.
 * @param[in]  serial_ch_id           Pointer to IP address returned from look up.
 * @param[in]  p_textstring           Pointer to IP address returned from look up.
 * @param[in]  byte_timeout           Pointer to IP address returned from look up.
 * @param[in]  timeout_ms           Pointer to IP address returned from look up.
 * @param[in]  expect_code           Pointer to IP address returned from look up.
 *
 * @retval FSP_SUCCESS              Function completed successfully.
 * @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 * @retval FSP_ERR_ASSERTION        Assertion error occurred.
 **********************************************************************************************************************/
bool rm_circ_buf_put(rm_circ_buf_t * p_cbuf, uint8_t data)
{
    bool success = false;

    /* Data is discarded if the buffer is full.. */
    if (rm_circ_buff_full(p_cbuf) == false)
    {
        p_cbuf->p_buffer[p_cbuf->head] = data;

        if (++p_cbuf->head >= p_cbuf->size)
        {
            p_cbuf->head = 0;
        }
        success = true;
    }

    return success;
}

void rm_circ_buf_clear(rm_circ_buf_t * p_cbuf)
{
    p_cbuf->tail = p_cbuf->head = 0;
}

/***********************************************************************************************************************
 *  Send and receive an AT command with testing for return.
 *
 * @param[in]  p_instance_ctrl      Pointer to array holding URL to query from DNS.
 * @param[in]  serial_ch_id           Pointer to IP address returned from look up.
 * @param[in]  p_textstring           Pointer to IP address returned from look up.
 * @param[in]  byte_timeout           Pointer to IP address returned from look up.
 * @param[in]  timeout_ms           Pointer to IP address returned from look up.
 * @param[in]  expect_code           Pointer to IP address returned from look up.
 *
 * @retval FSP_SUCCESS              Function completed successfully.
 * @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 * @retval FSP_ERR_ASSERTION        Assertion error occurred.
 **********************************************************************************************************************/
bool rm_circ_buf_get(rm_circ_buf_t * p_cbuf, uint8_t * p_data)
{
    bool success = false;

    if (rm_circ_buff_empty(p_cbuf) == false)
    {
        *p_data = p_cbuf->p_buffer[p_cbuf->tail];

        if (++p_cbuf->tail >= p_cbuf->size)
        {
            p_cbuf->tail = 0;
        }
        success = true;
    }
    return success;
}
