LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := ptr_inject.sh

LOCAL_CPPFLAGS := -std=c++17 -fexceptions -fno-rtti -fvisibility=hidden -ffunction-sections -fdata-sections -w
LOCAL_LDFLAGS  += -Wl,--gc-sections,--strip-all
LOCAL_ARM_MODE := arm

LOCAL_SRC_FILES := inject.cpp
LOCAL_LDLIBS    := -llog

include $(BUILD_EXECUTABLE)
