# 建议：变量名也同步改为FNN_BAGGING开头（可选，不改也能运行）
FNN_BAGGING_TEST_SRCS := \
$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_bagging/Fnn_bagging_test.cc

FNN_BAGGING_SRCS := \
$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_bagging/Fnn_bagging_test.cc

FNN_BAGGING_HDRS :=

# 模型文件路径必须改（指向Fnn_bagging目录）
FNN_BAGGING_GENERATOR_INPUTS := \
$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_bagging/models/fnn_ensemble_model_1.tflite \
$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_bagging/models/fnn_ensemble_model_1_int8.tflite \
$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_bagging/models/fnn_ensemble_model_2.tflite \
$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_bagging/models/fnn_ensemble_model_2_int8.tflite \
$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_bagging/models/fnn_ensemble_model_3.tflite \
$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_bagging/models/fnn_ensemble_model_3_int8.tflite

# 生成文件路径必须改（指向Fnn_bagging目录）
FNN_BAGGING_GENERATED_SRCS := \
$(GENERATED_SRCS_DIR)$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_bagging/models/fnn_ensemble_model_1_data.cc \
$(GENERATED_SRCS_DIR)$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_bagging/models/fnn_ensemble_model_1_int8_data.cc \
$(GENERATED_SRCS_DIR)$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_bagging/models/fnn_ensemble_model_2_data.cc \
$(GENERATED_SRCS_DIR)$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_bagging/models/fnn_ensemble_model_2_int8_data.cc \
$(GENERATED_SRCS_DIR)$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_bagging/models/fnn_ensemble_model_3_data.cc \
$(GENERATED_SRCS_DIR)$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_bagging/models/fnn_ensemble_model_3_int8_data.cc

# 生成头文件路径必须改（指向Fnn_bagging目录）
FNN_BAGGING_GENERATED_HDRS := \
$(GENERATED_SRCS_DIR)$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_bagging/models/fnn_ensemble_model_1_data.h \
$(GENERATED_SRCS_DIR)$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_bagging/models/fnn_ensemble_model_1_int8_data.h \
$(GENERATED_SRCS_DIR)$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_bagging/models/fnn_ensemble_model_2_data.h \
$(GENERATED_SRCS_DIR)$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_bagging/models/fnn_ensemble_model_2_int8_data.h \
$(GENERATED_SRCS_DIR)$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_bagging/models/fnn_ensemble_model_3_data.h \
$(GENERATED_SRCS_DIR)$(TENSORFLOW_ROOT)tensorflow/lite/micro/examples/Fnn_bagging/models/fnn_ensemble_model_3_int8_data.h

# 测试目标名建议改（避免冲突）
$(eval $(call microlite_test,fnn_bagging_test,\
$(FNN_BAGGING_TEST_SRCS),,$(FNN_BAGGING_GENERATOR_INPUTS)))

$(eval $(call microlite_test,fnn_bagging,\
$(FNN_BAGGING_SRCS),,$(FNN_BAGGING_GENERATOR_INPUTS)))

# 变量赋值也要同步（如果改了变量名）
FNN_BAGGING_SRCS += $(FNN_BAGGING_GENERATED_SRCS)
FNN_BAGGING_HDRS += $(FNN_BAGGING_GENERATED_HDRS)

# 可选：保留旧的list目标，或新增新的list目标
list_fnn_bagging_example_sources:
	@echo $(FNN_BAGGING_SRCS)

list_fnn_bagging_example_headers:
	@echo $(FNN_BAGGING_HDRS)

# 兼容旧的list目标（可选）
list_fnn_example_sources:
	@echo $(FNN_BAGGING_SRCS)

list_fnn_example_headers:
	@echo $(FNN_BAGGING_HDRS)