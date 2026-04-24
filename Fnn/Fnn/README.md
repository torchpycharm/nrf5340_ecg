关闭wsl
wsl --shutdown
打开wsl
wsl

cd ~

cd tflite-micro

train
编译
bazel build tensorflow/lite/micro/examples/Fnn:train
运行
bazel-bin/tensorflow/lite/micro/examples/Fnn/train --save_tf_model --save_dir=/tmp/model_created/ --epochs=100 --train_data_path=/home/lixinying/tflite-micro/tensorflow/lite/micro/examples/Fnn/training_set.csv --test_data_path=/home/lixinying/tflite-micro/tensorflow/lite/micro/examples/Fnn/test_set.csv

ptq
编译
bazel build tensorflow/lite/micro/examples/Fnn/quantization:ptq
运行
bazel-bin/tensorflow/lite/micro/examples/Fnn/quantization/ptq --source_model_dir=/tmp/model_created/saved_model --target_dir=/tmp/quant_model/

Fnn_test
bazel run tensorflow/lite/micro/examples/Fnn:Fnn_test