//*****************************************************************************
//
// Copyright: 2019-2021, 上海交通大学工程实践与科技创新III-A教学组
// File name: dac_demo.c
// Description: 
//    1.本代码可用于初步检查DAC6571芯片功能是否正常，PL0须连线DAC6571之SDA，PL1连线SCL；
//    2.开机或复位后，DAC编码置为（十进制）1023，底板上右边4位数码管显示该数值；
//    3.由1号和4号键分别控制（十进制）DAC编码其加100和减100；
//    4.由2号和5号键分别控制（十进制）DAC编码其加10和减10；
//    5.由3号和6号键分别控制（十进制）DAC编码其加1和减1；
//    6.代码脱胎于课程初始DEMO程序，所以部分保留了它的代码功能或痕迹；
// Author:	上海交通大学工程实践与科技创新III-A教学组（孟、袁）
// Version: 1.1.0.20210930 
// Date：2021-9-30
// History：2021-9-31修改完善注释（袁）
//
//*****************************************************************************

//*****************************************************************************
//
// 头文件
//
//*****************************************************************************
#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_memmap.h"        // 基址宏定义
#include "inc/hw_types.h"         // 数据类型宏定义，寄存器访问函数
#include "driverlib/debug.h"      // 调试用
#include "driverlib/gpio.h"       // 通用IO口宏定义
#include "driverlib/pin_map.h"    // TM4C系列MCU外围设备管脚宏定义
#include "driverlib/sysctl.h"     // 系统控制定义
#include "driverlib/systick.h"    // SysTick Driver 原型
#include "driverlib/interrupt.h"  // NVIC Interrupt Controller Driver 原型
#include "driverlib/adc.h"        // ADC相关的模块 

#include "tm1638.h"               // 与控制TM1638芯片有关的函数
#include "DAC6571.h"              // 与控制DAC6571芯片有关的函数

//*****************************************************************************
//
// 宏定义
//
//*****************************************************************************
#define SYSTICK_FREQUENCY		50		// SysTick频率为50Hz，即循环定时周期20ms

#define V_T40ms 2 							 // 0.04s软件定时器溢出值，2个20ms
#define V_T60ms 4
#define V_T100ms	5              // 0.1s软件定时器溢出值，5个20ms
#define V_T500ms	25             // 0.5s软件定时器溢出值，25个20ms
#define V_T240ms 12 
#define V_T400ms 120 // 1.6s
#define DAC_set_current_max 1001
#define current_display_error 5
//*****************************************************************************
//
// 函数原型声明
//
//*****************************************************************************
void GPIOInit(void);        // GPIO初始化
void SysTickInit(void);     // 设置SysTick中断 
void DevicesInit(void);     // MCU器件初始化，注：会调用上述函数
void ADCInit(void);			// ADC初始化
void ADC_Sample(void);  // ADC采样
void ADC_Sample_1(void);  // ADC
void StateMachine(void); // 状态机
void ShiftAvarage(void); // 滑动平均
//*****************************************************************************
//
// 变量定义
//
//*****************************************************************************

// 软件定时器计数
uint8_t clock100ms = 0;
uint8_t clock500ms = 0;
uint8_t clock40ms = 0;
uint8_t clock250ms = 0;
uint8_t clock400ms = 0;

// 软件定时器溢出标志
uint8_t clock100ms_flag = 0;
uint8_t clock500ms_flag = 0;
uint8_t clock40ms_flag = 0;
uint8_t clock250ms_flag = 0;
uint8_t clock400ms_flag = 0;

// 测试用计数器
uint32_t test_counter = 0;

// 8位数码管显示的数字或字母符号
// 注：板上数码位从左到右序号排列为4、5、6、7、0、1、2、3
uint8_t digit[8]={' ',' ',' ',' ','_',' ','_',' '};

// 8位小数点 1亮  0灭
// 注：板上数码位小数点从左到右序号排列为4、5、6、7、0、1、2、3
uint8_t pnt = 0x11;

// 8个LED指示灯状态，0灭，1亮
// 注：板上指示灯从左到右序号排列为7、6、5、4、3、2、1、0
//     对应元件LED8、LED7、LED6、LED5、LED4、LED3、LED2、LED1
uint8_t led[] = {1, 1, 1, 1, 1, 1, 1, 0};

// 当前按键值
uint8_t key_code = 0;
uint8_t key_cnt = 0;

// DAC6571
uint32_t DAC6571_code = 1023;
uint32_t DAC6571_voltage = 250;
uint8_t  DAC6571_flag = 0;


// 系统时钟频率 
uint32_t ui32SysClock;


// AIN2(PE1)  ADC???[0-4095]
uint32_t ui32ADC0Value[2]; 
uint32_t ui32ADC1Value[1]; 

//??30???????
uint32_t data_u[30];
uint32_t data_i[30];
uint32_t data_i_c[30];

//?????????????
//?30?????
uint8_t flag_u=0; 
uint8_t flag_i=0;
uint8_t flag_i_c =0;

uint32_t sum_u=0;
uint32_t sum_i=0;
uint32_t sum_i_c=0;
uint32_t mean_u=0;
uint32_t mean_i=0;
uint32_t mean_i_c=0;

// AIN2???(???0.01V) [0.00-3.30]
uint32_t ui32ADC0Voltage; 


// DAC转码后的设定电流
uint32_t DAC_set_current = 0;
uint32_t Last_DAC_set_current = 0;

// 循环显示
uint8_t display_toggle_cnt = 0;
uint8_t display_toggle_flag = 0;

// 状态机变量
uint8_t status = 1;
int error = 0;
uint32_t DAC_RANGE_VALID_MAX = 0;
uint32_t DAC_RANGE_VALID_MIN = 0;
uint8_t valid_flag  = 0 ;
uint32_t fix_thresh = 20;
uint8_t fix_flag = 0;
uint32_t fix_code = 0;

// 均流
uint32_t target_i=0;
uint32_t last_target_i=0;


//*****************************************************************************
//
// 主程序
//
//*****************************************************************************
 int main(void)
{
	uint8_t temp,i;

	DevicesInit();            //  MCU器件初始化
	
	while (clock100ms < 3);   // 延时>60ms,等待TM1638上电完成
	TM1638_Init();	          // 初始化TM1638
	
    DAC6571_flag = 1;
    
	while (1) // 程序主循环
	{				
		StateMachine(); // 状态机
    if (clock100ms_flag == 1)   // 检查DAC电压是否要变
		{
			clock100ms_flag = 0;
			// DAC6571_code = 120;
			// DAC 与 code 的关系 还要再修正一下
			//DAC6571_code = (DAC_set_current - 3.78) * 128. / 276.0;
			// 数码管显�
			
			if (display_toggle_flag == 1)
			{
				
				digit[4] = mean_u/ 1000; 	     // ??ADC??????
				digit[5] = mean_u / 100 % 10; 	 // ??ADC??????
				digit[6] = mean_u / 10 % 10; 	 // ??ADC??????
				digit[7] = mean_u % 10;           // ??ADC??????
				digit[0] = mean_i / 1000. ; 	  // 计算千位数
				digit[1] = mean_i / 100 % 10;   // 计算百位数
				digit[2] = mean_i / 10 % 10;    // 计算十位数
				digit[3] = mean_i % 10;         // 计算个位数

			}
			/*
			if (display_toggle_flag == 2)
			{	
				pnt = 0x11
				digit[4] = mean_i / 1000; 	     // ??ADC??????
				digit[5] = mean_i / 100 % 10; 	 // ??ADC??????
				digit[6] = mean_i / 10 % 10; 	 // ??ADC??????
				digit[7] = mean_i % 10;           // ??ADC??????
			}
			if (display_toggle_flag == 3)
			{
				pnt = 0x1;
				digit[4] = DAC6571_code / 1000 ; 	  // 计算千位数
				digit[5] = DAC6571_code / 100 % 10;   // 计算百位数
				digit[6] = DAC6571_code / 10 % 10;    // 计算十位数
				digit[7] = DAC6571_code % 10;         // 计算个位数
      }
			*/
			// 显示DAC芯片设定的电�
			if (display_toggle_flag == 2){
			digit[4] = mean_i / 1000 ; 	  // 计算千位数
			digit[5] = mean_i / 100 % 10;   // 计算百位数
			digit[6] = mean_i / 10 % 10;    // 计算十位数
			digit[7] = mean_i % 10;         // 计算个位数
				
			digit[0] = mean_i_c / 1000. ; 	  // 计算千位数
			digit[1] = mean_i_c / 100 % 10;   // 计算百位数
			digit[2] = mean_i_c / 10 % 10;    // 计算十位数
			digit[3] = mean_i_c % 10;         // 计算个位数
			}
			DAC6571_Fastmode_Operation(DAC6571_code); //DAC转换后输出
		}
		
		// 用于DEBUG的走马灯
		if (clock500ms_flag == 1)   // 检查0.5秒定时是否到
		{
			clock500ms_flag = 0;
			// 8个指示灯以走马灯方式，每0.5秒向右（循环）移动一格
			temp = led[0];
			for (i = 0; i < 7; i++) led[i] = led[i + 1];
			led[7] = temp;
		}
		
		// ADC编码转换部分
		if (clock40ms_flag == 1)        // ??40ms??????  ?80ms
    {
      clock40ms_flag = 0;
			ShiftAvarage(); // 进行滑动平均
      /*      
			sum_u-=data_u[flag_u];
			sum_i-=data_i[flag_i];
			ADC_Sample();
					
			data_u[flag_u]=ui32ADC0Value[0];
			sum_u+=data_u[flag_u];
			flag_u=(flag_u+1)%30;
					
			data_i[flag_i]=ui32ADC0Value[1];
			sum_i+=data_i[flag_i];
			flag_i=(flag_i+1)%30;
						
						
			mean_u=sum_u/30;
			mean_i=sum_i/30;
					
						//mean_u=sum_u/27;
						//mean_i=sum_i/56;
					
			mean_u/=1.2432;
			mean_i/=1.2340;
				
			mean_u*=2;
			mean_i/=15;
			mean_i/=0.1017;
						*/
    }
		
		//Last_DAC_set_current = DAC_set_current; //记录下上一次设定的电流
		
		
	}
	
}
//*****************************************************************************
//
// 函数原型：void StateMachine(void)
// 函数功能：
// 函数参数：无
// 函数返回值：无
//
//*****************************************************************************
void StateMachine(void) {
	switch(status) {
	/*	case 1: 
			// 对 mean_i 进行稳定处理
			
			DAC6571_code = (target_i - 3.78) * 128. / 276.0 ;
			error = target_i - mean_i; // 电压差
			if (error > 100) {
				
			}
		case 2: // 状态二进行等待
			if (clock250ms_flag) {
				clock250ms_flag = 0;
				status = 3;
			} else {
				status = 2;
			}
			break;
	*/	
		
		case 1: // 状态一
			/*if (fix_flag) {
				DAC6571_code = DAC6571_code;
				if (mean_i - target_i > -fix_thresh && mean_i - target_i < fix_thresh){
					status = 1;
					break;
				} else {
					fix_flag = 0;
				}
			}
			else if (mean_i - target_i > -5 && mean_i - target_i < 5) {
				fix_flag = 1;
				status = 1;
				break;
			}
			
			if (!fix_flag) {
				DAC6571_code = (target_i - 3.78) * 128. / 276.0 ;
				status = 2;
				clock250ms = 0;
				clock250ms_flag = 0;
				break;
			} 
			*/
			  //DAC6571_code = (target_i*0.932 - 3.78) * 128. / 276.0 ;
			  last_target_i = target_i;
				if (mean_i - mean_i_c < 10 && mean_i - mean_i_c > -10) {
					status = 1;
					break;
				} 
				else {
					DAC6571_code = (target_i*0.932 - 3.78) * 128. / 276.0 ;
				}
				status = 2;
				clock250ms = 0;
				clock250ms_flag = 0;
				break;
		case 2: // 状态二 利用systick进行延时 
			if (clock250ms_flag) {
				clock250ms_flag = 0;
				status = 3;
			} else {
				status = 2;
			}
			break;
		case 3: // 状态三 
			if (clock400ms_flag) {
				clock400ms_flag = 0;
				status = 3;
				break;
			}
			
			// TODO:这边需要添加一个门限。防止抖动
			if (last_target_i != target_i) { //TODO: 添加一个门限
				if (mean_i > target_i+3 && mean_u < 5200){
					error = mean_i - target_i;
					
					if (mean_i - mean_i_c < 10) {
							fix_code = DAC6571_code;
							fix_flag = 1;
							DAC_RANGE_VALID_MAX = DAC6571_code; 
							if (DAC_RANGE_VALID_MAX - DAC_RANGE_VALID_MIN > 0 
								&& DAC_RANGE_VALID_MAX - DAC_RANGE_VALID_MIN < 10)
								valid_flag = 1;
					}
					
					if (valid_flag) {
						//if(fix_flag)
						//	DAC6571_code = (DAC_RANGE_VALID_MAX+DAC_RANGE_VALID_MIN) / 2 + 1;
						//else 
						//	DAC6571_code = fix_code;
					} else {
						if (error > 20)
							DAC6571_code -= 1;
						else
							DAC6571_code -= 1;
					}
					//if (DAC6571_code <= 80 && DAC6571_code >= 72 ) DAC6571_code = 75; // 强行修正
					//if (DAC6571_code <= 125 && DAC6571_code >= 118 ) DAC6571_code = 120;
					clock250ms = 0;
					clock250ms_flag = 0;
					status = 2;
				} else if (mean_i < target_i-3 && mean_u < 5200){
					error = target_i - mean_i;
					
					if (mean_i_c - mean_i < 10) {
							fix_code = DAC6571_code;
							fix_flag = 1;
							DAC_RANGE_VALID_MIN = DAC6571_code;
							if (DAC_RANGE_VALID_MAX - DAC_RANGE_VALID_MIN > 0 
								&& DAC_RANGE_VALID_MAX - DAC_RANGE_VALID_MIN < 10)
								valid_flag = 1; 						
					} else {
						fix_flag = 0;
					}
					
					
					if (valid_flag) {
						//if (fix_flag)
						//	DAC6571_code = (DAC_RANGE_VALID_MAX+DAC_RANGE_VALID_MIN) / 2 + 1;
						//else 
						//	DAC6571_code = fix_code;
					} else {
						if (error > 20)
							DAC6571_code += 5;
						else
							DAC6571_code += 1;
					}
					//if (DAC6571_code <= 80 && DAC6571_code >= 72 ) DAC6571_code = 75; // 强行修正
					//if (DAC6571_code <= 125 && DAC6571_code >= 118 ) DAC6571_code = 120; // 强行修正
					clock250ms = 0;
					clock250ms_flag = 0;
					status = 2;
				} else if (mean_i < target_i && mean_u > 5200) {
					status = 4;
				}
			} else{
				status = 1;
			}
			if (target_i != last_target_i) status = 1;
			last_target_i = target_i;
			break;
		case 4: // 状态四 限压保护状态
			if (target_i > mean_i && mean_u > 5200){
				status = 4;
			} else {
				status = 3;
			}
			break;
		}
		valid_flag = 0;
}
//*****************************************************************************
//
// 函数原型：void ShiftAvarage(void)
// 函数功能：滑动平均，求取ADC的量
// 函数参数：无
// 函数返回值：无
//
//*****************************************************************************
void ShiftAvarage(void)
{
			sum_u-=data_u[flag_u];
			sum_i-=data_i[flag_i];
			sum_i_c-=data_i_c[flag_i_c];
			
			ADC_Sample();
			ADC_Sample_1();
					
			data_u[flag_u]=ui32ADC0Value[0];
			sum_u+=data_u[flag_u];
			flag_u=(flag_u+1)%30;
					
			data_i[flag_i]=ui32ADC0Value[1];
			sum_i+=data_i[flag_i];
			flag_i=(flag_i+1)%30;
						
			data_i_c[flag_i_c]=ui32ADC1Value[0];
			sum_i_c+=data_i_c[flag_i_c];
			flag_i_c=(flag_i_c+1)%30;
						
			mean_u=sum_u/30;
			mean_i=sum_i/30;
			mean_i_c=sum_i_c/30;
					
			mean_u/=1.2432;
			mean_i/=1.2340;
			mean_i_c/=1.2340;
			
			mean_u*=2;
			mean_i/=15;
			mean_i/=0.1017;
			mean_i_c/=15;
			mean_i_c/=0.1017;
			
			// 二次标定
			mean_i *= 1.013;
			mean_i_c *= 1.0255;
			
			
			//target_i = 400;
			target_i = (mean_i + mean_i_c) / 2;
			//target_i = mean_i_c;
			if (mean_i_c > 900) target_i = target_i * 1.019;
			if ( 450 < mean_i_c && mean_i_c < 550) target_i = target_i * 0.99;
			if ( 650 < mean_i_c && mean_i_c < 750) target_i = target_i * 1.012;
}
//*****************************************************************************
//
// 函数原型：void GPIOInit(void)
// 函数功能：GPIO初始化。使能PortK，设置PK4,PK5为输出；使能PortM，设置PM0为输出。
//          （底板走线上，PK4连接着TM1638的STB，PK5连接TM1638的DIO，PM0连接TM1638的CLK）
//          （通过人工跳接，PL0须连线DAC6571之SDA，PL1连线SCL）
// 函数参数：无
// 函数返回值：无
//
//*****************************************************************************
void GPIOInit(void)
{
	//配置用于控制TM1638芯片、DAC6571芯片的管脚
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOK);				// 使能端口 K	
	while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOK)){};		// 等待端口 K准备完毕		
	
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOM);				// 使能端口 M	
	while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOM)){};		// 等待端口 M准备完毕		
	
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOL);				// 使能端口 L	
	while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOL)){};		// 等待端口 L准备完毕		
    
    // 设置端口 K的第4,5位（PK4,PK5）为输出引脚		PK4-STB  PK5-DIO
	GPIOPinTypeGPIOOutput(GPIO_PORTK_BASE, GPIO_PIN_4|GPIO_PIN_5);
	// 设置端口 M的第0位（PM0）为输出引脚   PM0-CLK
	GPIOPinTypeGPIOOutput(GPIO_PORTM_BASE, GPIO_PIN_0);	


    // 设置端口 L的第0,1位（PL0,PL1）为输出引脚		PL0-SDA  PL1-SCL (DAC6571)
	GPIOPinTypeGPIOOutput(GPIO_PORTL_BASE, GPIO_PIN_0|GPIO_PIN_1);
        
}

//*****************************************************************************
// 
// 函数原型：SysTickInit(void)
// 函数功能：设置SysTick中断
// 函数参数：无
// 函数返回值：无
//
//*****************************************************************************
void SysTickInit(void)
{
	SysTickPeriodSet(ui32SysClock/SYSTICK_FREQUENCY); // 设置心跳节拍,定时周期20ms
	SysTickEnable();  			// SysTick使能
	SysTickIntEnable();			// SysTick中断允许
}

//*****************************************************************************
// 
// 函数原型：void DevicesInit(void)
// 函数功能：CU器件初始化，包括系统时钟设置、GPIO初始化和SysTick中断设置
// 函数参数：无
// 函数返回值：无
//
//*****************************************************************************
void DevicesInit(void)
{
	// 使用外部25MHz主时钟源，经过PLL，然后分频为20MHz
	ui32SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |SYSCTL_OSC_MAIN | 
	                                   SYSCTL_USE_PLL |SYSCTL_CFG_VCO_480), 
	                                   1000000);

	GPIOInit();             // GPIO初始化
	ADCInit();
	SysTickInit();          // 设置SysTick中断
  IntMasterEnable();			// 总中断允许
}

//*****************************************************************************
//
// 函数原型:void ADCInit(void)
// 函数功能:ADC0???? ??AIN2/PE1??ADC??????,?????????????
// 函数参数:无
// 函数返回值:无
//
//*****************************************************************************
void ADCInit(void)
{	   
    // ADC0 ADC1
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
		SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC1);
    
	  // 使用AIN2/PE1 AIN1/PE2作为ADC输入,使能E1 E2 E3端口
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);

    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3); // 

    // 配置
		ADCSequenceConfigure(ADC0_BASE, 1, ADC_TRIGGER_PROCESSOR, 0);
		ADCSequenceConfigure(ADC1_BASE, 3, ADC_TRIGGER_PROCESSOR, 0);
	
	  // ADC采样
		ADCSequenceStepConfigure(ADC0_BASE, 1, 0, ADC_CTL_CH2);
	  ADCSequenceStepConfigure(ADC0_BASE, 1, 1, ADC_CTL_CH1 | ADC_CTL_IE | ADC_CTL_END);
	  ADCSequenceStepConfigure(ADC1_BASE, 3, 0, ADC_CTL_CH0 | ADC_CTL_IE | ADC_CTL_END);

    //
    // Configure step 0 on sequence 3.  Sample channel 0 (ADC_CTL_CH0) in
    // single-ended mode (default) and configure the interrupt flag
    // (ADC_CTL_IE) to be set when the sample is done.  Tell the ADC logic
    // that this is the last conversion on sequence 3 (ADC_CTL_END).  Sequence
    // 3 has only one programmable step.  Sequence 1 and 2 have 4 steps, and
    // sequence 0 has 8 programmable steps.  Since we are only doing a single
    // conversion using sequence 3 we will only configure step 0.  For more
    // information on the ADC sequences and steps, reference the datasheet.
    //


    // 使能单次采样方式(sample sequence 3)
    ADCSequenceEnable(ADC0_BASE, 1);
		ADCSequenceEnable(ADC1_BASE, 3);
    
		// 采样前清空中断标志位
    ADCIntClear(ADC0_BASE, 1);		
		ADCIntClear(ADC1_BASE, 3);

}

//*****************************************************************************
//
// 函数原型:uint32_t ADC_Sample(void)
// 函数功能�:??ADC????
// 函数参数:无
// 函数返回值:ADC???[0-4095]
//
//*****************************************************************************
void ADC_Sample(void)
{

    //
    // This array is used for storing the data read from the ADC FIFO. It
    // must be as large as the FIFO for the sequencer in use.  This example
    // uses sequence 3 which has a FIFO depth of 1.  If another sequence
    // was used with a deeper FIFO, then the array size must be changed.
    //
    // uint32_t pui32ADC0Value[2];
		
    // ??ADC??
    ADCProcessorTrigger(ADC0_BASE, 1);

	
    // 等待采样转换完成
    while(!ADCIntStatus(ADC0_BASE, 1, false))
    {
    }

    // 清空ADC中断标志位
    ADCIntClear(ADC0_BASE, 1);
		
    
		// 读取ADC采样值
    ADCSequenceDataGet(ADC0_BASE, 1, ui32ADC0Value);
		
		
    return ;
}
void ADC_Sample_1(void)
{

    //
    // This array is used for storing the data read from the ADC FIFO. It
    // must be as large as the FIFO for the sequencer in use.  This example
    // uses sequence 3 which has a FIFO depth of 1.  If another sequence
    // was used with a deeper FIFO, then the array size must be changed.
    //
    // uint32_t pui32ADC0Value[2];
		
    // ??ADC??
 		ADCProcessorTrigger(ADC1_BASE, 3);
	
    // 等待采样转换完成
    while(!ADCIntStatus(ADC1_BASE, 3, false) )
    {
    }

    // 清空ADC中断标志位
		ADCIntClear(ADC1_BASE, 3);
    
		// 读取ADC采样值
		ADCSequenceDataGet(ADC1_BASE, 3, ui32ADC1Value);
		
    return ;
}

//*****************************************************************************
// 
// 函数原型：void SysTick_Handler(void)
// 函数功能：SysTick中断服务程序
// 函数参数：无
// 函数返回值：无
//
//*****************************************************************************
void SysTick_Handler(void)       // 定时周期为20ms
{
	// 0.1秒钟软定时器计数
	if (++clock100ms >= V_T100ms)
	{
		clock100ms_flag = 1; // 当0.1秒到时，溢出标志置1
		clock100ms = 0;
	}
	// 40ms软定时器计数
	if (++clock40ms >= V_T40ms)
	{
		clock40ms_flag = 1;
		clock40ms = 0;
	}
	
	if (++clock400ms >= V_T400ms)
	{
		clock400ms_flag = 1;
		clock400ms = 0;
	}
	
 	// 0.5秒钟软定时器计数
	if (++clock500ms >= V_T500ms)
	{
		clock500ms_flag = 1; // 当0.5秒到时，溢出标志置1
		clock500ms = 0;
	}
	
	if (++clock250ms >= V_T240ms) {
		clock250ms_flag = 1;
		clock250ms = 0;
	}
	// 刷新全部数码管和LED指示灯
	TM1638_RefreshDIGIandLED(digit, pnt, led);

	// 检查当前键盘输入，0代表无键操作，1-9表示有对应按键
	// 键号显示在一位数码管上
	key_code = TM1638_Readkeyboard();

//	if (key_code != 0)
//	{
//		if (key_cnt < 4) key_cnt++;   // 按键消抖，4*20ms
//		else if (key_cnt == 4)
//		{
//			if (key_code == 1)      // 加1
//			{
//				if (DAC6571_code < DAC6571_code_max) 
//				{
//					DAC6571_code++;
//					DAC6571_flag = 1;
//				}
//			}
//			else if (key_code == 2)  // 减1
//			{
//				if (DAC6571_code > 0) 
//				{
//					DAC6571_code--;
//					DAC6571_flag = 1;
//				}
//			}
//			else if (key_code == 3)  // 加10
//			{
//				if (DAC6571_code < DAC6571_code_max - 10) 
//				{
//					DAC6571_code += 10;
//					DAC6571_flag = 1;
//				}
//			}
//			else if (key_code == 4)   // 减10
//			{
//				if (DAC6571_code > 10) 
//				{
//					DAC6571_code -= 10;
//					DAC6571_flag = 1;
//				}
//			}
//			else if (key_code == 5)   // 加100
//			{
//				if (DAC6571_code < DAC6571_code_max - 100) 
//				{
//					DAC6571_code += 100;
//					DAC6571_flag = 1;
//				}
//			}
//			else if (key_code == 6)   // 减100
//			{
//				if (DAC6571_code > 100) 
//				{
//					DAC6571_code -= 100;
//					DAC6571_flag = 1;
//				}
//			}

//			key_cnt = 5;   // 按键一直按着，只改变一次
//		}
//	}
//	else key_cnt = 0;
if (key_code != 0)
	{
		if (key_cnt < 4) key_cnt++;   // 按键消抖，4*20ms
		else if (key_cnt == 4)
		{
			
            switch(key_code)
            {
                case 1:     // 加100
									if (DAC_set_current < DAC_set_current_max - 100) 
				    {
					     DAC_set_current += 100;
					     DAC6571_flag = 1;
				    }
                    break;
                case 4:    // 减100
                    if (DAC_set_current >= 100) 
				    {
					    DAC_set_current -= 100;
					    DAC6571_flag = 1;
				    }
                    break;
                case 2:    // 加10
                   if (DAC_set_current < DAC_set_current_max - 10) 
				    {
					     DAC_set_current += 10;
					     DAC6571_flag = 1;
				    }                    
                    break;
                case 5:    // 减10
                   if (DAC_set_current >= 10) 
				    {
					    DAC_set_current -= 10;
					    DAC6571_flag = 1;
				    }
                    break;
                case 3:    // 加1
                   if (DAC_set_current < DAC_set_current_max - 1) 
				    {
					     DAC_set_current += 1;
					     DAC6571_flag = 1;
				    }
                    break;
                case 6:    // 减1
                   if (DAC_set_current >= 1) 
				    {
					    DAC_set_current -= 1;
					    DAC6571_flag = 1;
				    }
                    break;
								case 7:
									display_toggle_flag = 1;
									break;
								case 8:
									display_toggle_flag = 2;
									break;
								case 9:
									display_toggle_flag = 3;
									break;
								default:
                    break;
            }
            
			key_cnt = 5;   // 按键一直按着，只改变一次
		}
	}
	else key_cnt = 0;
       
	//digit[5] = key_code;   // 按键值

}
