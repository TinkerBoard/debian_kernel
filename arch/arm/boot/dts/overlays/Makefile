# Overlays for the Tinker board platform

dtbo-$(CONFIG_ARCH_ROCKCHIP) += \
	hifiberry-amp.dtbo \
	hifiberry-dac.dtbo \
	hifiberry-dacplus.dtbo \
	iqaudio-dacplus.dtbo \
	waveshare35a-tinker.dtbo \
	waveshare32b-tinker.dtbo \

targets += dtbs dtbs_install
targets += $(dtbo-y)

always		:= $(dtbo-y)
clean-files	:= *.dtbo
