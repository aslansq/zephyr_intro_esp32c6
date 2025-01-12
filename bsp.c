
#include "bsp.h"
#include "uart.h"
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define PIN_NUM 8

static const struct device *gpio_ct_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
// we need to do delay with macros because required delays are two fast
#define DELAY(x)    for(uint64_t delayIdx = 0; delayIdx < (x); ++delayIdx) {__asm__("nop");}
#define SET_PIN(st) gpio_pin_set_raw(gpio_ct_dev,PIN_NUM,(st))
// magic numbers oh well: I just look with scope to meet timing requirements
// see requirements below
// https://cdn-shop.adafruit.com/datasheets/WS2812B.pdf
#define SEND0()     SET_PIN(1);DELAY(8);SET_PIN(0);DELAY(25)
#define SEND1()     SET_PIN(1);DELAY(22);SET_PIN(0);DELAY(9)
#define RESET()     SET_PIN(0);k_usleep(50)

uint8_t bsp_rgb_led_init(void) {
	int32_t ret;
	bsp_rgb_led_color_s val = {
		.r = 0,
		.g = 0,
		.b = 0
	};

	if(!device_is_ready(gpio_ct_dev)) {
		return 1;
	}

	ret = gpio_pin_configure(
		gpio_ct_dev,
		PIN_NUM,
		GPIO_OUTPUT_ACTIVE
	);
	if(ret == 0) {
		RESET();
		bsp_rgb_led_set(&val);
	}

	return ret;
}

void bsp_rgb_led_set(bsp_rgb_led_color_s *val) {
	unsigned int key = irq_lock();
	uint32_t data = 0;
	data |= (val->g << 16);
	data |= (val->r << 8);
	data |= val->b;
	for(uint8_t i = 0; i < 24; ++i) {
		if(data & (0x800000)) { // check 23th bit
			SEND1();
		} else {
			SEND0();
		}
		data = data << 1;
	}
	irq_unlock(key);
	RESET();
}

void bsp_rgb_led_change(void) {
	static bsp_rgb_led_color_s val = {
		.r = 0,
		.g = 0,
		.b = 0
	};
	static uint8_t valIdx = 0;
	val.r = 0;
	val.g = 0;
	val.b = 0;
	if(valIdx >= 3) {
		valIdx = 0;
	}
	switch(valIdx) {
		case 0:
			val.r = 255;
			break;
		case 1:
			val.g = 255;
			break;
		case 2:
			val.b = 255;
			break;
		default:
			val.r = 255;
			val.g = 255;
			val.b = 255;
			break;
	}

	bsp_rgb_led_set(&val);

	valIdx++;
}

char bsp_getchar(void) {
	char c = -1;
	uart_rx_one_char(&c);
	return c;
}

int16_t bsp_getline(char *buf, uint8_t size) {
	int16_t ret = 0;
	char c = bsp_getchar();
	uint8_t bufIdx;
	for(bufIdx = 0; bufIdx < size;) {
		// ignore non ascii chars
		if(c < 0 || c > 127 || c == '\r') {
			// do nothing
		} else if(c == '\n') {
			buf[bufIdx] = '\0';
			bufIdx++;
			ret = bufIdx;
			break;
		} else if(bufIdx == (size-1)){
			ret = -1;
			break;
		} else {
			buf[bufIdx] = c;
			bufIdx++;
		}
		c = bsp_getchar();
		k_yield();
	}
	return ret;
}