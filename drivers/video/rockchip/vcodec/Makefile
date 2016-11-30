rk-vcodec-objs := vcodec_service.o vcodec_iommu_ops.o vcodec_iommu_drm.o

ifdef CONFIG_ION
rk-vcodec-objs += vcodec_iommu_ion.o
endif

obj-$(CONFIG_RK_VCODEC) += rk-vcodec.o
