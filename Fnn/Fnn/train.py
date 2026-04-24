import os
import numpy as np
from absl import app
from absl import flags
from absl import logging
import tensorflow as tf

# 适配 Keras 3.x 导入方式
from keras import Sequential
from keras.layers import Dense
from keras.optimizers import Adam

FLAGS = flags.FLAGS

flags.DEFINE_integer("epochs", 100, "训练轮数")
flags.DEFINE_string("save_dir", "/tmp/hello_world_models",
                    "模型保存目录")
flags.DEFINE_boolean("save_tf_model", False,
                     "是否保存原始TF模型")
flags.DEFINE_string("train_data_path", "training_set.csv",
                    "训练集Excel文件路径")
flags.DEFINE_string("test_data_path", "test_set.csv",
                    "测试集Excel文件路径")


class StandardScaler:
    """纯numpy实现的标准化器"""
    def __init__(self):
        self.mean_ = None
        self.scale_ = None
    
    def fit(self, X):
        """基于训练数据拟合均值和标准差"""
        self.mean_ = np.mean(X, axis=0)
        self.scale_ = np.std(X, axis=0)
        # 防止标准差为0导致除零错误
        self.scale_[self.scale_ == 0] = 1.0
        return self
    
    def transform(self, X):
        """使用拟合的均值和标准差标准化数据"""
        if self.mean_ is None or self.scale_ is None:
            raise ValueError("Scaler尚未拟合，请先调用fit方法")
        return (X - self.mean_) / self.scale_
    
    def fit_transform(self, X):
        """拟合并转换数据"""
        return self.fit(X).transform(X)


def load_excel_data(file_path):
    """加载csv数据"""
    try:
        # 读取CSV文件
        data = np.loadtxt(file_path, delimiter=',', skiprows=1)  # 跳过表头
        return data
    except Exception as e:
        logging.error(f"读取数据文件失败：{e}")
        # 提供测试数据生成（用于调试）
        logging.warning("使用测试数据替代，请确保实际运行时提供正确的CSV文件")
        # 生成模拟数据：6个特征 + 1个标签
        n_samples = 100
        features = np.random.randn(n_samples, 6)
        labels = np.random.randint(0, 2, (n_samples, 1))
        return np.hstack([features, labels])


def get_data(train_path, test_path):
    """加载并预处理训练集和测试集数据"""
    # 清除TF会话
    tf.keras.backend.clear_session()
    
    # 检查文件是否存在
    if not tf.io.gfile.exists(train_path):
        logging.error(f"训练数据文件不存在：{train_path}")
        raise FileNotFoundError(f"训练数据文件 {train_path} 不存在，请检查路径")
    if not tf.io.gfile.exists(test_path):
        logging.error(f"测试数据文件不存在：{test_path}")
        raise FileNotFoundError(f"测试数据文件 {test_path} 不存在，请检查路径")
    
    # 读取数据
    train_data = load_excel_data(train_path)
    test_data = load_excel_data(test_path)
    
    # 检查数据维度（6个特征 + 1个标签）
    if train_data.shape[1] != 7:
        raise ValueError(f"训练集数据维度错误，期望7列（6特征+1标签），实际{train_data.shape[1]}列")
    if test_data.shape[1] != 7:
        raise ValueError(f"测试集数据维度错误，期望7列（6特征+1标签），实际{test_data.shape[1]}列")
    
    # 提取特征列和标签列
    features_train = train_data[:, :6]  
    train_labels = train_data[:, 6].astype(int)  
    features_test = test_data[:, :6]
    test_labels = test_data[:, 6].astype(int)
    
    # 检查缺失值
    if np.isnan(features_train).any():
        logging.warning(f"训练特征包含 {np.sum(np.isnan(features_train))} 个缺失值(NaN)")
        features_train = np.nan_to_num(features_train)  # 替换NaN为0
    if np.isnan(features_test).any():
        logging.warning(f"测试特征包含 {np.sum(np.isnan(features_test))} 个缺失值(NaN)")
        features_test = np.nan_to_num(features_test)
    
    # 特征标准化
    scaler = StandardScaler()
    features_train = scaler.fit_transform(features_train)
    features_test = scaler.transform(features_test)
        
    logging.info(f"加载数据成功，训练集共{len(train_labels)}个样本，测试集共{len(test_labels)}个样本")
    
    return features_train, train_labels, features_test, test_labels, scaler


def create_model() -> tf.keras.Model:
    """创建二分类FNN模型"""
    
    # 构建模型：输入层(6) -> 隐藏层(4, ReLU) -> 输出层(1, Sigmoid)
    model = Sequential()
    model.add(Dense(4, activation='relu', input_shape=(6,), name="hidden_layer_4_neurons")) 
    model.add(Dense(1, activation='sigmoid', name="output_layer_binary_class"))              

    # 编译模型
    optimizer = Adam(learning_rate=0.001)
    model.compile(
        optimizer=optimizer,                # 优化器：Adam，学习率0.001
        loss='binary_crossentropy',         # 损失函数：二分类交叉熵
        metrics=['accuracy']                # 监控指标：准确率
    )
    
    return model


def convert_tflite_model(model):
    """将训练好的TF模型转换为TFLite格式"""
    
    try:
        converter = tf.lite.TFLiteConverter.from_keras_model(model)
        tflite_model = converter.convert()

        return tflite_model
    except Exception as e:
        logging.error(f"TFLite模型转换失败：{str(e)}")
        raise


def save_tflite_model(tflite_model, save_dir, model_name):
    """保存转换后的TFLite模型"""
    
    try:
        # 创建保存目录，如果不存在
        if not tf.io.gfile.exists(save_dir):
            tf.io.gfile.makedirs(save_dir)
            logging.info(f"创建保存目录：{save_dir}")
        
        # 保存模型文件
        save_path = os.path.join(save_dir, model_name)
        with tf.io.gfile.GFile(save_path, "wb") as f:
            f.write(tflite_model)
        
        # 验证保存结果
        if tf.io.gfile.exists(save_path):
            file_size_kb = tf.io.gfile.stat(save_path).length / 1024
            logging.info(f"TFLite模型保存成功 - 路径：{save_path}，文件大小：{file_size_kb:.2f} KB")
        else:
            logging.error(f"TFLite模型保存失败 - 文件未找到：{save_path}")

    except Exception as e:
        logging.error(f"保存TFLite模型失败：{str(e)}")
        raise


def confusion_matrix(y_true, y_pred):
    """纯numpy实现混淆矩阵计算"""
    # 获取唯一标签
    labels = np.unique(np.concatenate([y_true, y_pred]))
    if len(labels) > 2:
        raise ValueError("仅支持二分类任务")
    # 确保标签是0和1
    y_true = np.array(y_true)
    y_pred = np.array(y_pred)
    
    # 计算TP, TN, FP, FN
    TP = np.sum((y_true == 1) & (y_pred == 1))
    TN = np.sum((y_true == 0) & (y_pred == 0))
    FP = np.sum((y_true == 0) & (y_pred == 1))
    FN = np.sum((y_true == 1) & (y_pred == 0))
    
    return np.array([[TN, FP], [FN, TP]])


def evaluate_model(model, features_test, test_labels):
    """在测试集上评估模型性能并打印详细指标"""

    try:
        # 测试集预测
        test_pred_probs = model.predict(features_test, verbose=0)
        test_pred = (test_pred_probs > 0.5).astype(int).flatten()
        
        # 计算混淆矩阵和性能指标
        C_fnn_test = confusion_matrix(test_labels, test_pred)
        
        # 提取TP, TN, FP, FN
        TP = C_fnn_test[1, 1]
        TN = C_fnn_test[0, 0]
        FP = C_fnn_test[0, 1]
        FN = C_fnn_test[1, 0]

        # 计算性能指标（避免除以0）
        total = TP + TN + FP + FN
        accuracy_fnn = (TP + TN) / total if total > 0 else 0
        precision_fnn = TP / (TP + FP) if (TP + FP) > 0 else 0
        sensitivity_fnn = TP / (TP + FN) if (TP + FN) > 0 else 0  
        specificity_fnn = TN / (TN + FP) if (TN + FP) > 0 else 0 
        f1_fnn = 2 * TP / (2 * TP + FP + FN) if (2 * TP + FP + FN) > 0 else 0

        # 打印评估结果
        logging.info("\n==================== FNN模型评估结果 ====================")
        logging.info(f"测试集混淆矩阵：\n{C_fnn_test}")
        logging.info("----------------------------------------------------")
        logging.info(f"准确率(Accuracy)：         {accuracy_fnn:.4f}")
        logging.info(f"精确率(Precision)：        {precision_fnn:.4f}")
        logging.info(f"召回率(Sensitivity/Recall)：      {sensitivity_fnn:.4f}")
        logging.info(f"特异度(Specificity)：      {specificity_fnn:.4f}")
        logging.info(f"F1分数(F1-score)：         {f1_fnn:.4f}")
        
    except Exception as e:
        logging.error(f"模型评估失败：{str(e)}")
        raise


def train_model(epochs, features_train, train_labels, features_test, test_labels):
    """训练FNN二分类模型"""
    
    model = create_model()
    
    # 训练模型
    try:
        history = model.fit(
            features_train, train_labels,
            epochs=epochs,
            batch_size=8,
            verbose=1
        )
        
        # 保存原始TF模型
        if FLAGS.save_tf_model:
            logging.info(f"保存原始TF模型到：{FLAGS.save_dir}/saved_model")
            if not tf.io.gfile.exists(FLAGS.save_dir):
                tf.io.gfile.makedirs(FLAGS.save_dir)
            tf_model_path = os.path.join(FLAGS.save_dir, "saved_model")
            model.export(tf_model_path)
            
            # 验证保存结果
            if tf.io.gfile.exists(tf_model_path):
                logging.info(f"TF模型保存成功：{tf_model_path}")
            else:
                logging.error(f"TF模型保存失败：{tf_model_path}")

    except Exception as e:
        logging.error(f"模型训练失败：{str(e)}")
        raise
    
    # 评估模型性能
    evaluate_model(model, features_test, test_labels)
    
    return model


def main(_):
    """主函数：执行完整的模型训练流程"""
    # 设置日志级别，确保调试信息输出
    logging.set_verbosity(logging.INFO)
    
    try:
        # 1. 加载和预处理数据
        features_train, train_labels, features_test, test_labels, scaler = get_data(
            FLAGS.train_data_path, FLAGS.test_data_path
        )
        
        # 2. 训练模型
        trained_model = train_model(
            FLAGS.epochs, features_train, train_labels, features_test, test_labels
        )

        # 3. 转换并保存TFLite模型
        tflite_model = convert_tflite_model(trained_model)
        save_tflite_model(tflite_model,
                          FLAGS.save_dir,
                          model_name="fnn_binary_classification.tflite")
        
        # 4. 保存标准化器
        scaler_path = os.path.join(FLAGS.save_dir, "scaler.npy")
        with tf.io.gfile.GFile(scaler_path, "wb") as f:
            np.save(f, [scaler.mean_, scaler.scale_])
        
        # 验证保存结果
        if tf.io.gfile.exists(scaler_path):
            logging.info(f"标准化器保存成功 - 均值：{scaler.mean_.round(4)}，标准差：{scaler.scale_.round(4)}")
        else:
            logging.error(f"标准化器保存失败 - 文件未找到：{scaler_path}")
        
    except Exception as e:
        logging.error(f"训练流程执行失败：{str(e)}")
        raise


if __name__ == "__main__":
    app.run(main)