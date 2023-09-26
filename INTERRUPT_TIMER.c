/**********************************************************************************************************************
 * \file Cpu0_Main.c
 * \copyright Copyright (C) Infineon Technologies AG 2019
 * 
 * Use of this file is subject to the terms of use agreed between (i) you or the company in which ordinary course of 
 * business you are acting and (ii) Infineon Technologies AG or its licensees. If and as long as no such terms of use
 * are agreed, use of this file is subject to following:
 * 
 * Boost Software License - Version 1.0 - August 17th, 2003
 * 
 * Permission is hereby granted, free of charge, to any person or organization obtaining a copy of the software and 
 * accompanying documentation covered by this license (the "Software") to use, reproduce, display, distribute, execute,
 * and transmit the Software, and to prepare derivative works of the Software, and to permit third-parties to whom the
 * Software is furnished to do so, all subject to the following:
 * 
 * The copyright notices in the Software and this entire statement, including the above license grant, this restriction
 * and the following disclaimer, must be included in all copies of the Software, in whole or in part, and all 
 * derivative works of the Software, unless such copies or derivative works are solely in the form of 
 * machine-executable object code generated by a source language processor.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT SHALL THE 
 * COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN 
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS 
 * IN THE SOFTWARE.
 *********************************************************************************************************************/
#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"

// FOR LED
#define PORT10_BASE     (0xF003B000)
#define PORT10_IOCR0    (*(volatile unsigned int*)(PORT10_BASE + 0x10))     /* PORT10_IOCR0의 주소를 이용한 주소의 내용 (변수 아님) */
#define PORT10_OMR      (*(volatile unsigned int*)(PORT10_BASE + 0x04))     /* PORT10_OMR의 주소를 이용한 주소의 내용 (변수 아님)   */
/* volatile 타입으로 선언하는 이유는 동일한 메모리에 1/0을 반복적으로 쓰기를 하므로 컴파일러가 동일한 동작을 무의미하게 반복한다고 판단하여 '최적화'를 실행할 수 있으므로 이를 방지하기 위하여 'volatile' 형으로 선언함 */

#define P10_R0_PC1      11
#define P10_R0_PC2      19
#define P10_R0_PCL1     17
#define P10_R0_PCL2     18
#define P10_R0_PS1      1
#define P10_R0_PS2      2

/* SCU Registers */
#define SCU_BASE            (0xF0036000)
#define SCU_WDT_CPU0CON0     (*(volatile unsigned int*)(SCU_BASE + 0x100))

#define LCK                 1
#define ENDINIT             0

/* SRC Registers */
#define SRC_BASE            (0xF0038000)
#define SRC_CCU60_SR0        (*(volatile unsigned int*)(SRC_BASE + 0x420))

#define TOS                 11
#define SRE                 10
#define SRPN                0

/* CCU60 Registers */
#define CCU60_BASE          (0xF0002A00)
#define CCU60_CLC           (*(volatile unsigned int*)(CCU60_BASE + 0x00))    // CCU60 모듈을 enable하기 위한 레지스터(CCU60_CLC)
#define CCU60_T12           (*(volatile unsigned int*)(CCU60_BASE + 0x20))    // T12 counter register를 통해 현재 카운트 값을 읽음.  counter를 사용하기 위해서 초기화(reset) 필요. 리셋값 0x00
#define CCU60_T12PR         (*(volatile unsigned int*)(CCU60_BASE + 0x24))    // T12 Period register에 period value값을 계산하여서 저장
#define CCU60_TCTR0         (*(volatile unsigned int*)(CCU60_BASE + 0x70))    //
#define CCU60_TCTR4         (*(volatile unsigned int*)(CCU60_BASE + 0x78))
#define CCU60_INP           (*(volatile unsigned int*)(CCU60_BASE + 0xAC))
#define CCU60_IEN           (*(volatile unsigned int*)(CCU60_BASE + 0xB0))

#define DISS                1
#define DISR                0
#define CTM                 7
#define T12PRE              3
#define T12CLK              0
#define T12STR              6
#define T12RS               1
#define INPT12              10
#define ENT12PM             7

IfxCpu_syncEvent g_cpuSyncEvent = 0;

/* Initialize LED (RED & BLUE) */
void init_LED(void){
    /* reset PC1 & PC2 in IOCR0 */
    PORT10_IOCR0 &= ~((0x1F) << P10_R0_PC1);
    PORT10_IOCR0 &= ~((0x1F) << P10_R0_PC2);

    /* set PC1 & PC2 with push-pull (2b10000) */
    PORT10_IOCR0 |= ((0x10) << P10_R0_PC1);
    PORT10_IOCR0 |= ((0x10) << P10_R0_PC2);
}

void unlock_Safety_Critical_Reg(void){
    /* Password Access to unlock WDTSCON0 */
    SCU_WDT_CPU0CON0 = ((SCU_WDT_CPU0CON0 ^ 0xFC) & ~(1 << LCK)) | (1 << ENDINIT);
    while((SCU_WDT_CPU0CON0 & (1 << LCK)) != 0);

    /* Modify Access to clear ENDINIT bit */
    SCU_WDT_CPU0CON0 = ((SCU_WDT_CPU0CON0 ^ 0xFC) | (1 << LCK)) & ~ (1 << ENDINIT);
    while((SCU_WDT_CPU0CON0 & (1 << LCK)) == 0);
}

void lock_Safety_Critical_Reg(void){
    /* Password Access to unlock WDTSCON0 */
    SCU_WDT_CPU0CON0 = ((SCU_WDT_CPU0CON0 ^ 0xFC) & ~(1 << LCK)) | (1 << ENDINIT);
    while((SCU_WDT_CPU0CON0 & (1 << LCK)) != 0);

    /* Modify Access to clear ENDINIT bit */
    SCU_WDT_CPU0CON0 = ((SCU_WDT_CPU0CON0 ^ 0xFC) | (1 << LCK)) | (1 << ENDINIT);
    while((SCU_WDT_CPU0CON0 & (1 << LCK)) == 0);
}

void init_CCU60(void)
{
    /* CCU60 T12 Setting */

    unlock_Safety_Critical_Reg();

    CCU60_CLC &= ~(1 << DISR);                     // Enable CCU60 Module

    lock_Safety_Critical_Reg();

    while((CCU60_CLC & (1 << DISS)) != 0);       // Wait until module is enabled

    CCU60_TCTR0 &= ~((0x7) << T12CLK);           // f_T12 = f_CCU6 / prescaler
    CCU60_TCTR0 |= ((0x3) << T12CLK);            // F_CCU6 = 100 MHz, prescaler = 2048
    CCU60_TCTR0 |= (1 << T12PRE);                // f_T12 = 48828 Hz

    CCU60_TCTR0 &= ~(1 << CTM);                  // T12 always counts up and continues counting
                                                 // from zero after reaching the period value

    CCU60_T12PR = 24414 - 1;                     // Interrupt freq. = f_T12 / (period value  + 1)
    CCU60_TCTR4 |= (1 << T12STR);                // Interrupt freq. = 2 Hz

    CCU60_T12 = 0;                               // Clear T12 counting value

    /* CCU60 T12 Interrupt Setting */
    CCU60_INP &= ~((0x3) << INPT12);             // Service Request output SR0 is selected

    CCU60_IEN |= (1 << ENT12PM);                 // Enable Interrupt for T12 Period-Match

    /* SRC Interrupt Setting For CCU60 */
    SRC_CCU60_SR0 &= ~((0xFF) << SRPN);           // Set Priority : 0x0A
    SRC_CCU60_SR0 |= ((0x0A) << SRPN);

    SRC_CCU60_SR0 &= ~((0x3) << TOS);             // CPU0 services

    SRC_CCU60_SR0 |= (1 << SRE);                  // Service Request is enabled

    /* CCU60 T12 Start */
    CCU60_TCTR4 = (1 << T12RS);                  // T12 starts counting
}


int core0_main(void)
{
    IfxCpu_enableInterrupts();

    /* !!WATCHDOG0 AND SAFETY WATCHDOG ARE DISABLED HERE!!
     * Enable the watchdogs and service them periodically if it is required
     */
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());

    /* Wait for CPU sync event */
    IfxCpu_emitEvent(&g_cpuSyncEvent);
    IfxCpu_waitEvent(&g_cpuSyncEvent, 1);

    init_LED();
    init_CCU60();

    PORT10_OMR |= ((1<<P10_R0_PCL1) | (1<<P10_R0_PS1) | (0<<P10_R0_PS2) | (0<<P10_R0_PCL2));
    // Blue와 Red가 번갈아가면서 깜빡이도록 하기 위해 메인함수에서 blue,red의 초기 조건을 반대로 세팅 후 인터럽트에서 toggle

    while(1)
    {
    }
    return (1);
}

__interrupt( 0x0A ) __vector_table( 0 )
void CCU60_T12_ISR(void)
{

    PORT10_OMR |= ((1<<P10_R0_PCL1) | (1<<P10_R0_PS1) | (1<<P10_R0_PS2) | (1<<P10_R0_PCL2));           // Toggle LED RED

}


