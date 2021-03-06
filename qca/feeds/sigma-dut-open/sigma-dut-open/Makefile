include $(TOPDIR)/rules.mk

PKG:=sigma-dut
PKG_NAME:=sigma-dut-open
PKG_VERSION:=20190930
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=git://github.com/qca/sigma-dut.git
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG)-$(PKG_VERSION)

WIFI_ath9k:=PACKAGE_kmod-ath9k
WIFI_ath10k:=PACKAGE_kmod-ath10k
WIFI_ath11k:=PACKAGE_kmod-ath11k
WIFI_wigig:=PACKAGE_kmod-qca-wigig-driver
WIFI_OPEN:=$(WIFI_ath9k)||$(WIFI_ath10k)||$(WIFI_ath11k)||$(WIFI_wigig)


include $(INCLUDE_DIR)/local-development.mk

ifeq ($(DUMP)$(PKG_VERSION),)
  PKG_REV:=$(shell git ls-remote $(PKG_SOURCE_URL) $(PKG_BRANCH) | cut -b -7)
  PKG_VERSION:=g$(PKG_REV)
endif

PKG_SOURCE_SUBDIR:=$(PKG)-$(PKG_VERSION)
PKG_SOURCE_VERSION=2cffae58e352d724ad21318bb243971a6ab33d3c
PKG_SOURCE:=$(PKG)-$(PKG_VERSION)-$(PKG_SOURCE_VERSION).tar.gz

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION:=QCA
  CATEGORY:=QCA software
  DEPENDS:=+libpthread @$(WIFI_OPEN)
  TITLE:=Sigma DUT
endef

define Package/$(PKG_NAME)/description
  $(PKG_NAME) is a control agent for automating Wi-Fi Alliance certification tests
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/sbin $(1)/etc/init.d $(1)/etc/config
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sigma_dut $(1)/usr/sbin
	$(INSTALL_BIN) ./files/sigma_dut.init $(1)/etc/init.d/$(PKG)
	$(INSTALL_BIN) ./files/$(PKG).config $(1)/etc/config/$(PKG)
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
