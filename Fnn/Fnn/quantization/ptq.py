"""
该脚本将训练好的FNN浮点模型转换为INT8量化模型。
"""
import math
import os

from absl import app
from absl import flags
from absl import logging
import numpy as np
import tensorflow as tf

FLAGS = flags.FLAGS

flags.DEFINE_string("source_model_dir", "/tmp/model_created/saved_model",
                    "训练好的FNN模型所在目录")
flags.DEFINE_string("target_dir", "/tmp/quant_model",
                    "量化后INT8模型的保存目录")


def get_representative_data():
    """读取训练集数据作为量化校准的代表性数据"""
    # 数据读取路径
    data_path = "/home/lixinying/tflite-micro/tensorflow/lite/micro/examples/Fnn/training_set.csv"
    
    try:
        # 读取CSV数据（跳过表头）
        data = np.loadtxt(data_path, delimiter=',', skiprows=1)
        # 提取前6列特征（输入维度）
        x_values = data[:, :6].astype(np.float32)
        # 打乱数据（避免顺序影响）
        np.random.shuffle(x_values)
        mean = np.array([15.0445, 16.3455, 16.6089, 1.4722, 1.5089, 1.6104], dtype=np.float32)
        scale = np.array([12.3295, 11.9176, 15.3273, 2.5767, 2.7871, 2.7538], dtype=np.float32)
        x_values = (x_values - mean) / scale

        calib_num = min(512, len(x_values))
        x_values = x_values[:calib_num]
        logging.info(f"最终用于校准的样本数：{calib_num}")

        return x_values
    except Exception as e:
        logging.error(f"【错误】读取校准数据失败：{str(e)}")
        raise


def save_tflite_model(tflite_model, target_dir, model_name):
    """保存转换后的TFLite模型到指定目录"""
    # 如果不存在，创建保存目录
    if not os.path.exists(target_dir):
        os.makedirs(target_dir)
    # 拼接保存路径
    save_path = os.path.join(target_dir, model_name)
    # 写入模型文件
    try:
        with open(save_path, "wb") as f:
            f.write(tflite_model)

    except Exception as e:
        logging.error(f"【错误】保存量化模型失败：{str(e)}")
        raise


def convert_quantized_tflite_model(source_model_dir, x_values):
    """将保存的TF浮点模型转换为INT8量化TFLite模型"""
    
    # 定义量化校准用的代表性数据集生成器
    def representative_dataset(num_samples=512):
        calib_num = min(num_samples, len(x_values))
        for i in range(calib_num):
            # 模型输入形状：[1,6]
            input_data = x_values[i].reshape(1, 6)
            yield [input_data]

    try:
        # 初始化TF模型转换器
        converter = tf.lite.TFLiteConverter.from_saved_model(source_model_dir)
        
        # 配置量化参数
        converter.optimizations = [tf.lite.Optimize.DEFAULT]
        converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
        converter.inference_input_type = tf.int8
        converter.inference_output_type = tf.int8

        converter.target_spec.supported_types = [tf.int8]  # 显式指定量化类型
        converter.experimental_new_quantizer = True        # 启用新版量化器

        converter.representative_dataset = representative_dataset
        
        # 执行模型转换
        tflite_model = converter.convert()
        
        return tflite_model
    except Exception as e:
        logging.error(f"【错误】模型量化转换失败：{str(e)}")
        raise


def main(_):
    """主函数：执行完整的量化流程"""
    # 设置日志级别为INFO，确保所有调试信息输出
    logging.set_verbosity(logging.INFO)

    # 步骤1：加载量化校准数据
    logging.info("\n1.加载量化校准数据")
    x_values = get_representative_data()

    # 步骤2：执行模型量化转换
    logging.info("\n2.执行模型INT8量化转换")
    quantized_tflite_model = convert_quantized_tflite_model(
        FLAGS.source_model_dir, x_values)

    # 步骤3：保存量化后的模型
    logging.info("\n3.保存量化后的INT8模型")
    save_tflite_model(
        quantized_tflite_model,
        FLAGS.target_dir,
        model_name="fnn_binary_classification_int8.tflite"
    )

    logging.info("FNN模型INT8量化流程全部成功！")


if __name__ == "__main__":
    app.run(main)
