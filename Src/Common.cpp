#pragma once
#include "Common.h"
#include <shellscalingapi.h>
#include <Psapi.h>
#include <commdlg.h>
#include <ShlObj_core.h>
#include <comutil.h>



uint32_t bitScanReverse(uint32_t a)
{
    unsigned long index;
    _BitScanReverse(&index, a);
    return (uint32_t)index;
}