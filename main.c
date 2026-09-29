#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/counter.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#define ECHO_TIMEOUT_US 30000U
#define VEL_SONIDO 343U

static const struct gpio_dt_spec trigger =
	GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), trigger_gpios);
static const struct gpio_dt_spec echo =
	GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), echo_gpios);
static const struct device *const timer = DEVICE_DT_GET(DT_NODELABEL(echo_counter));

void main(void)
{
	uint32_t start, now, elapsed, top, timeout;
	int level, ret;
	uint32_t distance_milli_cm;

	if (!gpio_is_ready_dt(&trigger) || !gpio_is_ready_dt(&echo) ||
	    !device_is_ready(timer)) {
		printk("GPIO o timer no listo\n");
		return;
	}
	if (gpio_pin_configure_dt(&trigger, GPIO_OUTPUT_INACTIVE) < 0 ||
	    gpio_pin_configure_dt(&echo, GPIO_INPUT) < 0 || counter_start(timer) < 0) {
		printk("Error configurando GPIO o timer\n");
		return;
	}

	top = counter_get_top_value(timer);
	timeout = counter_us_to_ticks(timer, ECHO_TIMEOUT_US);

	while (1) {
		gpio_pin_set_dt(&trigger, 1);
		k_busy_wait(10);
		gpio_pin_set_dt(&trigger, 0);

		counter_get_value(timer, &start);
		while (gpio_pin_get_dt(&echo) == 0) {
			counter_get_value(timer, &now);
			elapsed = (now >= start) ? now - start : top - start + now + 1U;
			if (elapsed >= timeout) {
				break;
			}
		}
		level = gpio_pin_get_dt(&echo);
		if (level != 1 || elapsed >= timeout) {
			printk("Timeout esperando flanco de subida\n");
			k_msleep(60);
			continue;
		}

		counter_get_value(timer, &start);
		while (gpio_pin_get_dt(&echo) == 1) {
			counter_get_value(timer, &now);
			elapsed = (now >= start) ? now - start : top - start + now + 1U;
			if (elapsed >= timeout) {
				break;
			}
		}
		level = gpio_pin_get_dt(&echo);
		if (level != 0 || elapsed >= timeout) {
			printk("Timeout esperando flanco de bajada\n");
		} else {
			ret = counter_get_value(timer, &now);
			if (ret == 0) {
				elapsed = (now >= start) ? now - start : top - start + now + 1U;
				distance_milli_cm = elapsed * VEL_SONIDO / 20U;
				printk("Tiempo de vuelo: %u us, Distancia: %u.%u%u%u cm\n",
				       (unsigned int)counter_ticks_to_us(timer, elapsed),
				       distance_milli_cm / 1000U,
				       (distance_milli_cm / 100U) % 10U,
				       (distance_milli_cm / 10U) % 10U,
				       distance_milli_cm % 10U);
			}
		}
		k_msleep(600);
	}
}