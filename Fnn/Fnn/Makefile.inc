FNN_TEST_SRCS := \
$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn/Fnn_test.cc

FNN_SRCS := \
$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn/Fnn_test.cc

FNN_HDRS :=

FNN_GENERATOR_INPUTS := \
$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn/models/fnn_binary_classification.tflite \
$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn/models/fnn_binary_classification_int8.tflite

FNN_GENERATED_SRCS := \
$(GENERATED_SRCS_DIR)$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn/models/fnn_binary_classification_model_data.cc \
$(GENERATED_SRCS_DIR)$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn/models/fnn_binary_classification_int8_model_data.cc

FNN_GENERATED_HDRS := \
$(GENERATED_SRCS_DIR)$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn/models/fnn_binary_classification_model_data.h \
$(GENERATED_SRCS_DIR)$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn/models/fnn_binary_classification_int8_model_data.h

$(eval $(call microlite_test,fnn_test,\
$(FNN_TEST_SRCS),,$(FNN_GENERATOR_INPUTS)))

$(eval $(call microlite_test,fnn,\
$(FNN_SRCS),,$(FNN_GENERATOR_INPUTS)))

FNN_SRCS += $(FNN_GENERATED_SRCS)
FNN_HDRS += $(FNN_GENERATED_HDRS)

list_fnn_example_sources:
	@echo $(FNN_SRCS)

list_fnn_example_headers:
	@echo $(FNN_HDRS)