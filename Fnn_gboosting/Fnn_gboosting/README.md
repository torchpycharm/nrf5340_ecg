关闭wsl
wsl --shutdown
打开wsl
wsl

cd ~

cd tflite-micro

train
编译
bazel build tensorflow/lite/micro/examples/Fnn_gboosting:train
运行
bazel-bin/tensorflow/lite/micro/examples/Fnn_gboosting/train --save_tf_model --save_dir=/tmp/model_created/ --epochs=100 --train_data_path=/home/lixinying/tflite-micro/tensorflow/lite/micro/examples/Fnn_gboosting/training_set.csv --test_data_path=/home/lixinying/tflite-micro/tensorflow/lite/micro/examples/Fnn_gboosting/test_set.csv

ptq
编译
bazel build tensorflow/lite/micro/examples/Fnn_gboosting/quantization:ptq
运行
bazel-bin/tensorflow/lite/micro/examples/Fnn_gboosting/quantization/ptq --source_model_dir=/tmp/model_created/ --target_dir=/tmp/quant_model/

Fnn_gboosting_test
bazel run tensorflow/lite/micro/examples/Fnn_gboosting:Fnn_gboosting_test