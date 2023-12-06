#include <algorithm>
#include <stdint.h>
#include "lcd.h"

#define LCD_BASE 0x63000000

#define LCD_REG_CMD LCD_BASE
#define LCD_REG_DATA (LCD_BASE+4)

extern int debug;
static void lcd_reg_write(uint8_t reg)
{
	*(volatile uint8_t *)LCD_REG_CMD = reg;
}

static void lcd_data_write(uint8_t data)
{
	*(volatile uint8_t *)LCD_REG_DATA = data;
}

static uint8_t lcd_data_read(void)
{
	return *(volatile uint8_t *)LCD_REG_DATA;
}

static uint8_t lcd_status_read(void)
{
	return *(volatile uint8_t *)LCD_REG_CMD;
}

static void lcd_reg_data_write(uint8_t reg, uint8_t data)
{
	lcd_reg_write(reg);
	lcd_data_write(data);
}

static uint8_t lcd_reg_data_read(uint8_t reg)
{
	lcd_reg_write(reg);
	return lcd_data_read();
}

static void lcd_write_fifo_not_full(void)
{
	int i = 0x1000000;
	while(lcd_status_read() & 0x80 && --i);
	if (!i)
		printf("LCD timeout\r\n");
}
#if 0
static void lcd_write_fifo_empty(void)
{
	while(lcd_status_read() & 0x40);
}

static void lcd_read_fifo_not_full(void)
{
	while(lcd_status_read() & 0x20);
}

static void lcd_read_fifo_not_empty(void)
{
	while(lcd_status_read() & 0x10);
}
#endif

static inline void lcd_check_2d_busy(void)
{
	int i = 0x100000;
	while((lcd_status_read() & 0x08) && --i);
	if (!i)
		printf("LCD timeout\r\n");
}

static void lcd_reset(void)
{
	pinMode(PIN_LCD_RESET, OUTPUT);
	digitalWrite(PIN_LCD_RESET, HIGH);
	delay(1);
	digitalWrite(PIN_LCD_RESET, LOW);
	delay(1);
	digitalWrite(PIN_LCD_RESET, HIGH);
	delay(10);
}

static boolean lcd_begin(void)
{
	lcd_reset();
	if(!lcd_check_ic_ready())
		return false;

	//read ID code must disable pll, 01h bit7 set 0
	lcd_reg_data_write(0x01,0x08);
	delay(1);
	uint8_t id = lcd_reg_data_read(0xff);
	if (id != 0x76 && id != 0x77) {
		Serial.println("RA8876 or RA8877 not found!");
		return false;
	} else {
		Serial.println("RA8876 or RA8877 connect pass!");
	}

	if(!lcd_ra8876_initialize()) {
		Serial.println("ra8876 or RA8877 initial fail!");
		return false;
	} else {
		Serial.println("RA8876 or RA8877 initial Pass!");
	}
	return true;
}

static boolean lcd_ra8876_initialize(void)
{
	if(!lcd_ra8876_pll_init()) {
		Serial.println("PLL initial fail!");
		return false;
	}

	if(!lcd_ra8876_sdram_init()) {
		Serial.println("SDRAM initial fail!");
		return false;
	}

	lcd_reg_write(RA8876_CCR);//01h
	lcd_data_write(RA8876_PLL_ENABLE<<7|RA8876_WAIT_NO_MASK<<6|RA8876_KEY_SCAN_DISABLE<<5|RA8876_TFT_OUTPUT24<<3
		     |RA8876_I2C_MASTER_DISABLE<<2|RA8876_SERIAL_IF_ENABLE<<1|RA8876_HOST_DATA_BUS_SERIAL);

	lcd_reg_write(RA8876_MACR);//02h
	lcd_data_write(RA8876_DIRECT_WRITE<<6|RA8876_READ_MEMORY_LRTB<<4|RA8876_WRITE_MEMORY_LRTB<<1);

	lcd_reg_write(RA8876_ICR);//03h
	lcd_data_write(RA8877_LVDS_FORMAT<<3|RA8876_GRAPHIC_MODE<<2|RA8876_MEMORY_SELECT_IMAGE);

	lcd_reg_write(RA8876_MPWCTR);//10h
	lcd_data_write(RA8876_PIP1_WINDOW_DISABLE<<7|RA8876_PIP2_WINDOW_DISABLE<<6|RA8876_SELECT_CONFIG_PIP1<<4
		     |RA8876_IMAGE_COLOCR_DEPTH_16BPP << 2|TFT_MODE);

	lcd_reg_write(RA8876_PIPCDEP);//11h
	lcd_data_write(RA8876_PIP1_COLOR_DEPTH_16BPP<<2|RA8876_PIP2_COLOR_DEPTH_16BPP);

	lcd_reg_write(RA8876_AW_COLOR);//5Eh
	lcd_data_write(RA8876_CANVAS_BLOCK_MODE<<2|RA8876_CANVAS_COLOR_DEPTH_16BPP);

	lcd_reg_data_write(RA8876_BTE_COLR,RA8876_S0_COLOR_DEPTH_16BPP<<5|RA8876_S1_COLOR_DEPTH_16BPP<<2|RA8876_S0_COLOR_DEPTH_16BPP);//92h

	/*TFT timing configure*/
	lcd_reg_write(RA8876_DPCR);//12h
	lcd_data_write(XPCLK_INV<<7|RA8876_DISPLAY_OFF<<6|RA8876_OUTPUT_RGB);

	lcd_reg_write(RA8876_PCSR);//13h
	lcd_data_write(XHSYNC_INV<<7|XVSYNC_INV<<6|XDE_INV<<5);

	lcd_HorizontalWidthVerticalHeight(HDW,VDH);
	lcd_HorizontalNonDisplay(HND);
	lcd_HsyncStartPosition(HST);
	lcd_HsyncPulseWidth(HPW);
	lcd_VerticalNonDisplay(VND);
	lcd_VsyncStartPosition(VST);
	lcd_VsyncPulseWidth(VPW);

	/*image buffer configure*/
	lcd_displayImageStartAddress(PAGE1_START_ADDR);
	lcd_displayImageWidth(SCREEN_WIDTH);
	lcd_displayWindowStartXY(0,0);
	lcd_canvasImageStartAddress(PAGE1_START_ADDR);
	lcd_canvasImageWidth(SCREEN_WIDTH);
	lcd_activeWindowXY(0,0);
	lcd_activeWindowWH(SCREEN_WIDTH,SCREEN_HEIGHT);
	return true;
}
#if 0
static void lcd_data_write16(uint16_t data)
{
	// VERIFY
	lcd_data_write(data);
	lcd_data_write(data>>8);
}
#endif
static boolean lcd_check_sdram_ready(void)
{
	for (int i = 0; i < 1000000; i++) {
		if (lcd_status_read() &0x04)
			return true;
		delayMicroseconds(1);
	}
	return false;
}

static boolean lcd_check_ic_ready(void)
{
	for (int i=0;i<1000000;i++) {
		if (!(lcd_status_read() & 0x02))
			return true;
		delayMicroseconds(1);
	}
	return false;
}

static boolean lcd_ra8876_pll_init(void)
{

	// Set pixel clock
	if(50 < SCAN_FREQ) {
		lcd_reg_data_write(0x05,0x02);				//PLL Divided by 2
		lcd_reg_data_write(0x06,(SCAN_FREQ*2/OSC_FREQ)-1);
	}
	if(25 < SCAN_FREQ && SCAN_FREQ <= 50) {
		lcd_reg_data_write(0x05,0x04);				//PLL Divided by 4
		lcd_reg_data_write(0x06,(SCAN_FREQ*4/OSC_FREQ)-1);
	}
	if(12 < SCAN_FREQ && SCAN_FREQ <= 25) {
		lcd_reg_data_write(0x05,0x06);				//PLL Divided by 8
		lcd_reg_data_write(0x06,(SCAN_FREQ*8/OSC_FREQ)-1);
	}
	if(7 < SCAN_FREQ && SCAN_FREQ <= 12) {
		lcd_reg_data_write(0x05,0x08);				//PLL Divided by 16
		lcd_reg_data_write(0x06,(SCAN_FREQ*16/OSC_FREQ)-1);
	}
	if(SCAN_FREQ <= 7) {
		lcd_reg_data_write(0x05,0x0A);				//PLL Divided by 32
		lcd_reg_data_write(0x06,(SCAN_FREQ*32/OSC_FREQ)-1);
	}
	// Set SDRAM clock
	if(50 < DRAM_FREQ) {
		lcd_reg_data_write(0x07,0x02);				//PLL Divided by 2
		lcd_reg_data_write(0x08,(DRAM_FREQ*2/OSC_FREQ)-1);
	}
	if(25 < DRAM_FREQ && DRAM_FREQ <= 50) {
		lcd_reg_data_write(0x07,0x04);				//PLL Divided by 4
		lcd_reg_data_write(0x08,(DRAM_FREQ*4/OSC_FREQ)-1);
	}
	if(12 < DRAM_FREQ && DRAM_FREQ <= 25) {
		lcd_reg_data_write(0x07,0x06);				//PLL Divided by 8
		lcd_reg_data_write(0x08,(DRAM_FREQ*8/OSC_FREQ)-1);
	}
	// Set Core clock
	if(50 < CORE_FREQ) {
		lcd_reg_data_write(0x09,0x02);				//PLL Divided by 2
		lcd_reg_data_write(0x0A,(CORE_FREQ*2/OSC_FREQ)-1);
	}
	if(25 < CORE_FREQ && CORE_FREQ <= 50) {
		lcd_reg_data_write(0x09,0x04);				//PLL Divided by 4
		lcd_reg_data_write(0x0A,(CORE_FREQ*4/OSC_FREQ)-1);
	}
	if(12 < CORE_FREQ && CORE_FREQ <= 25) {
		lcd_reg_data_write(0x09,0x06);				//PLL Divided by 8
		lcd_reg_data_write(0x0A,(CORE_FREQ*8/OSC_FREQ)-1);
	}

	delay(1);
	lcd_reg_write(0x01);
	lcd_data_write(0x80);
	delay(2);//wait for pll stable
	return lcd_data_read() & 0x80;
}

static boolean lcd_ra8876_sdram_init(void)
{
	uint16_t Auto_Refresh = (64 * DRAM_FREQ * 1000) / 4096;

	lcd_reg_data_write(0xe0,0x29);
	lcd_reg_data_write(0xe1, 3);      //CAS:2=0x02，CAS:3=0x03
	lcd_reg_data_write(0xe2, Auto_Refresh);
	lcd_reg_data_write(0xe3, Auto_Refresh>>8);
	lcd_reg_data_write(0xe4, 0x01);
	lcd_check_sdram_ready();
	return true;
}

static void lcd_display_on(boolean on)
{
	uint8_t val = XPCLK_INV << 7 | RA8876_OUTPUT_RGB;
	if (on)
		val |= RA8876_DISPLAY_ON << 6;
	lcd_reg_data_write(RA8876_DPCR, val);
	delay(20);
}

static void lcd_HorizontalWidthVerticalHeight(uint16_t width, uint16_t height)
{
	lcd_reg_data_write(RA8876_HDWR, (width / 8) - 1);
	lcd_reg_data_write(RA8876_HDWFTR, width % 8);
	lcd_reg_data_write(RA8876_VDHR0, height - 1);
	lcd_reg_data_write(RA8876_VDHR1, (height - 1) >> 8);
}

static void lcd_HorizontalNonDisplay(uint16_t numbers)
{
	if (numbers < 8) {
		lcd_reg_data_write(RA8876_HNDR,0x00);
		lcd_reg_data_write(RA8876_HNDFTR,numbers);
	} else {
		lcd_reg_data_write(RA8876_HNDR, (numbers / 8) - 1);
		lcd_reg_data_write(RA8876_HNDFTR, numbers % 8);
	}
}

static void lcd_HsyncStartPosition(uint16_t numbers)
{
	if (numbers < 8)
		lcd_reg_data_write(RA8876_HSTR, 0x00);
	else
		lcd_reg_data_write(RA8876_HSTR, numbers / 8 - 1);
}

static void lcd_HsyncPulseWidth(uint16_t numbers)
{
	if (numbers < 8)
		lcd_reg_data_write(RA8876_HPWR,0x00);
	else
		lcd_reg_data_write(RA8876_HPWR, numbers / 8 - 1);
}

static void lcd_VerticalNonDisplay(uint16_t numbers)
{
	numbers--;
	lcd_reg_data_write(RA8876_VNDR0, numbers);
	lcd_reg_data_write(RA8876_VNDR1, numbers >> 8);
}

static void lcd_VsyncStartPosition(uint16_t numbers)
{
	lcd_reg_data_write(RA8876_VSTR, numbers - 1);
}

static void lcd_VsyncPulseWidth(uint16_t numbers)
{
	lcd_reg_data_write(RA8876_VPWR, numbers - 1);
}

static void lcd_displayImageStartAddress(uint32_t addr)
{
	lcd_reg_data_write(RA8876_MISA0,addr);
	lcd_reg_data_write(RA8876_MISA1,addr >> 8);
	lcd_reg_data_write(RA8876_MISA2,addr >> 16);
	lcd_reg_data_write(RA8876_MISA3,addr >> 24);
}

static void lcd_displayImageWidth(uint16_t width)
{
	lcd_reg_data_write(RA8876_MIW0, width);
	lcd_reg_data_write(RA8876_MIW1, width >> 8);
}

static void lcd_displayWindowStartXY(uint16_t x0, uint16_t y0)
{
	lcd_reg_data_write(RA8876_MWULX0, x0);
	lcd_reg_data_write(RA8876_MWULX1, x0 >> 8);
	lcd_reg_data_write(RA8876_MWULY0, y0);
	lcd_reg_data_write(RA8876_MWULY1, y0 >> 8);
}

static void lcd_canvasImageStartAddress(uint32_t addr)
{
	lcd_reg_data_write(RA8876_CVSSA0, addr);
	lcd_reg_data_write(RA8876_CVSSA1, addr >> 8);
	lcd_reg_data_write(RA8876_CVSSA2, addr >> 16);
	lcd_reg_data_write(RA8876_CVSSA3, addr >> 24);
}

static void lcd_canvasImageWidth(uint16_t width)
{
	lcd_reg_data_write(RA8876_CVS_IMWTH0, width);
	lcd_reg_data_write(RA8876_CVS_IMWTH1, width >> 8);
}

static void lcd_activeWindowXY(uint16_t x0, uint16_t y0)
{
	lcd_reg_data_write(RA8876_AWUL_X0, x0);
	lcd_reg_data_write(RA8876_AWUL_X1, x0 >> 8);
	lcd_reg_data_write(RA8876_AWUL_Y0, y0);
	lcd_reg_data_write(RA8876_AWUL_Y1, y0 >> 8);
}

static void lcd_activeWindowWH(uint16_t width, uint16_t height)
{
	lcd_reg_data_write(RA8876_AW_WTH0, width);
	lcd_reg_data_write(RA8876_AW_WTH1, width >> 8);
	lcd_reg_data_write(RA8876_AW_HT0, height);
	lcd_reg_data_write(RA8876_AW_HT1, height >> 8);
}

#if 0
static void lcd_setPixelCursor(uint16_t x, uint16_t y)
{
	lcd_reg_data_write(RA8876_CURH0, x);
	lcd_reg_data_write(RA8876_CURH1, x >> 8);
	lcd_reg_data_write(RA8876_CURV0, y);
	lcd_reg_data_write(RA8876_CURV1, y >> 8);
}

static void lcd_bte_Source0_MemoryStartAddr(uint32_t addr)
{
	lcd_reg_data_write(RA8876_S0_STR0, addr);
	lcd_reg_data_write(RA8876_S0_STR1, addr >> 8);
	lcd_reg_data_write(RA8876_S0_STR2, addr >> 16);
	lcd_reg_data_write(RA8876_S0_STR3, addr >> 24);
}

static void lcd_bte_Source0_ImageWidth(uint16_t width)
{
	lcd_reg_data_write(RA8876_S0_WTH0, width);
	lcd_reg_data_write(RA8876_S0_WTH1, width >> 8);
}

static void lcd_bte_Source0_WindowStartXY(uint16_t x0, uint16_t y0)
{
	lcd_reg_data_write(RA8876_S0_X0, x0);
	lcd_reg_data_write(RA8876_S0_X1, x0 >> 8);
	lcd_reg_data_write(RA8876_S0_Y0, y0);
	lcd_reg_data_write(RA8876_S0_Y1, y0 >> 8);
}

static void lcd_bte_Source1_MemoryStartAddr(uint32_t addr)
{
	lcd_reg_data_write(RA8876_S1_STR0, addr);
	lcd_reg_data_write(RA8876_S1_STR1, addr >> 8);
	lcd_reg_data_write(RA8876_S1_STR2, addr >> 16);
	lcd_reg_data_write(RA8876_S1_STR3, addr >> 24);
}

static void lcd_bte_Source1_ImageWidth(uint16_t width)
{
	lcd_reg_data_write(RA8876_S1_WTH0, width);
	lcd_reg_data_write(RA8876_S1_WTH1, width >> 8);
}

static void lcd_bte_Source1_WindowStartXY(uint16_t x0, uint16_t y0)
{
	lcd_reg_data_write(RA8876_S1_X0, x0);
	lcd_reg_data_write(RA8876_S1_X1, x0 >> 8);
	lcd_reg_data_write(RA8876_S1_Y0, y0);
	lcd_reg_data_write(RA8876_S1_Y1, y0 >> 8);
}

static void  lcd_bte_DestinationMemoryStartAddr(uint32_t addr)
{
	lcd_reg_data_write(RA8876_DT_STR0, addr);
	lcd_reg_data_write(RA8876_DT_STR1, addr >> 8);
	lcd_reg_data_write(RA8876_DT_STR3, addr >> 16);
	lcd_reg_data_write(RA8876_DT_STR3, addr >> 24);
}

static void lcd_bte_DestinationImageWidth(uint16_t width)
{
	lcd_reg_data_write(RA8876_DT_WTH0, width);
	lcd_reg_data_write(RA8876_DT_WTH1, width >> 8);
}

static void lcd_bte_DestinationWindowStartXY(uint16_t x0, uint16_t y0)
{
	lcd_reg_data_write(RA8876_DT_X0, x0);
	lcd_reg_data_write(RA8876_DT_X1, x0 >> 8);
	lcd_reg_data_write(RA8876_DT_Y0, y0);
	lcd_reg_data_write(RA8876_DT_Y1, y0 >> 8);
}

static void lcd_bte_WindowSize(uint16_t width, uint16_t height)
{
	lcd_reg_data_write(RA8876_BTE_WTH0, width);
	lcd_reg_data_write(RA8876_BTE_WTH1, width >> 8);
	lcd_reg_data_write(RA8876_BTE_HIG0, height);
	lcd_reg_data_write(RA8876_BTE_HIG1, height >> 8);
}

static void lcd_pwm_Prescaler(uint8_t prescaler)
{
	lcd_reg_data_write(RA8876_PSCLR, prescaler);
}

static void lcd_pwm_ClockMuxReg(uint8_t pwm1_clk_div, uint8_t pwm0_clk_div, uint8_t xpwm1_ctrl, uint8_t xpwm0_ctrl)
{
	lcd_reg_data_write(RA8876_PMUXR, pwm1_clk_div << 6 | pwm0_clk_div << 4 | xpwm1_ctrl << 2 | xpwm0_ctrl);
}

static void lcd_pwm_Configuration(uint8_t pwm1_inverter, uint8_t pwm1_auto_reload, uint8_t pwm1_start, uint8_t
				  pwm0_dead_zone, uint8_t pwm0_inverter, uint8_t pwm0_auto_reload, uint8_t pwm0_start)
{
	lcd_reg_data_write(RA8876_PCFGR, pwm1_inverter << 6 | pwm1_auto_reload << 5 | pwm1_start << 4 | pwm0_dead_zone << 3 |
			 pwm0_inverter << 2 | pwm0_auto_reload << 1 | pwm0_start);
}

static void lcd_pwm0_Duty(uint16_t duty)
{
	lcd_reg_data_write(RA8876_TCMPB0L, duty);
	lcd_reg_data_write(RA8876_TCMPB0H, duty >> 8);
}

static void lcd_pwm0_ClocksPerPeriod(uint16_t clocks_per_period)
{
	lcd_reg_data_write(RA8876_TCNTB0L,clocks_per_period);
	lcd_reg_data_write(RA8876_TCNTB0H,clocks_per_period >> 8);
}

static void lcd_pwm1_Duty(uint16_t duty)
{
	lcd_reg_data_write(RA8876_TCMPB1L,duty);
	lcd_reg_data_write(RA8876_TCMPB1H,duty>>8);
}

static void lcd_pwm1_ClocksPerPeriod(uint16_t clocks_per_period)
{
	lcd_reg_data_write(RA8876_TCNTB1L,clocks_per_period);
	lcd_reg_data_write(RA8876_TCNTB1F,clocks_per_period>>8);
}
#endif

static void lcd_prepare_ram_access(void)
{
	lcd_reg_write(RA8876_MRWDP);
}

static void foreGroundColor16bpp(uint16_t color)
{
	lcd_reg_data_write(RA8876_FGCR, color >> 8);
	lcd_reg_data_write(RA8876_FGCG, color >> 3);
	lcd_reg_data_write(RA8876_FGCB, color << 3);
}

static void backGroundColor16bpp(uint16_t color)
{
	lcd_reg_data_write(RA8876_BGCR, color >> 8);
	lcd_reg_data_write(RA8876_BGCG, color >> 3);
	lcd_reg_data_write(RA8876_BGCB, color << 3);
}

static void graphicMode(boolean on)
{
	uint8_t val = RA8877_LVDS_FORMAT << 3 | RA8876_MEMORY_SELECT_IMAGE;
	if(!on)
		val |= RA8876_TEXT_MODE << 2;
	lcd_reg_data_write(RA8876_ICR, val);
}

#if 0
static void lcd_putPixel_16bpp(uint16_t x, uint16_t y, uint16_t color)
{
	lcd_setPixelCursor(x,y);
	lcd_prepare_ram_access();
	lcd_data_write16(color);
}

static void lcd_putPicture_16bpp(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
	lcd_activeWindowXY(x,y);
	lcd_activeWindowWH(width,height);
	lcd_setPixelCursor(x,y);
	lcd_prepare_ram_access();
}

static void lcd_putPicture_16bpp(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const unsigned char *data)
{
	lcd_activeWindowXY(x,y);
	lcd_activeWindowWH(width,height);
	lcd_setPixelCursor(x,y);
	lcd_prepare_ram_access();
	for(int i = 0; i < height; i++) {
		for(int j = 0; j < width; j++) {
			//lcd_write_fifo_not_full();//if high speed mcu and without Xnwait check
			lcd_data_write(*data);
			data++;
			//lcd_write_fifo_not_full();//if high speed mcu and without Xnwait check
			lcd_data_write(*data);
			data++;
		}
	}
	lcd_write_fifo_empty();//if high speed mcu and without Xnwait check
	lcd_activeWindowXY(0,0);
	lcd_activeWindowWH(SCREEN_WIDTH,SCREEN_HEIGHT);
}

static void lcd_putPicture_16bpp(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const unsigned short *data)
{
	lcd_activeWindowXY(x,y);
	lcd_activeWindowWH(width,height);
	lcd_setPixelCursor(x,y);
	lcd_prepare_ram_access();
	for(int i = 0; i < height; i++) {
		for(int j = 0; j < width; j++) {
			//lcd_write_fifo_not_full();//if high speed mcu and without Xnwait check
			lcd_data_write16(*data);
			data++;
			//lcd_write_fifo_empty();//if high speed mcu and without Xnwait check
		}
	}
	lcd_write_fifo_empty();//if high speed mcu and without Xnwait check
	lcd_activeWindowXY(0,0);
	lcd_activeWindowWH(SCREEN_WIDTH,SCREEN_HEIGHT);
}
#endif
static void lcd_textMode(boolean on)
{
	graphicMode(!on);
}

static void lcd_textColor(uint16_t foreground_color, uint16_t background_color)
{
	lcd_check_2d_busy();
	foreGroundColor16bpp(foreground_color);
	backGroundColor16bpp(background_color);
}

static void lcd_setTextCursor(uint16_t x, uint16_t y)
{
	lcd_reg_data_write(RA8876_F_CURX0, x);
	lcd_reg_data_write(RA8876_F_CURX1, x >> 8);
	lcd_reg_data_write(RA8876_F_CURY0, y);
	lcd_reg_data_write(RA8876_F_CURY1, y >> 8);
}

static void lcd_setTextParameter1(uint8_t source_select, uint8_t size_select, uint8_t iso_select)
{
	lcd_reg_data_write(RA8876_CCR0, source_select << 6 | size_select << 4 | iso_select);
}

static void lcd_setTextParameter2(uint8_t align, uint8_t chroma_key, uint8_t width_enlarge, uint8_t height_enlarge)
{
	lcd_reg_data_write(RA8876_CCR1,align << 7 | chroma_key << 6 | width_enlarge << 2 | height_enlarge);
}

#if 0
static void lcd_genitopCharacterRomParameter(uint8_t scs_select, uint8_t clk_div, uint8_t rom_select, uint8_t character_select, uint8_t gt_width)
{
	lcd_reg_data_write(RA8876_SFL_CTRL, scs_select << 7 | RA8876_SERIAL_FLASH_FONT_MODE << 6 | \
			RA8876_SERIAL_FLASH_ADDR_24BIT << 5 | RA8876_FOLLOW_RA8876_MODE << 4 | \
			RA8876_SPI_FAST_READ_8DUMMY);
	lcd_reg_data_write(RA8876_SPI_DIVSOR,clk_div);//bbh
	lcd_reg_data_write(RA8876_GTFNT_SEL,rom_select<<5);//ceh
	lcd_reg_data_write(RA8876_GTFNT_CR,character_select<<3|gt_width);//cfh
}
#endif

static void lcd_putString(uint16_t x0, uint16_t y0, const char *str)
{
	lcd_check_2d_busy();
	lcd_textMode(true);
	lcd_setTextCursor(x0,y0);
	lcd_prepare_ram_access();
	while(*str) {
		lcd_write_fifo_not_full();
		lcd_data_write(*str);
		++str;
	}
	lcd_check_2d_busy();
	lcd_textMode(false);
}

static void set_draw_params_rect(int x0, int y0, int x1, int y1, int color)
{
	foreGroundColor16bpp(color);
	lcd_reg_data_write(RA8876_DLHSR0, x0);
	lcd_reg_data_write(RA8876_DLHSR1, x0 >> 8);
	lcd_reg_data_write(RA8876_DLVSR0, y0);
	lcd_reg_data_write(RA8876_DLVSR1, y0 >> 8);
	lcd_reg_data_write(RA8876_DLHER0, x1);
	lcd_reg_data_write(RA8876_DLHER1, x1 >> 8);
	lcd_reg_data_write(RA8876_DLVER0, y1);
	lcd_reg_data_write(RA8876_DLVER1, y1 >> 8);
}

static void lcd_drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
	lcd_check_2d_busy();
	set_draw_params_rect(x0, y0, x1, y1, color);
	lcd_reg_data_write(RA8876_DCR0, RA8876_DRAW_LINE);
}

static void lcd_drawSquare(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
	lcd_check_2d_busy();
	set_draw_params_rect(x0, y0, x1, y1, color);
	lcd_reg_data_write(RA8876_DCR1, RA8876_DRAW_SQUARE);
}

void lcd_drawSquareFill(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
	lcd_check_2d_busy();
	set_draw_params_rect(x0, y0, x1, y1, color);
	lcd_reg_data_write(RA8876_DCR1, RA8876_DRAW_SQUARE_FILL);
}
#if 0
static void lcd_drawCircleSquare(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t xr, uint16_t yr, uint16_t color)
{
	set_draw_params_rect(x0, y0, x1, y1, color);
	lcd_reg_data_write(RA8876_ELL_A0, xr);
	lcd_reg_data_write(RA8876_ELL_A1, xr >> 8);
	lcd_reg_data_write(RA8876_ELL_B0, yr);
	lcd_reg_data_write(RA8876_ELL_B1, yr >> 8);
	lcd_reg_data_write(RA8876_DCR1, RA8876_DRAW_CIRCLE_SQUARE);
}

static void lcd_drawCircleSquareFill(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t xr, uint16_t yr, uint16_t color)
{
	set_draw_params_rect(x0, y0, x1, y1, color);
	lcd_reg_data_write(RA8876_ELL_A0,xr);//77h
	lcd_reg_data_write(RA8876_ELL_A1,xr>>8);//78h
	lcd_reg_data_write(RA8876_ELL_B0,yr);//79h
	lcd_reg_data_write(RA8876_ELL_B1,yr>>8);//7ah
	lcd_reg_data_write(RA8876_DCR1, RA8876_DRAW_CIRCLE_SQUARE_FILL);
}

static void lcd_drawTriangle(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
	set_draw_params_rect(x0, y0, x1, y1, color);
	lcd_reg_data_write(RA8876_DTPH0,x2);//70h
	lcd_reg_data_write(RA8876_DTPH1,x2>>8);//71h
	lcd_reg_data_write(RA8876_DTPV0,y2);//72h
	lcd_reg_data_write(RA8876_DTPV1,y2>>8);//73h
	lcd_reg_data_write(RA8876_DCR0, RA8876_DRAW_TRIANGLE);
}

static void lcd_drawTriangleFill(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
	set_draw_params_rect(x0, y0, x1, y1, color);
	lcd_reg_data_write(RA8876_DTPH0,x2);//70h
	lcd_reg_data_write(RA8876_DTPH1,x2>>8);//71h
	lcd_reg_data_write(RA8876_DTPV0,y2);//72h
	lcd_reg_data_write(RA8876_DTPV1,y2>>8);//73h
	lcd_reg_data_write(RA8876_DCR0, RA8876_DRAW_TRIANGLE_FILL);//67h,0xa2
}

static void set_draw_params_circle(int x0, int y0, int xr, int yr, int color)
{
	foreGroundColor16bpp(color);
	lcd_reg_data_write(RA8876_DEHR0, x0);
	lcd_reg_data_write(RA8876_DEHR1, x0 >> 8);
	lcd_reg_data_write(RA8876_DEVR0, y0);
	lcd_reg_data_write(RA8876_DEVR1, y0 >> 8);
	lcd_reg_data_write(RA8876_ELL_A0, xr);
	lcd_reg_data_write(RA8876_ELL_A1, xr >> 8);
	lcd_reg_data_write(RA8876_ELL_B0, yr);
	lcd_reg_data_write(RA8876_ELL_B1, yr >> 8);
	lcd_check_2d_busy();
}

static void lcd_drawCircle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color)
{
	set_draw_params_circle(x0, y0, r, r, color);
	lcd_reg_data_write(RA8876_DCR1, RA8876_DRAW_CIRCLE);
}

static void lcd_drawCircleFill(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color)
{
	set_draw_params_circle(x0, y0, r, r, color);
	lcd_reg_data_write(RA8876_DCR1, RA8876_DRAW_CIRCLE_FILL);
}

static void lcd_drawEllipse(uint16_t x0, uint16_t y0, uint16_t xr, uint16_t yr, uint16_t color)
{
	set_draw_params_circle(x0, y0, xr, yr, color);
	lcd_reg_data_write(RA8876_DCR1, RA8876_DRAW_ELLIPSE);
}


static void lcd_drawEllipseFill(uint16_t x0, uint16_t y0, uint16_t xr, uint16_t yr, uint16_t color)
{
	set_draw_params_circle(x0, y0, xr, yr, color);
	lcd_reg_data_write(RA8876_DCR1,RA8876_DRAW_ELLIPSE_FILL);
}

static void lcd_bteMemoryCopy(uint32_t s0_addr, uint16_t s0_image_width, uint16_t s0_x, uint16_t s0_y, uint32_t des_addr, uint16_t des_image_width,
                                uint16_t des_x, uint16_t des_y, uint16_t copy_width, uint16_t copy_height)
{
	lcd_bte_Source0_MemoryStartAddr(s0_addr);
	lcd_bte_Source0_ImageWidth(s0_image_width);
	lcd_bte_Source0_WindowStartXY(s0_x,s0_y);
	lcd_bte_DestinationMemoryStartAddr(des_addr);
	lcd_bte_DestinationImageWidth(des_image_width);
	lcd_bte_DestinationWindowStartXY(des_x,des_y);
	lcd_bte_WindowSize(copy_width,copy_height);
	lcd_reg_data_write(RA8876_BTE_CTRL1, RA8876_BTE_ROP_CODE_12 << 4 | RA8876_BTE_MEMORY_COPY_WITH_ROP);
	lcd_reg_data_write(RA8876_BTE_COLR,RA8876_S0_COLOR_DEPTH_16BPP << 5 | \
			RA8876_S1_COLOR_DEPTH_16BPP << 2 | RA8876_DESTINATION_COLOR_DEPTH_16BPP);
	lcd_reg_data_write(RA8876_BTE_CTRL0,RA8876_BTE_ENABLE << 4);
	lcd_check_2d_busy();
}

static void lcd_bteMemoryCopyWithROP(uint32_t s0_addr, uint16_t s0_image_width, uint16_t s0_x, uint16_t s0_y, uint32_t s1_addr, uint16_t s1_image_width, uint16_t s1_x, uint16_t s1_y,
                                       uint32_t des_addr, uint16_t des_image_width, uint16_t des_x, uint16_t des_y, uint16_t copy_width, uint16_t copy_height, uint8_t rop_code)
{
	lcd_check_2d_busy();
	lcd_bte_Source0_MemoryStartAddr(s0_addr);
	lcd_bte_Source0_ImageWidth(s0_image_width);
	lcd_bte_Source0_WindowStartXY(s0_x,s0_y);
	lcd_bte_Source1_MemoryStartAddr(s1_addr);
	lcd_bte_Source1_ImageWidth(s1_image_width);
	lcd_bte_Source1_WindowStartXY(s1_x,s1_y);
	lcd_bte_DestinationMemoryStartAddr(des_addr);
	lcd_bte_DestinationImageWidth(des_image_width);
	lcd_bte_DestinationWindowStartXY(des_x,des_y);
	lcd_bte_WindowSize(copy_width,copy_height);
	lcd_reg_data_write(RA8876_BTE_CTRL1, rop_code << 4 | RA8876_BTE_MEMORY_COPY_WITH_ROP);
	lcd_reg_data_write(RA8876_BTE_COLR, RA8876_S0_COLOR_DEPTH_16BPP << 5 | \
			RA8876_S1_COLOR_DEPTH_16BPP << 2 | \
			RA8876_DESTINATION_COLOR_DEPTH_16BPP);
	lcd_reg_data_write(RA8876_BTE_CTRL0,RA8876_BTE_ENABLE << 4);
}


static void lcd_bteMemoryCopyWithChromaKey(uint32_t s0_addr, uint16_t s0_image_width, uint16_t s0_x, uint16_t s0_y,
					     uint32_t des_addr, uint16_t des_image_width, uint16_t des_x, uint16_t des_y, uint16_t copy_width, uint16_t copy_height, uint16_t chromakey_color)
{
	lcd_check_2d_busy();
	lcd_bte_Source0_MemoryStartAddr(s0_addr);
	lcd_bte_Source0_ImageWidth(s0_image_width);
	lcd_bte_Source0_WindowStartXY(s0_x,s0_y);
	lcd_bte_DestinationMemoryStartAddr(des_addr);
	lcd_bte_DestinationImageWidth(des_image_width);
	lcd_bte_DestinationWindowStartXY(des_x,des_y);
	lcd_bte_WindowSize(copy_width,copy_height);
	backGroundColor16bpp(chromakey_color);
	lcd_reg_data_write(RA8876_BTE_CTRL1, RA8876_BTE_MEMORY_COPY_WITH_CHROMA);
	lcd_reg_data_write(RA8876_BTE_COLR, RA8876_S0_COLOR_DEPTH_16BPP << 5 | \
			RA8876_S1_COLOR_DEPTH_16BPP << 2 | \
			RA8876_DESTINATION_COLOR_DEPTH_16BPP);
	lcd_reg_data_write(RA8876_BTE_CTRL0,RA8876_BTE_ENABLE<<4);
}


static void lcd_bteMpuWriteWithROP(uint32_t s1_addr, uint16_t s1_image_width, uint16_t s1_x, uint16_t s1_y, uint32_t des_addr, uint16_t des_image_width,
				     uint16_t des_x, uint16_t des_y, uint16_t width, uint16_t height, uint8_t rop_code,const unsigned char *data)
{
	lcd_bte_Source1_MemoryStartAddr(s1_addr);
	lcd_bte_Source1_ImageWidth(s1_image_width);
	lcd_bte_Source1_WindowStartXY(s1_x,s1_y);
	lcd_bte_DestinationMemoryStartAddr(des_addr);
	lcd_bte_DestinationImageWidth(des_image_width);
	lcd_bte_DestinationWindowStartXY(des_x,des_y);
	lcd_bte_WindowSize(width,height);
	lcd_reg_data_write(RA8876_BTE_CTRL1, rop_code << 4 | RA8876_BTE_MPU_WRITE_WITH_ROP);
	lcd_reg_data_write(RA8876_BTE_COLR, RA8876_S0_COLOR_DEPTH_16BPP << 5 | \
			RA8876_S1_COLOR_DEPTH_16BPP << 2 | \
			RA8876_DESTINATION_COLOR_DEPTH_16BPP);
	lcd_reg_data_write(RA8876_BTE_CTRL0, RA8876_BTE_ENABLE << 4);
	lcd_prepare_ram_access();

	for(int i = 0; i < height; i++) {
		for(int j = 0; j < (width * 2); j++) {
			lcd_write_fifo_not_full();
			lcd_data_write(*data);
			data++;
		}
	}
	lcd_write_fifo_empty();
}

static void lcd_bteMpuWriteWithROP(uint32_t s1_addr, uint16_t s1_image_width, uint16_t s1_x, uint16_t s1_y, uint32_t des_addr, uint16_t des_image_width,
uint16_t des_x, uint16_t des_y, uint16_t width, uint16_t height, uint8_t rop_code,const unsigned short *data)
{
	lcd_bte_Source1_MemoryStartAddr(s1_addr);
	lcd_bte_Source1_ImageWidth(s1_image_width);
	lcd_bte_Source1_WindowStartXY(s1_x,s1_y);
	lcd_bte_DestinationMemoryStartAddr(des_addr);
	lcd_bte_DestinationImageWidth(des_image_width);
	lcd_bte_DestinationWindowStartXY(des_x,des_y);
	lcd_bte_WindowSize(width,height);
	lcd_reg_data_write(RA8876_BTE_CTRL1, rop_code << 4 | \
			RA8876_BTE_MPU_WRITE_WITH_ROP);
	lcd_reg_data_write(RA8876_BTE_COLR, RA8876_S0_COLOR_DEPTH_16BPP << 5 | \
			RA8876_S1_COLOR_DEPTH_16BPP << 2 | \
			RA8876_DESTINATION_COLOR_DEPTH_16BPP);
	lcd_reg_data_write(RA8876_BTE_CTRL0, RA8876_BTE_ENABLE << 4);
	lcd_prepare_ram_access();

	for(int j = 0; j < height; j++) {
		for(int i = 0; i < width; i++) {
			lcd_write_fifo_not_full();//if high speed mcu and without Xnwait check
			lcd_data_write16(*data);
			data++;
			//lcd_write_fifo_empty();//if high speed mcu and without Xnwait check
		}
	}
	lcd_write_fifo_empty();
}

static void lcd_bteMpuWriteWithROP(uint32_t s1_addr, uint16_t s1_image_width, uint16_t s1_x, uint16_t s1_y, uint32_t des_addr, uint16_t des_image_width,
				     uint16_t des_x, uint16_t des_y, uint16_t width, uint16_t height, uint8_t rop_code)
{
	lcd_bte_Source1_MemoryStartAddr(s1_addr);
	lcd_bte_Source1_ImageWidth(s1_image_width);
	lcd_bte_Source1_WindowStartXY(s1_x,s1_y);
	lcd_bte_DestinationMemoryStartAddr(des_addr);
	lcd_bte_DestinationImageWidth(des_image_width);
	lcd_bte_DestinationWindowStartXY(des_x,des_y);
	lcd_bte_WindowSize(width,height);
	lcd_reg_data_write(RA8876_BTE_CTRL1, rop_code << 4 | \
			RA8876_BTE_MPU_WRITE_WITH_ROP);
	lcd_reg_data_write(RA8876_BTE_COLR, RA8876_S0_COLOR_DEPTH_16BPP << 5 | \
			RA8876_S1_COLOR_DEPTH_16BPP << 2 | \
			RA8876_DESTINATION_COLOR_DEPTH_16BPP);
	lcd_reg_data_write(RA8876_BTE_CTRL0, RA8876_BTE_ENABLE<<4);
	lcd_prepare_ram_access();
}


static void lcd_bteMpuWriteWithChromaKey(uint32_t des_addr, uint16_t des_image_width,
					   uint16_t des_x, uint16_t des_y, uint16_t width, uint16_t height,
					   uint16_t chromakey_color,const unsigned char *data)
{
	lcd_bte_DestinationMemoryStartAddr(des_addr);
	lcd_bte_DestinationImageWidth(des_image_width);
	lcd_bte_DestinationWindowStartXY(des_x,des_y);
	lcd_bte_WindowSize(width,height);
	backGroundColor16bpp(chromakey_color);
	lcd_reg_data_write(RA8876_BTE_CTRL1, RA8876_BTE_MPU_WRITE_WITH_CHROMA);
	lcd_reg_data_write(RA8876_BTE_COLR, RA8876_S0_COLOR_DEPTH_16BPP << 5 | \
			RA8876_S1_COLOR_DEPTH_16BPP << 2 | \
			RA8876_DESTINATION_COLOR_DEPTH_16BPP);
	lcd_reg_data_write(RA8876_BTE_CTRL0, RA8876_BTE_ENABLE<<4);
	lcd_prepare_ram_access();

	for(int i = 0; i < height; i++) {
		for(int j = 0; j < (width * 2); j++) {
			lcd_write_fifo_not_full();
			lcd_data_write(*data);
			data++;
		}
	}
	lcd_write_fifo_empty();
}



static void lcd_bteMpuWriteWithChromaKey(uint32_t des_addr, uint16_t des_image_width, uint16_t des_x, uint16_t des_y,
					   uint16_t width, uint16_t height, uint16_t chromakey_color,const unsigned short *data)
{
	lcd_bte_DestinationMemoryStartAddr(des_addr);
	lcd_bte_DestinationImageWidth(des_image_width);
	lcd_bte_DestinationWindowStartXY(des_x,des_y);
	lcd_bte_WindowSize(width,height);
	backGroundColor16bpp(chromakey_color);
	lcd_reg_data_write(RA8876_BTE_CTRL1, RA8876_BTE_MPU_WRITE_WITH_CHROMA);
	lcd_reg_data_write(RA8876_BTE_COLR, RA8876_S0_COLOR_DEPTH_16BPP << 5 | \
			RA8876_S1_COLOR_DEPTH_16BPP << 2 | \
			RA8876_DESTINATION_COLOR_DEPTH_16BPP);
	lcd_reg_data_write(RA8876_BTE_CTRL0, RA8876_BTE_ENABLE<<4);
	lcd_prepare_ram_access();

	for(int j = 0; j < height; j++) {
		for(int i = 0; i < width; i++) {
			lcd_write_fifo_not_full();//if high speed mcu and without Xnwait check
			lcd_data_write16(*data);
			data++;
			//lcd_write_fifo_empty();//if high speed mcu and without Xnwait check
		}
	}
	lcd_write_fifo_empty();
}

static void lcd_bteMpuWriteWithChromaKey(uint32_t des_addr, uint16_t des_image_width, uint16_t des_x, uint16_t des_y, uint16_t width, uint16_t height, uint16_t chromakey_color)
{
	lcd_bte_DestinationMemoryStartAddr(des_addr);
	lcd_bte_DestinationImageWidth(des_image_width);
	lcd_bte_DestinationWindowStartXY(des_x,des_y);
	lcd_bte_WindowSize(width,height);
	backGroundColor16bpp(chromakey_color);
	lcd_reg_data_write(RA8876_BTE_CTRL1, RA8876_BTE_MPU_WRITE_WITH_CHROMA);
	lcd_reg_data_write(RA8876_BTE_COLR, RA8876_S0_COLOR_DEPTH_16BPP << 5 | \
			RA8876_S1_COLOR_DEPTH_16BPP << 2 | \
			RA8876_DESTINATION_COLOR_DEPTH_16BPP);
	lcd_reg_data_write(RA8876_BTE_CTRL0,RA8876_BTE_ENABLE<<4);
	lcd_prepare_ram_access();
}


static void lcd_bteMpuWriteColorExpansion(uint32_t des_addr, uint16_t des_image_width, uint16_t des_x, uint16_t des_y,
					    uint16_t width, uint16_t height, uint16_t foreground_color, uint16_t background_color,
					    const unsigned char *data)
{
	lcd_check_2d_busy();
	lcd_bte_DestinationMemoryStartAddr(des_addr);
	lcd_bte_DestinationImageWidth(des_image_width);
	lcd_bte_DestinationWindowStartXY(des_x,des_y);
	lcd_bte_WindowSize(width, height);
	foreGroundColor16bpp(foreground_color);
	backGroundColor16bpp(background_color);
	lcd_reg_data_write(RA8876_BTE_CTRL1, RA8876_BTE_ROP_BUS_WIDTH8 << 4 | \
			RA8876_BTE_MPU_WRITE_COLOR_EXPANSION);
	lcd_reg_data_write(RA8876_BTE_COLR, RA8876_S0_COLOR_DEPTH_16BPP << 5 | \
			RA8876_S1_COLOR_DEPTH_16BPP << 2| \
			RA8876_DESTINATION_COLOR_DEPTH_16BPP);
	lcd_reg_data_write(RA8876_BTE_CTRL0,RA8876_BTE_ENABLE<<4);
	lcd_prepare_ram_access();

	for(int i = 0; i < height; i++) {
		for(int j = 0; j < width / 8; j++) {
			lcd_write_fifo_not_full();
			lcd_data_write(*data);
			data++;
		}
	}
	lcd_write_fifo_empty();
}

static void lcd_bteMpuWriteColorExpansion(uint32_t des_addr, uint16_t des_image_width,
					  uint16_t des_x, uint16_t des_y,
					  uint16_t width, uint16_t height,
					  uint16_t foreground_color, uint16_t background_color)
{
	lcd_bte_DestinationMemoryStartAddr(des_addr);
	lcd_bte_DestinationImageWidth(des_image_width);
	lcd_bte_DestinationWindowStartXY(des_x,des_y);
	lcd_bte_WindowSize(width,height);
	foreGroundColor16bpp(foreground_color);
	backGroundColor16bpp(background_color);
	lcd_reg_data_write(RA8876_BTE_CTRL1, RA8876_BTE_ROP_BUS_WIDTH8 << 4 | \
			RA8876_BTE_MPU_WRITE_COLOR_EXPANSION);
	lcd_reg_data_write(RA8876_BTE_COLR, RA8876_S0_COLOR_DEPTH_16BPP << 5 | \
			RA8876_S1_COLOR_DEPTH_16BPP << 2 | \
			RA8876_DESTINATION_COLOR_DEPTH_16BPP);
	lcd_reg_data_write(RA8876_BTE_CTRL0,RA8876_BTE_ENABLE << 4);
	lcd_prepare_ram_access();
}

static void lcd_bteMpuWriteColorExpansionWithChromaKey(uint32_t des_addr, uint16_t des_image_width, uint16_t des_x, uint16_t des_y,
							 uint16_t width, uint16_t height, uint16_t foreground_color, uint16_t background_color,
							 const unsigned char *data)
{
	lcd_check_2d_busy();
	lcd_bte_DestinationMemoryStartAddr(des_addr);
	lcd_bte_DestinationImageWidth(des_image_width);
	lcd_bte_DestinationWindowStartXY(des_x,des_y);
	lcd_bte_WindowSize(width,height);
	foreGroundColor16bpp(foreground_color);
	backGroundColor16bpp(background_color);
	lcd_reg_data_write(RA8876_BTE_CTRL1,RA8876_BTE_ROP_BUS_WIDTH8 << 4 | \
			RA8876_BTE_MPU_WRITE_COLOR_EXPANSION_WITH_CHROMA);
	lcd_reg_data_write(RA8876_BTE_COLR,RA8876_S0_COLOR_DEPTH_16BPP << 5 | \
			RA8876_S1_COLOR_DEPTH_16BPP << 2 | \
			RA8876_DESTINATION_COLOR_DEPTH_16BPP);
	lcd_reg_data_write(RA8876_BTE_CTRL0,RA8876_BTE_ENABLE << 4);
	lcd_prepare_ram_access();

	for(int i = 0;i < height; i++) {
		for(int j = 0; j < width / 8; j++) {
			lcd_write_fifo_not_full();
			lcd_data_write(*data);
			data++;
		}
	}
	lcd_write_fifo_empty();
}

static void lcd_bteMpuWriteColorExpansionWithChromaKey(uint32_t des_addr, uint16_t des_image_width, uint16_t des_x, uint16_t des_y,
						       uint16_t width, uint16_t height, uint16_t foreground_color, uint16_t background_color)
{
	lcd_bte_DestinationMemoryStartAddr(des_addr);
	lcd_bte_DestinationImageWidth(des_image_width);
	lcd_bte_DestinationWindowStartXY(des_x,des_y);
	lcd_bte_WindowSize(width,height);
	foreGroundColor16bpp(foreground_color);
	backGroundColor16bpp(background_color);
	lcd_reg_data_write(RA8876_BTE_CTRL1, RA8876_BTE_ROP_BUS_WIDTH8 << 4 | \
			RA8876_BTE_MPU_WRITE_COLOR_EXPANSION_WITH_CHROMA);
	lcd_reg_data_write(RA8876_BTE_COLR, RA8876_S0_COLOR_DEPTH_16BPP << 5 | \
			RA8876_S1_COLOR_DEPTH_16BPP << 2 | \
			RA8876_DESTINATION_COLOR_DEPTH_16BPP);
	lcd_reg_data_write(RA8876_BTE_CTRL0, RA8876_BTE_ENABLE << 4);
	lcd_prepare_ram_access();
}

static void lcd_btePatternFill(uint8_t p8x8or16x16, uint32_t s0_addr, uint16_t s0_image_width, uint16_t s0_x, uint16_t s0_y,
			       uint32_t des_addr, uint16_t des_image_width, uint16_t des_x, uint16_t des_y, uint16_t width, uint16_t height)
{
	lcd_check_2d_busy();
	lcd_bte_Source0_MemoryStartAddr(s0_addr);
	lcd_bte_Source0_ImageWidth(s0_image_width);
	lcd_bte_Source0_WindowStartXY(s0_x,s0_y);
	lcd_bte_DestinationMemoryStartAddr(des_addr);
	lcd_bte_DestinationImageWidth(des_image_width);
	lcd_bte_DestinationWindowStartXY(des_x,des_y);
	lcd_bte_WindowSize(width,height);
	lcd_reg_data_write(RA8876_BTE_CTRL1,RA8876_BTE_ROP_CODE_12 << 4 | \
			RA8876_BTE_PATTERN_FILL_WITH_ROP);
	lcd_reg_data_write(RA8876_BTE_COLR,RA8876_S0_COLOR_DEPTH_16BPP << 5 | \
			RA8876_S1_COLOR_DEPTH_16BPP << 2 | \
			RA8876_DESTINATION_COLOR_DEPTH_16BPP);

	if(p8x8or16x16 == 0)
		lcd_reg_data_write(RA8876_BTE_CTRL0, RA8876_BTE_ENABLE << 4 | RA8876_PATTERN_FORMAT8X8);
	else
		lcd_reg_data_write(RA8876_BTE_CTRL0, RA8876_BTE_ENABLE<<4 | RA8876_PATTERN_FORMAT16X16);
}

static void lcd_btePatternFillWithChromaKey(uint8_t p8x8or16x16, uint32_t s0_addr, uint16_t s0_image_width, uint16_t s0_x, uint16_t s0_y,
					    uint32_t des_addr, uint16_t des_image_width, uint16_t des_x, uint16_t des_y,
					    uint16_t width, uint16_t height, uint16_t chromakey_color)
{
	lcd_check_2d_busy();
	lcd_bte_Source0_MemoryStartAddr(s0_addr);
	lcd_bte_Source0_ImageWidth(s0_image_width);
	lcd_bte_Source0_WindowStartXY(s0_x,s0_y);
	lcd_bte_DestinationMemoryStartAddr(des_addr);
	lcd_bte_DestinationImageWidth(des_image_width);
	lcd_bte_DestinationWindowStartXY(des_x,des_y);
	lcd_bte_WindowSize(width,height);
	backGroundColor16bpp(chromakey_color);
	lcd_reg_data_write(RA8876_BTE_CTRL1, RA8876_BTE_ROP_CODE_12 << 4 | \
			RA8876_BTE_PATTERN_FILL_WITH_CHROMA);
	lcd_reg_data_write(RA8876_BTE_COLR, RA8876_S0_COLOR_DEPTH_16BPP << 5 | \
			RA8876_S1_COLOR_DEPTH_16BPP << 2 | \
			RA8876_DESTINATION_COLOR_DEPTH_16BPP);
	if(p8x8or16x16 == 0)
		lcd_reg_data_write(RA8876_BTE_CTRL0,RA8876_BTE_ENABLE << 4 | RA8876_PATTERN_FORMAT8X8);
	else
		lcd_reg_data_write(RA8876_BTE_CTRL0,RA8876_BTE_ENABLE << 4 | RA8876_PATTERN_FORMAT16X16);
}

static void lcd_setSerialFlash4BytesMode(uint8_t scs_select)
{
	lcd_reg_data_write(RA8876_SPIMCR2, scs_select << 5 | \
			RA8876_SPIM_MODE0);
	lcd_reg_data_write(RA8876_SPIMCR2, scs_select << 5 | \
			RA8876_SPIM_NSS_ACTIVE << 4 | \
			RA8876_SPIM_MODE0);
	lcd_reg_write(RA8876_SPIDR);
	delay(1);
	lcd_data_write(0xB7);
	delay(1);
	lcd_reg_data_write( RA8876_SPIMCR2, scs_select << 5 | \
			 RA8876_SPIM_NSS_INACTIVE << 4 | \
			 RA8876_SPIM_MODE0);
}

static void lcd_dma_24bitAddressBlockMode(uint8_t scs_select, uint8_t clk_div, uint16_t x0, uint16_t y0,
					  uint16_t width, uint16_t height, uint16_t picture_width, uint32_t addr)
{
	lcd_check_2d_busy();
	lcd_reg_data_write(RA8876_SFL_CTRL, scs_select <<7 | \
			RA8876_SERIAL_FLASH_DMA_MODE << 6 |   \
			RA8876_SERIAL_FLASH_ADDR_24BIT << 5 | \
			RA8876_FOLLOW_RA8876_MODE << 4 | \
			RA8876_SPI_FAST_READ_8DUMMY);
	lcd_reg_data_write(RA8876_SPI_DIVSOR, clk_div);
	lcd_reg_data_write(RA8876_DMA_DX0, x0);
	lcd_reg_data_write(RA8876_DMA_DX1, x0 >> 8);
	lcd_reg_data_write(RA8876_DMA_DY0, y0);
	lcd_reg_data_write(RA8876_DMA_DY1, y0 >> 8);
	lcd_reg_data_write(RA8876_DMAW_WTH0, width);
	lcd_reg_data_write(RA8876_DMAW_WTH1, width >> 8);
	lcd_reg_data_write(RA8876_DMAW_HIGH0, height);
	lcd_reg_data_write(RA8876_DMAW_HIGH1, height >> 8);
	lcd_reg_data_write(RA8876_DMA_SWTH0, picture_width);
	lcd_reg_data_write(RA8876_DMA_SWTH1, picture_width >> 8);
	lcd_reg_data_write(RA8876_DMA_SSTR0, addr);
	lcd_reg_data_write(RA8876_DMA_SSTR1, addr >> 8);
	lcd_reg_data_write(RA8876_DMA_SSTR2, addr >> 16);
	lcd_reg_data_write(RA8876_DMA_SSTR3, addr >> 24);
	lcd_reg_data_write(RA8876_DMA_CTRL, RA8876_DMA_START);
}

static void lcd_dma_32bitAddressBlockMode(uint8_t scs_select, uint8_t clk_div, uint16_t x0, uint16_t y0,
					  uint16_t width, uint16_t height, uint16_t picture_width, uint32_t addr)
{
	lcd_check_2d_busy();
	lcd_reg_data_write(RA8876_SFL_CTRL, scs_select <<7 | \
			RA8876_SERIAL_FLASH_DMA_MODE << 6 | \
			RA8876_SERIAL_FLASH_ADDR_32BIT << 5 | \
			RA8876_FOLLOW_RA8876_MODE << 4 | \
			RA8876_SPI_FAST_READ_8DUMMY);
	lcd_reg_data_write(RA8876_SPI_DIVSOR,clk_div);
	lcd_reg_data_write(RA8876_DMA_DX0,x0);
	lcd_reg_data_write(RA8876_DMA_DX1,x0 >> 8);
	lcd_reg_data_write(RA8876_DMA_DY0,y0);
	lcd_reg_data_write(RA8876_DMA_DY1,y0 >> 8);
	lcd_reg_data_write(RA8876_DMAW_WTH0,width);
	lcd_reg_data_write(RA8876_DMAW_WTH1,width >> 8);
	lcd_reg_data_write(RA8876_DMAW_HIGH0,height);
	lcd_reg_data_write(RA8876_DMAW_HIGH1,height >> 8);
	lcd_reg_data_write(RA8876_DMA_SWTH0,picture_width);
	lcd_reg_data_write(RA8876_DMA_SWTH1,picture_width >> 8);
	lcd_reg_data_write(RA8876_DMA_SSTR0,addr);
	lcd_reg_data_write(RA8876_DMA_SSTR1,addr >> 8);
	lcd_reg_data_write(RA8876_DMA_SSTR2,addr >> 16);
	lcd_reg_data_write(RA8876_DMA_SSTR3,addr >> 24);
	lcd_reg_data_write(RA8876_DMA_CTRL,RA8876_DMA_START);
}
#endif

static void draw_bargraph(int x, int y, int width, int height)
{
	lcd_textColor(COLOR65K_WHITE, COLOR65K_BLACK);
	lcd_drawSquare(x, y, x + width, y + height, COLOR65K_WHITE);
}

static void draw_screen(void)
{
	lcd_drawSquareFill(0, 0, 1023, 32, COLOR65K_GRAYSCALE15);
	lcd_textColor(COLOR65K_CYAN, COLOR65K_GRAYSCALE15);
	lcd_putString(0, 0, "DM1SV 1200W PA");
	lcd_drawLine(0, 33, 1023, 33, COLOR65K_WHITE);
	lcd_drawLine(0, 600 - 100, 1023, 600 - 100, COLOR65K_WHITE);
	for (int i = 0; i < NUM_BUTTONS; i++)
		lcd_drawLine(1024 / NUM_BUTTONS * i, 500, 1024 / NUM_BUTTONS * i, 600, COLOR65K_WHITE);
	lcd_drawLine(1023, 500, 1023, 600, COLOR65K_WHITE);
	activate_menu(main_menu);
}

static void add_bargraph_marker(int x, int y, int width, float value, float min, float max, const char *text)
{
	int pos = get_x(x - 1, width + 2, value - min, max - min);

	lcd_drawLine(pos, y, pos, y + 10, COLOR65K_WHITE);
	lcd_printf(pos - (strlen(text) * 8) / 2, y + 10, COLOR65K_WHITE, COLOR65K_BLACK, text);
}

static float scale_swr(float value)
{
	return 30 * logf(value);
}

void draw_bargraphs(void)
{
	// Output
	lcd_printf(20, 50, COLOR65K_WHITE, COLOR65K_BLACK, "Forward Power");
	draw_bargraph(20, 100, 900, 30);
	lcd_setTextParameter1(RA8876_SELECT_INTERNAL_CGROM, RA8876_CHAR_HEIGHT_16, RA8876_SELECT_8859_1);
	add_bargraph_marker(20, 130, 900, WtoDBM(1), 30, 61, "1");
	add_bargraph_marker(20, 130, 900, WtoDBM(2), 30, 61, "2");
	add_bargraph_marker(20, 130, 900, WtoDBM(5), 30, 61, "5");
	add_bargraph_marker(20, 130, 900, WtoDBM(10), 30, 61, "10");
	add_bargraph_marker(20, 130, 900, WtoDBM(25), 30, 61, "25");
	add_bargraph_marker(20, 130, 900, WtoDBM(50), 30, 61, "50");
	add_bargraph_marker(20, 130, 900, WtoDBM(100), 30, 61, "100");
	add_bargraph_marker(20, 130, 900, WtoDBM(200), 30, 61, "200");
	add_bargraph_marker(20, 130, 900, WtoDBM(500), 30, 61, "500");
	add_bargraph_marker(20, 130, 900, WtoDBM(750), 30, 61, "750");
	add_bargraph_marker(20, 130, 900, WtoDBM(1000), 30, 61, "1000");
	add_bargraph_marker(20, 130, 900, WtoDBM(1200), 30, 61, "1200");
	lcd_setTextParameter1(RA8876_SELECT_INTERNAL_CGROM, RA8876_CHAR_HEIGHT_32, RA8876_SELECT_8859_1);

	lcd_printf(20, 175, COLOR65K_WHITE, COLOR65K_BLACK, "Reflected Power");
	draw_bargraph(20, 225, 400, 30);
	lcd_setTextParameter1(RA8876_SELECT_INTERNAL_CGROM, RA8876_CHAR_HEIGHT_16, RA8876_SELECT_8859_1);
	add_bargraph_marker(20, 255, 400, WtoDBM(1), 30, 53, "1");
	add_bargraph_marker(20, 255, 400, WtoDBM(2), 30, 53, "2");
	add_bargraph_marker(20, 255, 400, WtoDBM(5), 30, 53, "5");
	add_bargraph_marker(20, 255, 400, WtoDBM(10), 30, 53, "10");
	add_bargraph_marker(20, 255, 400, WtoDBM(25), 30, 53, "25");
	add_bargraph_marker(20, 255, 400, WtoDBM(50), 30, 53, "50");
	add_bargraph_marker(20, 255, 400, WtoDBM(100), 30, 53, "100");
	add_bargraph_marker(20, 255, 400, WtoDBM(200), 30, 53, "200");
	lcd_setTextParameter1(RA8876_SELECT_INTERNAL_CGROM, RA8876_CHAR_HEIGHT_32, RA8876_SELECT_8859_1);

	lcd_printf(525, 175, COLOR65K_WHITE, COLOR65K_BLACK, "SWR");
	draw_bargraph(525, 225, 400, 30);
	lcd_setTextParameter1(RA8876_SELECT_INTERNAL_CGROM, RA8876_CHAR_HEIGHT_16, RA8876_SELECT_8859_1);
	add_bargraph_marker(525, 255, 400, scale_swr(1), 0, 100, "1");
	add_bargraph_marker(525, 255, 400, scale_swr(1.1), 0, 100, "");
	add_bargraph_marker(525, 255, 400, scale_swr(1.2), 0, 100, "");
	add_bargraph_marker(525, 255, 400, scale_swr(1.3), 0, 100, "");
	add_bargraph_marker(525, 255, 400, scale_swr(1.4), 0, 100, "");
	add_bargraph_marker(525, 255, 400, scale_swr(1.5), 0, 100, "1.5");
	add_bargraph_marker(525, 255, 400, scale_swr(2), 0, 100, "2");
	add_bargraph_marker(525, 255, 400, scale_swr(3), 0, 100, "3");
	add_bargraph_marker(525, 255, 400, scale_swr(5), 0, 100, "5");
	add_bargraph_marker(525, 255, 400, scale_swr(10), 0, 100, "10");
	add_bargraph_marker(525, 255, 400, scale_swr(20), 0, 100, "20");
	lcd_setTextParameter1(RA8876_SELECT_INTERNAL_CGROM, RA8876_CHAR_HEIGHT_32, RA8876_SELECT_8859_1);

	lcd_printf(20, 300, COLOR65K_WHITE, COLOR65K_BLACK, "Input Power");
	draw_bargraph(20, 350, 400, 30);
	lcd_setTextParameter1(RA8876_SELECT_INTERNAL_CGROM, RA8876_CHAR_HEIGHT_16, RA8876_SELECT_8859_1);
	add_bargraph_marker(20, 380, 400, WtoDBM(1), 30, 53, "1");
	add_bargraph_marker(20, 380, 400, WtoDBM(2), 30, 53, "2");
	add_bargraph_marker(20, 380, 400, WtoDBM(5), 30, 53, "5");
	add_bargraph_marker(20, 380, 400, WtoDBM(10), 30, 53, "10");
	add_bargraph_marker(20, 380, 400, WtoDBM(25), 30, 53, "25");
	add_bargraph_marker(20, 380, 400, WtoDBM(50), 30, 53, "50");
	add_bargraph_marker(20, 380, 400, WtoDBM(100), 30, 53, "100");
	add_bargraph_marker(20, 380, 400, WtoDBM(200), 30, 53, "200");
	lcd_setTextParameter1(RA8876_SELECT_INTERNAL_CGROM, RA8876_CHAR_HEIGHT_32, RA8876_SELECT_8859_1);


	lcd_printf(525, 300, COLOR65K_WHITE, COLOR65K_BLACK, "Current");
	draw_bargraph(525, 350, 400, 30);
	lcd_setTextParameter1(RA8876_SELECT_INTERNAL_CGROM, RA8876_CHAR_HEIGHT_16, RA8876_SELECT_8859_1);
	add_bargraph_marker(525, 380, 400, 0, 0, 40, "0");
	add_bargraph_marker(525, 380, 400, 5, 0, 40, "5");
	add_bargraph_marker(525, 380, 400, 10, 0, 40, "10");
	add_bargraph_marker(525, 380, 400, 15, 0, 40, "15");
	add_bargraph_marker(525, 380, 400, 20, 0, 40, "20");
	add_bargraph_marker(525, 380, 400, 25, 0, 40, "25");
	add_bargraph_marker(525, 380, 400, 30, 0, 40, "30");
	add_bargraph_marker(525, 380, 400, 35, 0, 40, "35");
	add_bargraph_marker(525, 380, 400, 40, 0, 40, "40");
	lcd_setTextParameter1(RA8876_SELECT_INTERNAL_CGROM, RA8876_CHAR_HEIGHT_32, RA8876_SELECT_8859_1);


	// Filter
//	draw_bargraph(525, 300, 400, 30);
//	draw_bargraph(525, 350, 400, 30);
//	draw_bargraph(525, 400, 400, 30);
}

void clear_bargraphs(void)
{
	lcd_drawSquareFill(0, 34, 1023, 499, COLOR65K_BLACK);
}

void setup_lcd(void)
{
	while(!lcd_begin());
	lcd_display_on(true);
	lcd_canvasImageStartAddress(PAGE1_START_ADDR);
	lcd_canvasImageWidth(SCREEN_WIDTH);
	lcd_drawSquareFill(0, 0, 1023, 599, COLOR65K_BLACK);
	lcd_setTextParameter1(RA8876_SELECT_INTERNAL_CGROM, RA8876_CHAR_HEIGHT_32, RA8876_SELECT_8859_1);
	lcd_setTextParameter2(RA8876_TEXT_FULL_ALIGN_DISABLE, RA8876_TEXT_CHROMA_KEY_DISABLE,
			      RA8876_TEXT_WIDTH_ENLARGEMENT_X1, RA8876_TEXT_HEIGHT_ENLARGEMENT_X1);

	draw_screen();

	lcd_setTextParameter1(RA8876_SELECT_INTERNAL_CGROM, RA8876_CHAR_HEIGHT_32,
			      RA8876_SELECT_8859_1);
//	lcd_setTextParameter2(RA8876_TEXT_FULL_ALIGN_DISABLE, RA8876_TEXT_CHROMA_KEY_ENABLE,
//			      RA8876_TEXT_WIDTH_ENLARGEMENT_X2, RA8876_TEXT_HEIGHT_ENLARGEMENT_X2);
//	lcd_selectExternalFont(RA8876_FONT_FAMILY_FIXED, RA8876_FONT_SIZE_16, RA8876_FONT_ENCODING_ASCII);
//	lcd_genitopCharacterRomParameter(RA8876_SERIAL_FLASH_SELECT0, RA8876_SPI_DIV4, RA8876_GT30L24T3Y,
//					 RA8876_ASCII, RA8876_GT_FIXED_WIDTH);
//	lcd_initExternalFontRom(0, RA8876_FONT_ROM_GT30L16U2W);
//	lcd_selectExternalFont(RA8876_FONT_FAMILY_FIXED, RA8876_FONT_SIZE_16, RA8876_FONT_ENCODING_ASCII);
//
//	lcd_setTextParameter1(RA8876_SELECT_EXTERNAL_CGROM, RA8876_CHAR_HEIGHT_16,
//			      RA8876_CHAR_WIDTH_16);
}

int status_display(int color, const char *fmt, ...)
{
	char buf[128];
	va_list va;
	int ret;

	va_start(va, fmt);
	ret = vsnprintf(buf, sizeof(buf), fmt, va);
	va_end(va);
	lcd_textColor(color, COLOR65K_GRAYSCALE15);
	lcd_putString(1023 - strlen(buf) * 16, 0, buf);
	lcd_drawSquareFill(600, 0, 1023 - strlen(buf) * 16, 32, COLOR65K_GRAYSCALE15);
	return ret;
}

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

static int get_x(int start, int width, float value, float max)
{
	return (value / max) * (width - 2) + (start + 1);
}

static void __update_bargraph(int x, int y, int width, int height,
			      float value, float peak, float max)
{
	int endg, endx, endy = y + height - 1;

	if (value < 0)
		value = 0;

	if (value > max)
		value = max;

	if (peak > max)
		peak = max;

	if (peak < value)
		peak = value;

	endg = get_x(x, width, value, max);
	lcd_drawSquareFill(x + 1, y + 1, endg, endy, COLOR65K_GREEN);
	endx = get_x(x, width, peak, max);
	lcd_drawSquareFill(endg + 1, y + 1, endx, endy,  COLOR65K_LIGHTRED);
	lcd_drawSquareFill(endx + 1, y + 1, x + width - 2, endy, COLOR65K_BLACK);
}

void update_bargraph(int x, int y, int width, int height, float min,
		     float value, float max, const char *fmt)
{
	__update_bargraph(x, y, width, height, value - min, -1, max - min);
	lcd_printf(x + width + 10, y, COLOR65K_WHITE, COLOR65K_BLACK, fmt, value);
}

void update_bargraph_peak(int x, int y, int width, int height, float min,
			  float value, float peak, float max, float fmtvalue,
			  const char *fmt)
{
	__update_bargraph(x, y, width, height, value - min, peak - min, max - min);
	lcd_printf(x + width + 10, y, COLOR65K_WHITE, COLOR65K_BLACK, fmt, fmtvalue);
}

int lcd_printf(int x, int y, int fg, int bg, const char *fmt, ...)
{
	char buf[128];
	va_list va;
	int ret;

	va_start(va, fmt);
	ret = vsnprintf(buf, sizeof(buf), fmt, va);
	va_end(va);
	lcd_textColor(fg, bg);
	lcd_putString(x, y, buf);
	return ret;
}

void lcd_printf_button(int num, int line, const char *fmt, ...)
{
	int width = 1024 / NUM_BUTTONS;
	int startx = num * width;
	int starty = 510 + line * 40;
//	int endx = (num + 1) * width;
	char buf[32];
	va_list va;

	va_start(va, fmt);
	vsnprintf(buf, sizeof(buf), fmt, va);
	va_end(va);
	lcd_drawSquareFill(startx + 1, starty, startx + width - 1, starty + 32, COLOR65K_GRAYSCALE15);
	lcd_printf(startx + (width - strlen(buf) * 16) / 2, starty, line ? COLOR65K_WHITE : COLOR65K_GREEN, COLOR65K_GRAYSCALE15, buf);
}

