# Bulb components

This component encapsulates a variety of dimming schemes commonly used in bulb lamps, and uses an abstraction layer to manage these schemes, which is convenient for developers to integrate, and provides many common functions such as power limit, color calibration, breathing/gradient, etc., supported The dimming scheme is as follows:

- PWM Direct Drive Solution

  * RGB three-way + C/W two-way: RGB colored light and C/W white light channel mixed light output
  * RGB three-way + CCT/Brightness two-way: RGB color light channel mixed light output, white light uses a single way to control the color temperature, and the other way to control the brightness

- IIC Dimming Chip Solution

  * SM2135E
  * SM2135EH
  * SM2235EGH
  * SM2335EGH
  * BP5758/BP5758D/BP5768D
  * BP1658CJ
  * KP18058
- Single bus solution
  * WS2812
## Example of using PWM direct drive scheme
The PWM direct drive solution has two control methods for the 5-way bulb lamp, RGB three-way + C/W two-way and RGB three-way + CCT/Brightness two-way, the main difference is that the latter uses a separate hardware channel to control the color temperature And brightness, there is no need for a program to calculate the color mixing ratio, so the accuracy of the color temperature is the highest in this method, and the former needs to calculate the ratio of cool color and warm color lamp bead output according to the required color temperature.

The usage examples are as follows:
```

lightbulb_config_t config = {
     //1. Select PWM output and configure parameters
     .type = DRIVER_ESP_PWM,
     .driver_conf.pwm.freq_hz = 4000,

     //2. Function selection, enable/disable according to your needs
     .capability.enable_fades = true,
     .capability.fades_ms = 800,
     .capability.enable_lowpower = false,
     /* If your driver's white light output is software color mixing instead of hardware independent control, you need to enable this function */
     .capability.enable_mix_cct = false,
     .capability.enable_status_storage = true,
     /* for configuration is 3 or 2 or 5 output mode */
     .capability.mode_mask = COLOR_MODE,
     .capability.storage_cb = NULL,
     .capability.sync_change_brightness_value = true,

     //3. Configure hardware pins for PWM output
     .io_conf.pwm_io.red = 25,
     .io_conf.pwm_io.green = 26,
     .io_conf.pwm_io.blue = 27,

     //4. Limit parameters, please refer to the following sections for detailed rules
     .external_limit = NULL,

     //5. Color calibration parameters
     .gamma_conf = NULL,

     //6. Initialize the lighting parameters, if on is set, the bulb will be turned on when the driver is initialized
     .init_status.mode = WORK_COLOR,
     .init_status.on = true,
     .init_status.hue = 0,
     .init_status.saturation = 100,
     .init_status.value = 100,
};
lightbulb_init(&config);

```

## Example of using IIC dimming chip solution
This component already supports a variety of IIC dimming chips. For the specific functions and parameters of the dimming chips, please refer to the chip manual.

The usage examples are as follows:
```

lightbulb_config_t config = {
     //1. Select the required chip and configure the parameters. The parameters of each chip configuration are different, please refer to the chip manual carefully
     .type = DRIVER_SM2135E,
     .driver_conf.sm2135e.rgb_current = SM2135E_RGB_CURRENT_20MA,
     .driver_conf.sm2135e.wy_current = SM2135E_WY_CURRENT_40MA,
     .driver_conf.sm2135e.iic_clk = 4,
     .driver_conf.sm2135e.iic_sda = 5,
     .driver_conf.sm2135e.freq_khz = 400,
     .driver_conf.sm2135e.enable_iic_queue = true,

     //2. Drive function selection, enable/disable according to your needs
     .capability.enable_fades = true,
     .capability.fades_ms = 800,
     .capability.enable_lowpower = false,

     /* For IIC scheme, this option must be configured as enabled */
     .capability.enable_mix_cct = true,
     .capability.enable_status_storage = true,
     .capability.mode_mask = COLOR_AND_WHITE_MODE,
     .capability.storage_cb = NULL,
     .capability.sync_change_brightness_value = true,

     //3. Configure the hardware pins of the IIC chip
     .io_conf.iic_io.red = OUT3,
     .io_conf.iic_io.green = OUT2,
     .io_conf.iic_io.blue = OUT1,
     .io_conf.iic_io.cold_white = OUT5,
     .io_conf.iic_io.warm_yellow = OUT4,

     //4. Limit parameters, please refer to the following sections for detailed rules
     .external_limit = NULL,

     //5. Color calibration parameters
     .gamma_conf = NULL,

     //6. Initialize the lighting parameters, if on is set, the bulb will be turned on when the driver is initialized
     .init_status.mode = WORK_COLOR,
     .init_status.on = true,
     .init_status.hue = 0,
     .init_status.saturation = 100,
     .init_status.value = 100,
};
lightbulb_init(&config);

```

## Example of using a single-bus solution
This component uses the SPI driver to output the data required by WS2812, and the data encapsulation sequence is GRB.
```

lightbulb_config_t config = {
     //1. Select WS2812 output and configure parameters
     .type = DRIVER_WS2812,
     .driver_conf.ws2812.led_num = 22,
     .driver_conf.ws2812.ctrl_io = 4,

     //2. Drive function selection, enable/disable according to your needs
     .capability.enable_fades = true,
     .capability.fades_ms = 800,
     .capability.enable_status_storage = true,

     /* For WS2812 only COLOR_MODE can be selected */
     .capability.mode_mask = COLOR_MODE,
     .capability.storage_cb = NULL,

     //3. Limit parameters, please refer to the following sections for detailed rules
     .external_limit = NULL,

     //4. Color calibration parameters
     .gamma_conf = NULL,

     //5. Initialize the lighting parameters, if on is set, the bulb will be turned on when the driver is initialized
     .init_status.mode = WORK_COLOR,
     .init_status.on = true,
     .init_status.hue = 0,
     .init_status.saturation = 100,
     .init_status.value = 100,
};
lightbulb_init(&config);

```

## Instructions for Limiting Parameter Usage
The main purpose of the limit parameter is to limit the maximum output power and limit the brightness parameter to a range. The color light and white light of this component can be controlled independently, so there are 2 sets of maximum/minimum brightness parameters and power parameters. The color light uses the HSV model, the value represents the brightness of the color light, and the white light uses the brightness parameter. The value and brightness data input range is 0 <= x <= 100.

If the brightness limit parameter is set, the input value will be scaled proportionally, for example, we set the following parameters


```
lightbulb_power_limit_t limit = {
     .white_max_brightness = 100,
     .white_min_brightness = 10,
     .color_max_value = 100,
     .color_min_value = 10,
     .white_max_power = 100,
     .color_max_power = 100
}

```

The relationship between value and brightness input and output is as follows:


| input | output |
| ----- | ------ |
| 100   | 100    |
| 80    | 82     |
| 50    | 55     |
| 11    | 19     |
| 1     | 10     |
| 0     | 0      |

The power limit is further carried out after the brightness parameter limit. For the RGB channel, the adjustment range is 100 <= x <= 300. The relationship between input and output is as follows:


| input       | output(color_max_power = 100) | output(color_max_power = 200) | output(color_max_power = 300) |
| ----------- | ----------------------------- | ----------------------------- | ----------------------------- |
| 255,255,0   | 127,127,0                     | 255,255,0                     | 255,255,0                     |
| 127,127,0   | 63,63,0                       | 127,127,0                     | 127,127,0                     |
| 63,63,0     | 31,31,0                       | 63,63,0                       | 63,63,0                       |
| 255,255,255 | 85,85,85                      | 170,170,170                   | 255,255,255                   |
| 127,127,127 | 42,42,42                      | 84,84,84                      | 127,127,127                   |
| 63,63,63    | 21,21,21                      | 42,42,42                      | 63,63,63                      |



## Color Calibration Parameters

The color calibration parameters consist of 2 parts, which are used to generate the curve coefficient x_curve_coe of the gamma grayscale table and

- White balance coefficient r_balance_coe for final adjustme
- The value of the curve coefficient is 0.8 <= x <= 2.2, and the output of each parameter is as follows
    | x = 1.0 | x < 1.0 | x > 1.0
```c

   |  x = 1.0                           | x < 1.0                          | x > 1.0
max|                                    |                                  |
   |                                *   |                     *            |                           *
   |                             *      |                  *               |                          *
   |                          *         |               *                  |                         *
   |                       *            |             *                    |                       *
   |                    *               |           *                      |                     *
   |                 *                  |         *                        |                   *
   |              *                     |       *                          |                 *
   |           *                        |     *                            |              *
   |        *                           |    *                             |           * 
   |     *                              |   *                              |        *
   |  *                                 |  *                               |  *
0  |------------------------------------|----------------------------------|------------------------------   
  0               ...                255 
```

The component will generate a gamma calibration table based on the maximum input value supported by each driver, and all 8bit RGB will be converted to calibrated values. It is recommended to set the same coefficient for RGB channels.

The value of the white balance coefficient is 0.5-1.0, and the final fine-tuning is performed on the data. The calculation rule is output value = input value * coefficient, and different coefficients can be set for each channel.
## sample code
[Click here](https://github.com/espressif/esp-iot-solution/tree/master/examples/lighting/lightbulb) for sample code and usage instructions.
