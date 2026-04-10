# Android.mk for moonlight-core and binding
MY_LOCAL_PATH := $(call my-dir)

PROJECT_ROOT := $(abspath $(MY_LOCAL_PATH)/../../../../..)
CARD_SDK_PATH := $(abspath $(PROJECT_ROOT)/vendor/cardboard/sdk)
CARD_PROTO_PATH := $(abspath $(PROJECT_ROOT)/vendor/cardboard/proto)


include $(call all-subdir-makefiles)

LOCAL_PATH := $(MY_LOCAL_PATH)

include $(CLEAR_VARS)
LOCAL_MODULE    := moonlight-core

LOCAL_SRC_FILES := moonlight-common-c/src/AudioStream.c \
                   moonlight-common-c/src/ByteBuffer.c \
                   moonlight-common-c/src/Connection.c \
                   moonlight-common-c/src/ConnectionTester.c \
                   moonlight-common-c/src/ControlStream.c \
                   moonlight-common-c/src/FakeCallbacks.c \
                   moonlight-common-c/src/InputStream.c \
                   moonlight-common-c/src/LinkedBlockingQueue.c \
                   moonlight-common-c/src/Misc.c \
                   moonlight-common-c/src/Platform.c \
                   moonlight-common-c/src/PlatformCrypto.c \
                   moonlight-common-c/src/PlatformSockets.c \
                   moonlight-common-c/src/RtpAudioQueue.c \
                   moonlight-common-c/src/RtpVideoQueue.c \
                   moonlight-common-c/src/RtspConnection.c \
                   moonlight-common-c/src/RtspParser.c \
                   moonlight-common-c/src/SdpGenerator.c \
                   moonlight-common-c/src/SimpleStun.c \
                   moonlight-common-c/src/VideoDepacketizer.c \
                   moonlight-common-c/src/VideoStream.c \
                   moonlight-common-c/reedsolomon/rs.c \
                   moonlight-common-c/enet/callbacks.c \
                   moonlight-common-c/enet/compress.c \
                   moonlight-common-c/enet/host.c \
                   moonlight-common-c/enet/list.c \
                   moonlight-common-c/enet/packet.c \
                   moonlight-common-c/enet/peer.c \
                   moonlight-common-c/enet/protocol.c \
                   moonlight-common-c/enet/unix.c \
                   moonlight-common-c/enet/win32.c \
                   simplejni.c \
                   callbacks.c \
                   minisdl.c \


LOCAL_C_INCLUDES := $(LOCAL_PATH)/moonlight-common-c/enet/include \
                    $(LOCAL_PATH)/moonlight-common-c/reedsolomon \
                    $(LOCAL_PATH)/moonlight-common-c/src \

LOCAL_CFLAGS := -DHAS_SOCKLEN_T=1 -DLC_ANDROID -DHAVE_CLOCK_GETTIME=1

ifeq ($(NDK_DEBUG),1)
LOCAL_CFLAGS += -DLC_DEBUG
endif

LOCAL_LDLIBS := -llog

LOCAL_STATIC_LIBRARIES := libopus libssl libcrypto cpufeatures
LOCAL_LDFLAGS += -Wl,--exclude-libs,ALL

LOCAL_BRANCH_PROTECTION := standard

include $(BUILD_SHARED_LIBRARY)

$(call import-module,android/cpufeatures)

VR_CARD_SOURCES := \
    $(CARD_SDK_PATH)/cardboard.cc \
    $(CARD_SDK_PATH)/distortion_mesh.cc \
    $(CARD_SDK_PATH)/lens_distortion.cc \
    $(CARD_SDK_PATH)/polynomial_radial_distortion.cc \
    $(CARD_SDK_PATH)/head_tracker.cc \
    $(CARD_SDK_PATH)/rendering/opengl_es2_distortion_renderer.cc \
    $(CARD_SDK_PATH)/screen_params/android/screen_params.cc \
    $(CARD_SDK_PATH)/device_params/android/device_params.cc \
    $(CARD_SDK_PATH)/qrcode/android/qr_code.cc \
    $(CARD_SDK_PATH)/qrcode/cardboard_v1/cardboard_v1.cc \
    $(CARD_SDK_PATH)/jni_utils/android/jni_utils.cc \
    $(CARD_SDK_PATH)/sensors/gyroscope_bias_estimator.cc \
    $(CARD_SDK_PATH)/sensors/sensor_fusion_ekf.cc \
    $(CARD_SDK_PATH)/sensors/median_filter.cc \
    $(CARD_SDK_PATH)/sensors/mean_filter.cc \
    $(CARD_SDK_PATH)/sensors/neck_model.cc \
    $(CARD_SDK_PATH)/sensors/lowpass_filter.cc \
    $(CARD_SDK_PATH)/sensors/android/device_accelerometer_sensor.cc \
    $(CARD_SDK_PATH)/sensors/android/device_gyroscope_sensor.cc \
    $(CARD_SDK_PATH)/sensors/android/sensor_event_producer.cc \
    $(CARD_SDK_PATH)/util/matrixutils.cc \
    $(CARD_SDK_PATH)/util/matrix_4x4.cc \
    $(CARD_SDK_PATH)/util/matrix_3x3.cc \
    $(CARD_SDK_PATH)/util/rotation.cc \
    $(CARD_SDK_PATH)/util/vectorutils.cc \
    $(CARD_SDK_PATH)/util/is_initialized.cc \

VR_MODULE_PATH := $(MY_LOCAL_PATH)/..

VR_MODULE_SOURCES := \
    vr/vr_renderer.cpp \
    vr/vr_renderer_jni.cc \
    vr/util.cc \
    $(VR_CARD_SOURCES)

LOCAL_PATH := $(VR_MODULE_PATH)
include $(CLEAR_VARS)
LOCAL_MODULE := vr_renderer
LOCAL_SRC_FILES := $(VR_MODULE_SOURCES)
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/vr \
    $(CARD_SDK_PATH) \
    $(CARD_SDK_PATH)/include \
    $(CARD_PROTO_PATH)
LOCAL_CPPFLAGS := -std=c++17 -fexceptions -frtti
LOCAL_LDLIBS := -llog -landroid -lGLESv2 -lEGL -ldl -lc++_shared -latomic
include $(BUILD_SHARED_LIBRARY)
