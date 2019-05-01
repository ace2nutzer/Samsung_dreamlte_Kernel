# Exynos FMP makefile
obj-$(CONFIG_FIPS_FMP) += first_file.o
obj-$(CONFIG_FMP_UFS) += fmp_ufs.o
obj-$(CONFIG_FMP_UFS) += fmp_derive_iv.o
obj-$(CONFIG_FIPS_FMP) += fmpdev.o fmp_integrity.o fmplib.o
obj-$(CONFIG_FIPS_FMP) += hmac_fmp.o sha256_fmp.o
obj-$(CONFIG_FIPS_FMP) += fmp_ufs_fips.o
obj-$(CONFIG_FIPS_FMP) += last_file.o

obj-$(CONFIG_FMP_MMC) += fmp_mmc.o

ccflags-$(CONFIG_FMP_UFS) += -Idrivers/scsi/ufs
ccflags-$(CONFIG_FMP_MMC) += -Idrivers/mmc/host
