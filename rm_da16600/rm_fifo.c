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

#include "rm_fifo.h"

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
void rm_fifo_init(rm_fifo_t * p_fifo, uint8_t ** p_buf, uint32_t size)
{
    p_fifo->p_buffer = p_buf;
    p_fifo->size     = size;
    p_fifo->head     = 0;
    p_fifo->tail     = 0;
    p_fifo->items    = 0;
}

/**********************************************************************************************************************
 *  Add an item to the head of the FIFO.
 *
 *  @retval TRUE                     Item added successfully.
 *  @retval FALSE                    Item not added, FIFO full.
 **********************************************************************************************************************/
bool rm_fifo_put(rm_fifo_t * p_fifo, uint8_t ** p_data)
{
    bool added = false;

    /* Data is discarded if the buffer is full.. */
    if (rm_fifo_full(p_fifo) == false)
    {
        p_fifo->p_buffer[p_fifo->head] = *p_data;

        if (++p_fifo->head >= p_fifo->size)
        {
            p_fifo->head = 0;
        }
        p_fifo->items++;

        added = true;
    }
    return added;
}

/**********************************************************************************************************************
 *  Get an item from the tail of the FIFO.
 *
 *  @retval TRUE                     Item removed successfully.
 *  @retval FALSE                    Item not removed, FIFO empty.
 **********************************************************************************************************************/
bool rm_fifo_get(rm_fifo_t * p_fifo, uint8_t ** p_data)
{
    bool removed = false;

    if (rm_fifo_empty(p_fifo) == false)
    {
        *p_data = p_fifo->p_buffer[p_fifo->tail];

        if (++p_fifo->tail >= p_fifo->size)
        {
            p_fifo->tail = 0;
        }
        p_fifo->items--;

        removed = true;
    }
    return removed;
}

/**********************************************************************************************************************
 *  Checks if FIFO is empty.
 *
 *  @retval TRUE                     FIFO is empty.
 *  @retval FALSE                    FIFO is not empty.
 **********************************************************************************************************************/
bool rm_fifo_empty(rm_fifo_t * p_fifo)
{
    return (p_fifo->items == 0);
}

/**********************************************************************************************************************
 *  Checks if FIFO is full.
 *
 *  @retval TRUE                     FIFO is full.
 *  @retval FALSE                    FIFO is not full.
 **********************************************************************************************************************/
bool rm_fifo_full(rm_fifo_t * p_fifo)
{
    return (p_fifo->items == p_fifo->size);
}
