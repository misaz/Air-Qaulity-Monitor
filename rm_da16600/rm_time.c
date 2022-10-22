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

#include "r_timer_api.h"
#include "rm_time.h"
#include "hal_data.h"

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
static volatile uint32_t g_timer_ticks = 0;
static bool g_initialized = false;

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/
/**********************************************************************************************************************
 *  Initialize time module.
 **********************************************************************************************************************/
void rm_time_init(void)
{
    if (false == g_initialized)
    {
        fsp_err_t fsp_err;
        fsp_err = g_timer.p_api->open(g_timer.p_ctrl, g_timer.p_cfg);
        assert(FSP_SUCCESS == fsp_err);
        fsp_err = g_timer.p_api->start(g_timer.p_ctrl);
        assert(FSP_SUCCESS == fsp_err);
        g_timer_ticks = 0;
        g_initialized = true;
    }
}

/**********************************************************************************************************************
 *  Timer interrupt callback.
 *
 *  @param[in]  p_args       Pointer to data structure containing callback arguments.
 **********************************************************************************************************************/
void rm_time_tick_callback(timer_callback_args_t *p_args)
{
    FSP_PARAMETER_NOT_USED(p_args);

    g_timer_ticks++;
}

/**********************************************************************************************************************
 *  Gets current value of tick timer.
 *
 *  @retval Current value of tick timer.
 **********************************************************************************************************************/
uint16_t rm_time_now_ms(void)
{
    return (uint16_t)g_timer_ticks;
}

/**********************************************************************************************************************
 *  Returns elapsed number of ticks since reference time.
 *
 *  @param[in]  ref_time   Reference time that will be compared to current value of tick timer.
 *
 *  @retval Difference between ref_time and current tick timer value.
 **********************************************************************************************************************/
uint16_t rm_time_elapsed_since_ms(uint16_t ref_time)
{
    uint16_t elapsed_time;

    if (g_timer_ticks >= ref_time)
    {
        elapsed_time = (uint16_t)(g_timer_ticks - ref_time);
    }
    else
    {
        elapsed_time = (uint16_t)((0xFFFFFFFFUL - ref_time) + g_timer_ticks);
    }
    return(elapsed_time);
}
