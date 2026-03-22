# 建议：变量名同步改为FNN_GBOOSTING开头（保持命名一致性）
FNN_GBOOSTING_TEST_SRCS := \
$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_gboosting/Fnn_gboosting_test.cc

FNN_GBOOSTING_SRCS := \
$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_gboosting/Fnn_gboosting_test.cc

FNN_GBOOSTING_HDRS :=

# 模型文件路径改为Fnn_gboosting目录，模型名改为梯度提升命名
FNN_GBOOSTING_GENERATOR_INPUTS := \
$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_gboosting/models/gradient_boost_model_1.tflite \
$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_gboosting/models/gradient_boost_model_1_int8.tflite \
$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_gboosting/models/gradient_boost_model_2.tflite \
$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_gboosting/models/gradient_boost_model_2_int8.tflite \
$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_gboosting/models/gradient_boost_model_3.tflite \
$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_gboosting/models/gradient_boost_model_3_int8.tflite

# 生成文件路径改为Fnn_gboosting目录，匹配梯度提升模型命名
FNN_GBOOSTING_GENERATED_SRCS := \
$(GENERATED_SRCS_DIR)$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_gboosting/models/gradient_boost_model_1_data.cc \
$(GENERATED_SRCS_DIR)$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_gboosting/models/gradient_boost_model_1_int8_data.cc \
$(GENERATED_SRCS_DIR)$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_gboosting/models/gradient_boost_model_2_data.cc \
$(GENERATED_SRCS_DIR)$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_gboosting/models/gradient_boost_model_2_int8_data.cc \
$(GENERATED_SRCS_DIR)$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_gboosting/models/gradient_boost_model_3_data.cc \
$(GENERATED_SRCS_DIR)$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_gboosting/models/gradient_boost_model_3_int8_data.cc

# 生成头文件路径改为Fnn_gboosting目录，匹配梯度提升模型命名
FNN_GBOOSTING_GENERATED_HDRS := \
$(GENERATED_SRCS_DIR)$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_gboosting/models/gradient_boost_model_1_data.h \
$(GENERATED_SRCS_DIR)$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_gboosting/models/gradient_boost_model_1_int8_data.h \
$(GENERATED_SRCS_DIR)$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_gboosting/models/gradient_boost_model_2_data.h \
$(GENERATED_SRCS_DIR)$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_gboosting/models/gradient_boost_model_2_int8_data.h \
$(GENERATED_SRCS_DIR)$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_gboosting/models/gradient_boost_model_3_data.h \
$(GENERATED_SRCS_DIR)$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_gboosting/models/gradient_boost_model_3_int8_data.h

# 测试目标名改为fnn_gboosting，匹配新的工程命名
$(eval $(call microlite_test,fnn_gboosting_test,\
$(FNN_GBOOSTING_TEST_SRCS),,$(FNN_GBOOSTING_GENERATOR_INPUTS)))

$(eval $(call microlite_test,fnn_gboosting,\
$(FNN_GBOOSTING_SRCS),,$(FNN_GBOOSTING_GENERATOR_INPUTS)))

# 变量赋值同步改为FNN_GBOOSTING前缀
FNN_GBOOSTING_SRCS += $(FNN_GBOOSTING_GENERATED_SRCS)
FNN_GBOOSTING_HDRS += $(FNN_GBOOSTING_GENERATED_HDRS)

# 新增梯度提升专属的list目标
list_fnn_gboosting_example_sources:
	@echo $(FNN_GBOOSTING_SRCS)

list_fnn_gboosting_example_headers:
	@echo $(FNN_GBOOSTING_HDRS)

list_fnn_example_sources:
	@echo $(FNN_GBOOSTING_SRCS)

list_fnn_example_headers:
	@echo $(FNN_GBOOSTING_HDRS)