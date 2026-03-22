"""
该脚本将训练好的FNN浮点模型批量转换为INT8量化模型（支持多模型批量处理）。
"""
import math
import os
import glob

from absl import app
from absl import flags
from absl import logging
import numpy as np
import tensorflow as tf

FLAGS = flags.FLAGS

flags.DEFINE_string("source_model_dir", "/tmp/model_created",
                    "训练好的FNN集成模型根目录（包含多个saved_model_x子目录）")
flags.DEFINE_string("target_dir", "/tmp/quant_models",
                    "量化后INT8模型的保存目录")
flags.DEFINE_integer("num_models", 3, "需要量化的模型数量（默认3个）")


def get_representative_data():
    """读取训练集数据作为量化校准的代表性数据"""
    # 数据读取路径
    data_path = "/home/lixinying/tflite-micro/tensorflow/lite/micro/examples/Fnn_bagging/training_set.csv"
    
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
        logging.info(f"【成功】量化模型已保存至：{save_path}")

    except Exception as e:
        logging.error(f"【错误】保存量化模型失败：{str(e)}")
        raise


def find_model_dirs(root_dir, num_models):
    """
    查找所有需要量化的模型目录
    :param root_dir: 模型根目录
    :param num_models: 模型数量
    :return: 模型目录列表
    """
    model_dirs = []
    
    # 方式1：按命名规则查找（saved_model_1, saved_model_2...）
    for i in range(1, num_models + 1):
        model_path = os.path.join(root_dir, f"saved_model_{i}")
        if os.path.exists(model_path):
            model_dirs.append((i, model_path))
        else:
            logging.warning(f"【警告】模型目录不存在：{model_path}")
    
    # 如果没找到，尝试自动查找所有saved_model开头的目录
    if not model_dirs:
        logging.info("【提示】未按命名规则找到模型，尝试自动查找所有saved_model目录")
        all_dirs = glob.glob(os.path.join(root_dir, "saved_model_*"))
        for idx, dir_path in enumerate(all_dirs, 1):
            model_dirs.append((idx, dir_path))
    
    if not model_dirs:
        raise FileNotFoundError(f"【错误】在{root_dir}下未找到任何模型目录")
    
    logging.info(f"【成功】找到{len(model_dirs)}个模型目录：{[dir_path for _, dir_path in model_dirs]}")
    return model_dirs


def convert_quantized_tflite_model(source_model_dir, x_values, model_idx):
    """
    将保存的TF浮点模型转换为INT8量化TFLite模型
    :param source_model_dir: 单个模型目录
    :param x_values: 校准数据
    :param model_idx: 模型序号
    :return: 量化后的tflite模型
    """
    
    # 定义量化校准用的代表性数据集生成器
    def representative_dataset(num_samples=512):
        calib_num = min(num_samples, len(x_values))
        for i in range(calib_num):
            # 模型输入形状：[1,6]
            input_data = x_values[i].reshape(1, 6)
            yield [input_data]

    try:
        logging.info(f"\n【开始】量化第{model_idx}个模型：{source_model_dir}")
        
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
        
        logging.info(f"【成功】第{model_idx}个模型量化完成")
        return tflite_model
        
    except Exception as e:
        logging.error(f"【错误】第{model_idx}个模型量化转换失败：{str(e)}")
        raise


def main(_):
    """主函数：执行完整的批量量化流程"""
    # 设置日志级别为INFO，确保所有调试信息输出
    logging.set_verbosity(logging.INFO)

    # 步骤1：加载量化校准数据
    logging.info("\n===== 步骤1：加载量化校准数据 =====")
    x_values = get_representative_data()

    # 步骤2：查找所有需要量化的模型目录
    logging.info("\n===== 步骤2：查找模型目录 =====")
    model_dirs = find_model_dirs(FLAGS.source_model_dir, FLAGS.num_models)

    # 步骤3：批量执行模型量化转换
    logging.info("\n===== 步骤3：批量执行INT8量化 =====")
    for model_idx, model_path in model_dirs:
        # 量化单个模型
        quantized_tflite_model = convert_quantized_tflite_model(
            model_path, x_values, model_idx)
        
        # 保存量化后的模型
        save_tflite_model(
            quantized_tflite_model,
            FLAGS.target_dir,
            model_name=f"fnn_ensemble_model_{model_idx}_int8.tflite"
        )

    logging.info("\n===== FNN集成模型INT8量化流程全部完成！=====")


if __name__ == "__main__":
    app.run(main)
