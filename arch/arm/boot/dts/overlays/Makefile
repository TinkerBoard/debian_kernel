# Overlays for the Tinker board platform

dtbo-$(CONFIG_ARCH_ROCKCHIP) += \
	hifiberry-amp.dtbo \
	hifiberry-dac.dtbo \
	hifiberry-dacplus.dtbo \
	hifiberry-digi.dtbo \
	hifiberry-digi-pro.dtbo \
	iqaudio-dac.dtbo \
	iqaudio-dacplus.dtbo \
	iqaudio-digi-wm8804-audio.dtbo \
	justboom-dac.dtbo \
	justboom-digi.dtbo \
	waveshare35a-tinker.dtbo \
	waveshare32b-tinker.dtbo \
	pps-gpio.dtbo \
	w1-gpio.dtbo \
	pitft35-resistive-tinker.dtbo \
	ds1307.dtbo \
	lirc-rpi.dtbo \
	lirc-toshiba.dtbo \

targets += dtbs dtbs_install
targets += $(dtbo-y)

always		:= $(dtbo-y)
clean-files	:= *.dtbo
