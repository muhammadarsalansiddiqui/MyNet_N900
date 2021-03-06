/*
 * arch/ubicom32/mach-ip7k/board-vir200.c
 *   Support for VIR200 Internet Audio Player
 *
 * (C) Copyright 2009-2010, Ubicom, Inc.
 *
 * This file is part of the Ubicom32 Linux Kernel Port.
 *
 * The Ubicom32 Linux Kernel Port is free software: you can redistribute
 * it and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Ubicom32 Linux Kernel Port is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the Ubicom32 Linux Kernel Port.  If not, 
 * see <http://www.gnu.org/licenses/>.
 */
#include <linux/device.h>
#include <linux/gpio.h>
#include <asm/board.h>

#include <linux/delay.h>

#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>

#include <asm/vdc_tio.h>

#include <asm/ubicom32bl.h>
#include <asm/ubicom32lcdpower.h>

#include <asm/machdep.h>

#include <asm/flash.h>
#include <asm/ubi32-fc.h>
#include <asm/ubi32-nand-spi-er.h>

#include <asm/adv7180.h>

#include <asm/audio.h>
#include <asm/ubi32-pcm.h>
#include <asm/ubi32-cs4525.h>

/* using resistive touch adapter board */
//#define TSC2007

/******************************************************************************
 * ubicom32fb VDC platform data
 */
struct ubicom32fb_platform_data vir200_ubicom32fb_platform_data __initdata;

/******************************************************************************
 * Flash controller support
 */

/*
 * vir200_nand_select
 */
static void vir200_nand_select(void *appdata)
{
	gpio_set_value(GPIO_RC_31, 1);
}

/*
 * vir200_nor_select
 */
static void vir200_nor_select(void *appdata)
{
	gpio_set_value(GPIO_RC_31, 0);
}

/******************************************************************************
 * Jog Dial platform data
 */
#include <asm/ubicom32jog.h>
static struct ubicom32jog_platform_data vir200_jog_platform_data = {
	.has_button		= 1,
	.button_poll_interval	= 80,
	.button_gpio		= GPIO_RB_14,
	.jog_gpio_a		= GPIO_RB_15,
	.jog_gpio_b		= GPIO_RA_7,
};

static struct platform_device vir200_jog_device = {
	.name		= "ubicom32jog",
	.dev		= {
		.platform_data = &vir200_jog_platform_data,
	},

};

/******************************************************************************
 * i2c devices
 */

/******************************************************************************
 * /dev/i2c-0: SDA RB6, SCL RB5 - CS4525
 */
static struct i2c_gpio_platform_data vir200_i2c_data = {
	.sda_pin		= GPIO_RB_6,
	.scl_pin		= GPIO_RB_5,
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.udelay			= 25,
};

static struct platform_device vir200_i2c_device = {
	.name	= "i2c-gpio",
	.id	= 0,
	.dev	= {
		.platform_data = &vir200_i2c_data,
	},
};

/*
 * Devices on /dev/i2c-0
 */
static struct i2c_board_info __initdata vir200_i2c_board_info[] = {

	/*
	 * U6, CS4525 DAC, address 0x4A
	 */
	{
		.type		= "cs4525",
		.addr		= 0x4A,
	},
};


/******************************************************************************
 * /dev/i2c-1: SDA RB7, SCL RB8
 */
static struct i2c_gpio_platform_data vir200_i2c_1_data = {
	.sda_pin		= GPIO_RB_8,
	.scl_pin		= GPIO_RB_7,
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.udelay			= 50,
};

static struct platform_device vir200_i2c_1_device = {
	.name	= "i2c-gpio",
	.id	= 1,
	.dev	= {
		.platform_data = &vir200_i2c_1_data,
	},
};

/******************************************************************************
 * /dev/i2c-2: SDA RB10, SCL RB9 - Cap touch
 */
static struct i2c_gpio_platform_data vir200_i2c_2_data = {
	.sda_pin		= GPIO_RB_10,
	.scl_pin		= GPIO_RB_9,
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.udelay			= 10,
};

static struct platform_device vir200_i2c_2_device = {
	.name	= "i2c-gpio",
	.id	= 2,
	.dev	= {
		.platform_data = &vir200_i2c_2_data,
	},
};

/*
 * Touch controller
 *
 * Connected via I2C bus, interrupt on PA6
 */
#ifdef TSC2007

#include <linux/i2c/tsc2007.h>

/*
 * vir200_tsc2007_exit_platform_hw
 */
static void vir200_tsc2007_exit_platform_hw(void)
{
	UBICOM32_IO_PORT(IO_PORT_RA)->ctl0 &= ~(0x03 << 17);
	gpio_free(GPIO_RA_5);
}

/*
 * vir200_tsc2007_init_platform_hw
 */
static int vir200_tsc2007_init_platform_hw(void)
{
	int res = gpio_request(GPIO_RA_5, "TSC2007_IRQ");
	if (res) {
		printk(KERN_ERR "%s: could not request TOUCH IRQ %d\n", __FUNCTION__, GPIO_RA_5);
		return res;
	}
	UBICOM32_IO_PORT(IO_PORT_RA)->ctl0 &= ~(0x03 << 17);
	UBICOM32_IO_PORT(IO_PORT_RA)->ctl0 |= (0x02 << 17);
	return 0;
}

/*
 * vir200_tsc2007_get_pendown_state
 */
static int vir200_tsc2007_get_pendown_state(void)
{
	return !gpio_get_value(GPIO_RA_5);
}

static struct tsc2007_platform_data vir200_tsc2007_data = {
	.model                  = 2007,
	.x_plate_ohms           = 350,
	.get_pendown_state      = vir200_tsc2007_get_pendown_state,
	.init_platform_hw       = vir200_tsc2007_init_platform_hw,
	.exit_platform_hw       = vir200_tsc2007_exit_platform_hw,
};

static struct i2c_board_info __initdata vir200_i2c_2_board_info[] = {
	{
		.type           = "tsc2007",
		.addr           = 0x48,
		.irq            = 45, // PA5  
		.platform_data  = &vir200_tsc2007_data,
	},
};

#else /* using cap touch driver */

#include <linux/i2c/zy070.h>

/*
 * vir200_zy070_exit_platform_hw
 */
static void vir200_zy070_exit_platform_hw(void)
{
	UBICOM32_IO_PORT(IO_PORT_RA)->ctl0 &= ~(0x03 << 17);
	gpio_free(GPIO_RA_5);
}

/*
 * vir200_zy070_init_platform_hw
 */
static int vir200_zy070_init_platform_hw(void)
{
	int res = gpio_request(GPIO_RA_5, "ZY070_IRQ");
	if (res) {
		printk(KERN_ERR "%s: could not request TOUCH IRQ %d\n", __FUNCTION__, GPIO_RA_5);
		return res;
	}
	UBICOM32_IO_PORT(IO_PORT_RA)->ctl0 &= ~(0x03 << 17);
	UBICOM32_IO_PORT(IO_PORT_RA)->ctl0 |= (0x02 << 17);
	return 0; 
}

static struct zy070_platform_data vir200_zy070_data = {
	.maxx = 800,
	.maxy = 480,
	.flags = ZY070_FLIP_Y,
	.init_platform_hw	= vir200_zy070_init_platform_hw,
	.exit_platform_hw	= vir200_zy070_exit_platform_hw,
};

static struct i2c_board_info __initdata vir200_i2c_2_board_info[] = {
	{
		.type		= "zy070",
		.addr		= 0x20,
		.irq		= 45, // PA5
		.platform_data	= &vir200_zy070_data,
	}
};

#endif /* end of resistive / cap touch driver */

/******************************************************************************
 * /dev/i2c-3: SDA RB4, SCL RB3 - RTC, light Sensor and Video ADC
 */
static struct i2c_gpio_platform_data vir200_i2c_3_data = {
	.sda_pin		= GPIO_RB_4,
	.scl_pin		= GPIO_RB_3,
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.udelay			= 25,
};

static struct platform_device vir200_i2c_3_device = {
	.name	= "i2c-gpio",
	.id	= 3,
	.dev	= {
		.platform_data = &vir200_i2c_3_data,
	},
};

/*
 * Devices on /dev/i2c-3
 */
static struct i2c_board_info __initdata vir200_i2c_3_board_info[] = {
	/*
	 * U20, S35390A RTC, address 0x30
	 */
	{
		.type           = "s35390a",
		.addr           = 0x30,
	},

	/*
	 * U20, ISL29011, address 0x44
	 */
	{
		.type           = "isl29011",
		.addr           = 0x44,
	},

#if 0
	/*
	 * U24, ADV7180, address 0x42
	 */
	{
		.type           = "adv7180",
		.addr           = 0x42,
	},
#endif
};

/******************************************************************************
 * /dev/i2c-4: SDA RB17, SCL RB18 - FM Tuner
 */
static struct i2c_gpio_platform_data vir200_i2c_4_data = {
	.sda_pin		= GPIO_RB_17,
	.scl_pin		= GPIO_RB_18,
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.udelay			= 25,
};

static struct platform_device vir200_i2c_4_device = {
	.name	= "i2c-gpio",
	.id	= 4,
	.dev	= {
		.platform_data = &vir200_i2c_4_data,
	},
};

/******************************************************************************
 * LCD PON for LCD Adapter board
 */
static struct ubicom32lcdpower_platform_data vir200_lcdpower_data = {
	.vgh_gpio               = GPIO_RD_2,
	.vgh_polarity		= true,
};

static struct platform_device vir200_lcdpower_device = {
	.name	= "ubicom32lcdpower",
	.id	= -1,
	.dev	= {
		.platform_data = &vir200_lcdpower_data,
	},
};

/******************************************************************************
 * Backlight on the board PD0, hardware PWM
 */
static struct ubicom32bl_platform_data vir200_backlight_data = {
	.type			= UBICOM32BL_TYPE_PWM,
	.pwm_channel		= 2,
	.pwm_prescale		= 15,
	.pwm_period		= 60,
	.default_intensity	= 0xFF,
};

static struct platform_device vir200_backlight_device = {
	.name	= "ubicom32bl",
	.id	= -1,
	.dev	= {
		.platform_data = &vir200_backlight_data,
	},
};

/******************************************************************************
 * Devices on this board
 */
static struct platform_device *vir200_devices[] __initdata = {
	&vir200_i2c_device,
	&vir200_i2c_1_device,
	&vir200_i2c_2_device,
	&vir200_i2c_3_device,
	&vir200_i2c_4_device,
	&vir200_jog_device,
	&vir200_lcdpower_device,
	&vir200_backlight_device,
};

/*
 * vir200_init
 *	Called to add the devices which we have on this board
 */
static int __init vir200_init(void)
{
	struct platform_device *audio_dev;
	void *tmp;

	board_init();
	ubi_gpio_init();

	/*
	 * Flash storage devices
	 *      NAND selected when PC31 is HIGH
	 *      NOR selected when PC31 is LOW
	 */
	ubicom32_flash_init();
	if (gpio_request(GPIO_RC_31, "FLASH SEL")) {
		printk(KERN_ERR "%s: Could not request FLASH_SEL\n", __FUNCTION__);
	}
	gpio_direction_output(GPIO_RC_31, 0);

	tmp = ubicom32_flash_alloc(vir200_nor_select, NULL, NULL);
	if (!tmp) {
		printk(KERN_ERR "%s: Could not alloc NOR flash\n", __FUNCTION__);
	} else {
		struct ubicom32fc_platform_data pd;
		memset(&pd, 0, sizeof(pd));
		pd.select = ubicom32_flash_select;
		pd.unselect = ubicom32_flash_unselect;
		pd.appdata = tmp;
		if (ubicom32_flash_add("ubicom32fc", 0, &pd, sizeof(pd))) {
			printk(KERN_ERR "%s: Could not add NOR flash\n", __FUNCTION__);
		}
	}

	tmp = ubicom32_flash_alloc(vir200_nand_select, NULL, NULL);
	if (!tmp) {
		printk(KERN_ERR "%s: Could not alloc NAND flash\n", __FUNCTION__);
	} else {
		struct ubi32_nand_spi_er_platform_data pd;
		memset(&pd, 0, sizeof(pd));
		pd.select = ubicom32_flash_select;
		pd.unselect = ubicom32_flash_unselect;
		pd.appdata = tmp;
		if (ubicom32_flash_add("ubi32-nand-spi-er", 0, &pd, sizeof(pd))) {
			printk(KERN_ERR "%s: Could not add NAND flash\n", __FUNCTION__);
		}
	}

	/*
	 * Bring up board devices
	 */
	platform_add_devices(vir200_devices, ARRAY_SIZE(vir200_devices));

	/*
	 * Initialize the display
	 */
	vdc_tio_fill_platform_data(&vir200_ubicom32fb_platform_data);
	printk(KERN_INFO "%s: initializing video driver\n", __FUNCTION__);
	vdc_tio_init(&vir200_ubicom32fb_platform_data);

	/*
	 * CS4525 DAC
	 */
	audio_dev = audio_device_alloc("snd-ubi32-cs4525", "audio", "audio-i2sout", 
				       sizeof(struct ubi32_cs4525_platform_data));
	if (audio_dev) {
		struct ubi32_cs4525_platform_data *cs4525_pd;

		cs4525_pd = audio_device_priv(audio_dev);
		cs4525_pd->power_enable_pin = GPIO_RE_2;
		cs4525_pd->power_enable_polarity = 1;
		cs4525_pd->reset_pin = GPIO_RB_12;
		vir200_i2c_board_info[0].platform_data = audio_dev;
	}

	/*
	 * Touch controller reset
	 */
	if (gpio_request(GPIO_RD_3, "Touch Reset")) {
		printk(KERN_ERR "%s: could not request Touch Rest GPIO\n", __FUNCTION__);
	}
	gpio_direction_output(GPIO_RD_3, 1);
	udelay(1);
	gpio_set_value(GPIO_RD_3, 0);

	printk(KERN_INFO "%s: registering i2c resources\n", __FUNCTION__);
	i2c_register_board_info(0, vir200_i2c_board_info, ARRAY_SIZE(vir200_i2c_board_info));
	i2c_register_board_info(2, vir200_i2c_2_board_info, ARRAY_SIZE(vir200_i2c_2_board_info));
	i2c_register_board_info(3, vir200_i2c_3_board_info, ARRAY_SIZE(vir200_i2c_3_board_info));

	printk(KERN_INFO "VIR200\n");

	return 0;
}

arch_initcall(vir200_init);

/*
 * vir200_late_init
 *	Called late to setup ADV7180
 */
static int __init vir200_late_init(void)
{
	/*
	 * Video In
	 */
	if (adv7180_init(3, 0x21, GPIO_RC_27, 4)) {
		printk(KERN_WARNING "Could not init ADV7180\n");
	}

	/*
	 * Always select ADV_HS
	 */
	if (gpio_request(GPIO_RC_29, "INT_AB1") == 0) {
		gpio_direction_output(GPIO_RC_29, 1);
	}

	if (gpio_request(GPIO_RC_24, "ADV_HS") == 0) {
		gpio_direction_input(GPIO_RC_24);
	}

	return 0;
}
late_initcall(vir200_late_init);

