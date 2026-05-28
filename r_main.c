/***********************************************************************************************************************
* DISCLAIMER
* This software is supplied by Renesas Electronics Corporation and is only intended for use with Renesas products.
* No other uses are authorized. This software is owned by Renesas Electronics Corporation and is protected under all
* applicable laws, including copyright laws. 
* THIS SOFTWARE IS PROVIDED "AS IS" AND RENESAS MAKES NO WARRANTIES REGARDING THIS SOFTWARE, WHETHER EXPRESS, IMPLIED
* OR STATUTORY, INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
* NON-INFRINGEMENT.  ALL SUCH WARRANTIES ARE EXPRESSLY DISCLAIMED.TO THE MAXIMUM EXTENT PERMITTED NOT PROHIBITED BY
* LAW, NEITHER RENESAS ELECTRONICS CORPORATION NOR ANY OF ITS AFFILIATED COMPANIES SHALL BE LIABLE FOR ANY DIRECT,
* INDIRECT, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES FOR ANY REASON RELATED TO THIS SOFTWARE, EVEN IF RENESAS OR
* ITS AFFILIATES HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
* Renesas reserves the right, without notice, to make changes to this software and to discontinue the availability 
* of this software. By using this software, you agree to the additional terms and conditions found by accessing the 
* following link:
* http://www.renesas.com/disclaimer
*
* Copyright (C) 2011, 2023 Renesas Electronics Corporation. All rights reserved.
***********************************************************************************************************************/

/***********************************************************************************************************************
* File Name    : r_main.c
* Version      : CodeGenerator for RL78/G13 V2.05.07.02 [17 Nov 2023]
* Device(s)    : R5F1007E
* Tool-Chain   : CCRL
* Description  : This file implements main function.
* Creation Date: 2026/05/28
***********************************************************************************************************************/

/***********************************************************************************************************************
Includes
***********************************************************************************************************************/
#include "r_cg_macrodriver.h"
#include "r_cg_cgc.h"
#include "r_cg_port.h"
#include "r_cg_adc.h"
#include "r_cg_timer.h"
#include "r_cg_wdt.h"
/* Start user code for include. Do not edit comment generated here */
/* 状態定義用の列挙型 */
typedef enum {
    STATE_NORMAL,   /* 通常状態 */
    STATE_ALERT     /* P147電圧異常によるアラート状態 */
} SYSTEM_MODE;

/* グローバル変数 */
volatile unsigned int g_timer_1ms = 0; /* 汎用1msカウンタ */
SYSTEM_MODE g_system_mode = STATE_NORMAL;
/* End user code. Do not edit comment generated here */
#include "r_cg_userdefine.h"

/***********************************************************************************************************************
Pragma directive
***********************************************************************************************************************/
/* Start user code for pragma. Do not edit comment generated here */
/* End user code. Do not edit comment generated here */

/***********************************************************************************************************************
Global variables and functions
***********************************************************************************************************************/
/* Start user code for global. Do not edit comment generated here */
/* 外部グローバル変数の宣言 */
extern volatile unsigned int g_timer_1ms;
extern SYSTEM_MODE g_system_mode;

/* プロトタイプ宣言 */
unsigned char check_sw1_click(void);
unsigned char check_sw2_click(void);
unsigned int read_adc_p147(void);
/* End user code. Do not edit comment generated here */
void R_MAIN_UserInit(void);

/***********************************************************************************************************************
* Function Name: main
* Description  : This function implements main function.
* Arguments    : None
* Return Value : None
***********************************************************************************************************************/
void main(void)
{
    /* 各種状態管理変数 */
    unsigned char sw1_state = 0;  /* 0~3の4状態 */
    unsigned char sw2_state = 0;  /* 0~1の2状態 */
    
    unsigned int p16_high_duration = 0; /* P16がHになってからの時間(ms) */
    unsigned int alert_blink_timer = 0; /* アラート時のLED点滅用タイマ */
    
    unsigned char prev_p16 = 0;   /* 前回ループ時のP16の状態 */
    unsigned int adc_val = 0;
    
    R_MAIN_UserInit();

    /* Start user code. Do not edit comment generated here */
    /* A/Dコンバータ初期化＆タイマスタート */
    R_ADC_Set_OperationOn();
    R_TAU0_Channel0_Start();
    while (1U)
    {
	R_WDT_Restart();
        /* --- 1. アラート（異常検出）状態の処理 --- */
        if (g_system_mode == STATE_ALERT)
        {
            /* P30(SW1)が押されて離されたら通常状態に復帰 */
            if (check_sw1_click())
            {
                /* 初期状態へリセット */
                P0_bit.no0 = 1; /* P00=H */
                P0_bit.no1 = 1; /* P01=H */
                sw1_state = 0;
                
                g_system_mode = STATE_NORMAL;
            }
            else
            {
                /* 100ms間隔でP01(LED赤)を反転、P00(LED緑)はH固定 */
                if (g_timer_1ms >= 100)
                {
                    g_timer_1ms = 0;
                    P0_bit.no0 = 1;          /* P00=H 念のため固定 */
                    P0_bit.no1 = ~P0_bit.no1; /* P01反転 */
                }
            }
            continue; /* アラート中は以下の通常処理をスキップ */
        }

        /* --- 2. 通常状態の処理 --- */
        
        /* SW1 (P30) の押下判定 */
        if (check_sw1_click())
        {
            sw1_state = (sw1_state + 1) % 4;
            switch (sw1_state)
            {
                case 0: P0_bit.no0 = 1; P0_bit.no1 = 1; break; /* 1. P00=H, P01=H */
                case 1: P0_bit.no0 = 0; P0_bit.no1 = 1; break; /* 2. P00=L, P01=H */
                case 2: P0_bit.no0 = 0; P0_bit.no1 = 0; break; /* 3. P00=L, P01=L */
                case 3: P0_bit.no0 = 1; P0_bit.no1 = 0; break; /* 4. P00=H, P01=L */
            }
        }

        /* SW2 (P122) の押下判定 */
        if (check_sw2_click())
        {
            sw2_state = (sw2_state + 1) % 2;
            switch (sw2_state)
            {
                case 0: P1_bit.no6 = 0; P1_bit.no7 = 0; break; /* 1. P16=L, P17=L */
                case 1: P1_bit.no6 = 1; P1_bit.no7 = 0; break; /* 2. P16=H, P17=L */
            }
        }

        /* --- 3. P16(OUT1)の状態監視とP147(ADC)の電圧監視 --- */
        
        /* P16がLからHに立ち上がった瞬間を検知 */
        if (P1_bit.no6 == 1 && prev_p16 == 0)
        {
            g_timer_1ms = 0;       /* タイマクリア */
            p16_high_duration = 0;
        }
        
        /* P16がHの間の時間計測 */
        if (P1_bit.no6 == 1)
        {
            p16_high_duration += g_timer_1ms;
            g_timer_1ms = 0; /* 累積したらクリア */
            
            /* 100ms以降、電圧監視を開始 */
            if (p16_high_duration >= 100)
            {
                adc_val = read_adc_p147();
                
                /* 2.0V以上 (10bit ADCで 409以上) */
                if (adc_val >= 409)
                {
                    /* 1. 出力をクリア */
                    P1_bit.no6 = 0; /* P16=L */
                    P1_bit.no7 = 0; /* P17=L */
                    sw2_state = 0;  /* SW2の状態も初期状態へ */
                    
                    /* 2. アラート状態へ移行 */
                    g_system_mode = STATE_ALERT;
                    g_timer_1ms = 0;
                }
            }
        }
        else
        {
            p16_high_duration = 0;
        }
        
        prev_p16 = P1_bit.no6; /* 次回ループ用に状態保存 */
    }
}

/* * SW1(P30) チャタリング防止付きクリック判定関数
 * 戻り値: 1=押して離された, 0=変化なし
 */
unsigned char check_sw1_click(void)
{
    static unsigned char sw_history = 0xFF;
    static unsigned char pressed = 0;
    
    /* 簡易チャタリング防止：定期的に値をシフトして読み込み */
    /* 本来はタイマ同期が望ましいが、メインループの周期が十分速いため簡易的に処理 */
    sw_history = (sw_history << 1) | P3_bit.no0;
    
    /* 8回連続でLなら「確実に押されている」と判断 */
    if (sw_history == 0x00)
    {
        pressed = 1;
    }
    /* 8回連続でHなら「確実に離されている」と判断 */
    else if (sw_history == 0xFF)
    {
        if (pressed == 1)
        {
            pressed = 0;
            return 1; /* 押されて、今離された瞬間 */
        }
    }
    return 0;
}

/* * SW2(P122) チャタリング防止付きクリック判定関数
 * 戻り値: 1=押して離された, 0=変化なし
 */
unsigned char check_sw2_click(void)
{
    static unsigned char sw_history = 0xFF;
    static unsigned char pressed = 0;
    
    sw_history = (sw_history << 1) | P12_bit.no2;
    
    if (sw_history == 0x00)
    {
        pressed = 1;
    }
    else if (sw_history == 0xFF)
    {
        if (pressed == 1)
        {
            pressed = 0;
            return 1;
        }
    }
    return 0;
}

/*
 * P147 (ANI18) A/D変換実行関数
 * 戻り値: 10ビットの変換結果(0~1023)
 */
unsigned int read_adc_p147(void)
{
    /* 💡 変数の宣言はすべて関数の先頭（最初）にまとめる */
    unsigned int adc_result = 0;
    unsigned long timeout = 0;
    
    /* --- ここから下に実行文を書く --- */
    
    /* 1. A/Dコンバータの回路電源を強制的にONにする */
    ADCEN = 1U;   /* 周辺リバースレジスタ0 (PER0) のADC給電をON */
    ADIF = 0;     /* 念のためフラグをクリア */
    
    /* 2. 変換チャネルを ANI18 (P147) に指定 */
    ADS = 0x12U; 
    
    /* 3. A/D変換動作を開始させる */
    ADCS = 1U;    /* 変換開始 */
    
    /* 4. 変換終了（ADIF == 1）を待つ */
    while (ADIF == 0)
    {
        R_WDT_Restart(); /* ウォッチドッグタイマクリア */
        
        timeout++;
        if (timeout > 100000UL) 
        {
            /* もしハードウェア原因で変換が始まらなくても、強制脱出してフリーズを防ぐ */
            ADCS = 0U;
            return 0; 
        }
    }
    
    /* 5. 変換動作を終了して結果を取得 */
    ADCS = 0U;    /* 変換停止 */
    ADIF = 0;     /* フラグクリア */
    
    /* 変換結果の取得（右詰め10ビットを想定） */
    adc_result = ADCR >> 6; 
    
    return adc_result;
}

/***********************************************************************************************************************
* Function Name: R_MAIN_UserInit
* Description  : This function adds user code before implementing main function.
* Arguments    : None
* Return Value : None
***********************************************************************************************************************/
void R_MAIN_UserInit(void)
{
    /* Start user code. Do not edit comment generated here */
    EI();
    /* End user code. Do not edit comment generated here */
}

/* Start user code for adding. Do not edit comment generated here */
/* End user code. Do not edit comment generated here */
