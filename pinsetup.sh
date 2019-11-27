#!/bin/bash
#
# configure all lcd pins for driving the display
for pin in 11 12 13 15 16 17 18 19 20 21 22 24 26 ; do
    config-pin P9.${pin} out
done

# configure P9.14 as PWM for DI / dimmer
config-pin P9.14 pwm

# prepare dimmer and set to 'off' low brightness
pwmchip=`ls /sys/devices/platform/ocp/48302000.epwmss/48302200.pwm/pwm/`
pwm=`echo -en /sys/class/pwm/${pwmchip}/pwm-?:0`
if [ ! -e "${pwm}" ] ; then
  echo 0 > /sys/class/pwm/${pwmchip}/export
  pwm=`echo -en /sys/class/pwm/${pwmchip}/pwm-?:0`
fi
echo 50000 > ${pwm}/period
echo 49900 > ${pwm}/duty_cycle
echo 1 > ${pwm}/enable

