#
# Copyright (C) 2008-2012 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=gpio-dht-handler
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define KernelPackage/gpio-dht-handler
  SUBMENU:=Other modules
  DEPENDS:=@!LINUX_3_3
  TITLE:=DHT11 and DHT22 protocol handler
  FILES:=$(PKG_BUILD_DIR)/gpio-dht-handler.ko
  AUTOLOAD:=$(call AutoLoad,30,gpio-dht-handler,1)
  KCONFIG:=
endef

define KernelPackage/gpio-dht-handler/description
 This is a DHT11/DHT22 temperature and humidity sensor kernel module for AR9331 devices.
endef

MAKE_OPTS:= \
	ARCH="$(LINUX_KARCH)" \
	CROSS_COMPILE="$(TARGET_CROSS)" \
	SUBDIRS="$(PKG_BUILD_DIR)"

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" \
		$(MAKE_OPTS) \
		modules
endef

$(eval $(call KernelPackage,gpio-dht-handler))
