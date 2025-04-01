#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor_data_types.h>

#define SERVICE_DATA_LEN        12
#define SERVICE_UUID            0xfcd2
#define IDX_TEMPL               4
#define IDX_TEMPH               5
#define IDX_HUML                7
#define IDX_HUMH                8

#define IDX_PM2_5L              10
#define IDX_PM2_5H              11
#define IDX_PM10L               13
#define IDX_PM10H               14

#define ADV_PARAM BT_LE_ADV_PARAM(BT_LE_ADV_OPT_USE_IDENTITY, \
                                  BT_GAP_ADV_SLOW_INT_MIN, \
                                  BT_GAP_ADV_SLOW_INT_MAX, NULL)


static uint8_t service_data[SERVICE_DATA_LEN] = {
        BT_UUID_16_ENCODE(SERVICE_UUID),
        0x40,
        0x02,        /* Temperature */
        0xDE,        /* Low byte */
        0xAD,        /* High byte */
        0x03,        /* Humidity */
        0xBE,        /* Low byte */
        0xEF,        /* High byte*/
        0x0D,        /* PM2.5 */
        0xDE,        /* Low byte */
        0xAD,        /* High byte */
       // 0x0E,        /* PM10 */
       // 0xBE,        /* Low byte */
      //  0xEF,        /* High byte*/

};

static struct bt_data ad[] = {
        BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
        BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
        BT_DATA(BT_DATA_SVC_DATA16, service_data, ARRAY_SIZE(service_data))
};

const struct device * const dev = DEVICE_DT_GET_ANY(bosch_bme680);
static struct device const * const particle_sensor = DEVICE_DT_GET(DT_NODELABEL(my_device));

static void bt_ready(int result)
{
        if (0 != result) {
                printk("Bluetooth init failed (err %d)\n", result);
        } else {
                printk("Bluetooth initialized\n");

                result = bt_le_adv_start(ADV_PARAM, ad, ARRAY_SIZE(ad), NULL, 0);

                if (0 != result) {
                        printk("Advertising failed to start (err %d)\n", result);
                }
        }
}

int main(void)
{
        int err;
        int result;
        uint16_t temp_temp;
        uint16_t humidity_temp;
        uint16_t pm2_5_temp;
        uint16_t pm10_temp;

        struct sensor_value temperature, humidity, pm1_0, pm2_5, pm10;

        printk("Starting BTHome sensor template\n");

        if (NULL == dev) {
                printk("BME680 not found");
                result = -1;
        } else {
                result = device_is_ready(dev) ? 0 : -EAGAIN;

                if (0 != result) {
                        printk("\nError: Device \"%s\" is not ready; "
                               "check the driver initialization logs for errors.\n",
                               dev->name);
                }
        }

        if (0 == result) {
                result = bt_enable(bt_ready);

                if (0 != result) {
                        printk("Bluetooth init failed (err %d)\n", err);
                        // Code style exception for the sake of readability
                        return -EAGAIN;
                }
        }


        for (;;) {

                result = sensor_sample_fetch(dev);

                if (0 == result) {
                        result = sensor_channel_get(dev,
                                                    SENSOR_CHAN_AMBIENT_TEMP,
                                                    &temperature);

                }

                if (0 == result) {
                        result = sensor_channel_get(dev,
                                                    SENSOR_CHAN_HUMIDITY,
                                                    &humidity);
                }

                if (0 == result) {
                        result = sensor_sample_fetch(particle_sensor);
                }

                if (0 == result) {
                        result = sensor_channel_get(particle_sensor,
                                                    SENSOR_CHAN_PM_1_0,
                                                    &pm1_0);
                }

                if (0 == result) {
                        result = sensor_channel_get(particle_sensor,
                                                    SENSOR_CHAN_PM_2_5,
                                                    &pm2_5);
                }

                if (0 == result) {
                        result = sensor_channel_get(particle_sensor,
                                                    SENSOR_CHAN_PM_10,
                                                    &pm10);
                }

                if (0 == result) {
                        temp_temp = (temperature.val1 * 100 + temperature.val2 / 10000);
                        humidity_temp = (humidity.val1 * 100 + humidity.val2 / 10000);
                        pm2_5_temp = (pm2_5.val1);
                        pm10_temp = (pm10.val1);


                        service_data[IDX_TEMPH] = ((uint8_t *)(&temp_temp))[1];
                        service_data[IDX_TEMPL] = ((uint8_t *)(&temp_temp))[0];
                        service_data[IDX_HUMH] = ((uint8_t *)(&humidity_temp))[1];
                        service_data[IDX_HUML] = ((uint8_t *)(&humidity_temp))[0];

                        service_data[IDX_PM2_5L] = ((uint8_t *)(&pm2_5_temp))[0];
                        service_data[IDX_PM2_5H] = ((uint8_t *)(&pm2_5_temp))[1];
                       // service_data[IDX_PM10L] = ((uint8_t *)(&pm10_temp))[0];
                   //     service_data[IDX_PM10H] = ((uint8_t *)(&pm10_temp))[1];

                        printk("%d.%02dC, %d.%02d %%RH, PM1.0 %d %d %d\n",
                               temperature.val1,
                               temperature.val2 / 10000,
                               humidity.val1,
                               humidity.val2 / 10000,
                               pm1_0.val1,
                               pm2_5.val1,
                               pm10.val1
                               );

                        result = bt_le_adv_update_data(ad, ARRAY_SIZE(ad), NULL, 0);

                        if (0 != result) {
                                printk("Failed to update advertising data (err %d)\n", result);
                        }
                }

                k_sleep(K_MSEC(BT_GAP_ADV_SLOW_INT_MIN));
        }
        return 0;
}
