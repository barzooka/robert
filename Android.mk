LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:=                                      \
                  robert.cpp              \
                  sendevent.cpp \
                  getevent.cpp

LOCAL_MODULE:= robert

LOCAL_C_INCLUDES := 

LOCAL_CFLAGS :=

LOCAL_MODULE_TAGS:= optional

include $(BUILD_EXECUTABLE)

