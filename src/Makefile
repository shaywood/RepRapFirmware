# Makefile for RepRapFirmware
# licensed under the terms of the GPL v3
#
# written by Christian Hammacher, 2016

VERSION := 1.16-b1
DATE := 2016-10-22

GCC_VERSION := 4.8.3-2014q1
DUET_BOARD_VERSION := 2.0.4

# For Duet 0.6 / 0.8.5
BOSSAC_VERSION := 1.3a-arduino
# For DuetNG
BOSSAC_4E_PATH := ~/duet/sam-ba_cdc_linux/sam-ba_64

OPTIMISATION := -O2


# Get Arduino path
UNAME := $(shell uname -s)
ifeq ($(UNAME),Linux)
  ARDUINO_PATH := $(HOME)/.arduino15
endif
ifeq ($(UNAME),Darwin)
  ARDUINO_PATH := $(HOME)/Library/Arduino15
endif
ifeq (,$(wildcard $(ARDUINO_PATH)/.))
  $(error Arduino directory not found! Please install it first.)
endif

# Detect Duet board path
DUET_BOARD_PATH := $(ARDUINO_PATH)/packages/RepRap/hardware/sam/$(DUET_BOARD_VERSION)
ifeq (,$(wildcard $(DUET_BOARD_PATH)/.))
  $(error Duet board not found! Install it first via the Boards Manager in Arduino IDE.)
endif

# Detect Duet library path
DUET_LIBRARY_PATH := $(DUET_BOARD_PATH)/libraries
ifeq (,$(wildcard $(DUET_LIBRARY_PATH)/.))
  $(error Duet libraries not found! Check your installation.)?)
endif

# Get GCC path
GCC_PATH := $(ARDUINO_PATH)/packages/arduino/tools/arm-none-eabi-gcc/$(GCC_VERSION)
ifeq (,$(wildcard $(GCC_PATH)/.))
  $(error GCC toolchain not found! Check your installation.)
endif

# Get bossac path
BOSSAC_PATH := $(ARDUINO_PATH)/packages/arduino/tools/bossac/$(BOSSAC_VERSION)/bossac
ifeq (,$(wildcard $(BOSSAC_PATH)))
  $(warning Bossac not found! Uploading compiled binaries will not work.)
endif

# Prepare GCC paths
CROSS_COMPILE := arm-none-eabi-
CC := $(GCC_PATH)/bin/$(CROSS_COMPILE)gcc
CXX := $(GCC_PATH)/bin/$(CROSS_COMPILE)g++
LD := $(GCC_PATH)/bin/$(CROSS_COMPILE)gcc
OBJCOPY := $(GCC_PATH)/bin/$(CROSS_COMPILE)objcopy


# ================================= Target all ======================================

.PHONY := all
all: duet duetng

duet:
	@echo "------------ Building RepRapFirmware for Duet ------------"
	@$(MAKE) --no-print-directory CC=$(CC) CXX=$(CXX) LD=$(LD) OBJCOPY=$(OBJCOPY) OPTIMISATION=$(OPTIMISATION) DUET_BOARD_PATH=$(DUET_BOARD_PATH) DUET_LIBRARY_PATH=$(DUET_LIBRARY_PATH) VERSION=$(VERSION) DATE=$(DATE) -f $(PWD)/Duet/duet.mk

duetng:
	@echo "----------- Building RepRapFirmware for DuetNG -----------"
	@$(MAKE) --no-print-directory CC=$(CC) CXX=$(CXX) LD=$(LD) OBJCOPY=$(OBJCOPY) OPTIMISATION=$(OPTIMISATION) DUET_BOARD_PATH=$(DUET_BOARD_PATH) DUET_LIBRARY_PATH=$(DUET_LIBRARY_PATH) VERSION=$(VERSION) DATE=$(DATE) -f $(PWD)/DuetNG/duetNG.mk


# ================================= Target clean ====================================
.PHONY += clean
clean:
	@$(MAKE) --no-print-directory -f $(PWD)/Duet/duet.mk clean
	@$(MAKE) --no-print-directory -f $(PWD)/DuetNG/duetNG.mk clean


# ================================= Target upload ===================================
.PHONY += upload_duet upload_duetng
upload_duet:
	@$(MAKE) --no-print-directory CC=$(CC) CXX=$(CXX) LD=$(LD) OBJCOPY=$(OBJCOPY) OPTIMISATION=$(OPTIMISATION) DUET_BOARD_PATH=$(DUET_BOARD_PATH) DUET_LIBRARY_PATH=$(DUET_LIBRARY_PATH) BOSSAC_PATH=$(BOSSAC_PATH) VERSION=$(VERSION) DATE=$(DATE) -f $(PWD)/Duet/duet.mk upload

upload_duetng:
	@$(MAKE) --no-print-directory CC=$(CC) CXX=$(CXX) LD=$(LD) OBJCOPY=$(OBJCOPY) OPTIMISATION=$(OPTIMISATION) DUET_BOARD_PATH=$(DUET_BOARD_PATH) DUET_LIBRARY_PATH=$(DUET_LIBRARY_PATH) BOSSAC_4E_PATH=$(BOSSAC_4E_PATH) VERSION=$(VERSION) DATE=$(DATE) -f $(PWD)/DuetNG/duetNG.mk upload
