#ifndef PRINT_ARRAY_H
#define PRINT_ARRAY_H

#include <stdint.h>
#include "SEGGER_RTT.h"

static void print_array(uint8_t *p_data, uint32_t size)
{
    // Helper table for pretty printing
    uint8_t ascii_table[] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};

    for (uint32_t i = 0; i < size; i++)
    {
        uint8_t h,l;
        SEGGER_RTT_WriteString(0, "0x");
        h = ascii_table[((p_data[i] & 0xF0)>>4)];
        l = ascii_table[(p_data[i] & 0x0F)];
        SEGGER_RTT_Write(0, (char *)&h, 1);
        SEGGER_RTT_Write(0, (char *)&l, 1);
        SEGGER_RTT_WriteString(0, " ");
    }
    SEGGER_RTT_WriteString(0, "\r\n");
}

#endif /*PRINT_ARRAY_H*/
