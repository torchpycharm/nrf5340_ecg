import os
import numpy as np
import six
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
flags.DEFINE_string("save_dir", "/tmp/model_created",
                    "模型保存目录")
flags.DEFINE_boolean("save_tf_model", False,
                     "是否保存原始TF模型")
flags.DEFINE_string("train_data_path", "training_set.csv",
                    "训练集CSV文件路径")
flags.DEFINE_string("test_data_path", "test_set.csv",
                    "测试集CSV文件路径")
flags.DEFINE_integer("sample_size", 512, "单次抽样样本数量")
flags.DEFINE_integer("n_samples", 3, "抽样次数（模型数量）")


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
    """加载csv数据（兼容原有逻辑，同时适配新的数据读取方式）"""
    try:
        # 使用新的纯numpy读取方式，更健壮
        with open(file_path, 'r', encoding='utf-8') as f:
            lines = f.readlines()
        
        data_lines = [line.strip() for line in lines[1:] if line.strip()]
        data = []
        for line in data_lines:
            row_tests = line.split(',')[:7]
            try:
                row = [float(test) for test in row_tests]
                data.append(row)
            except ValueError as e:
                logging.warning(f"警告：跳过无效数据行 {line}，错误：{e}")
                continue
        
        data_np = np.array(data, dtype=np.float32)
        return data_np
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


def build_fnn_model(input_dim=6, hidden_units=4):
    """构建标准化FNN模型（复用模型结构）"""
    model = Sequential()
    model.add(Dense(hidden_units, activation='relu', input_shape=(input_dim,), name="hidden_layer_4_neurons"))
    model.add(Dense(1, activation='sigmoid', name="output_layer_binary_class"))
    model.compile(
        optimizer=Adam(learning_rate=0.001),
        loss='binary_crossentropy',
        metrics=['accuracy']
    )
    return model


def random_sample_with_replacement(features, labels, sample_size):
    """
    有放回随机抽样（替代sklearn的resample）
    :param features: 特征数组 (n_samples, n_features)
    :param labels: 标签数组 (n_samples,)
    :param sample_size: 抽样样本数
    :return: 抽样后的特征、标签
    """
    n_samples = len(features)
    # 生成有放回的随机索引
    sample_indices = np.random.choice(n_samples, size=sample_size, replace=True)
    sampled_features = features[sample_indices]
    sampled_labels = labels[sample_indices]
    return sampled_features, sampled_labels


def voting_inference(models, X_test_scaled):
    """
    多模型投票推理
    :param models: 训练好的模型列表
    :param X_test_scaled: 标准化后的测试集特征
    :return: 每个样本的投票结果、各模型的预测结果
    """
    # 存储每个模型的预测结果（0/1）
    all_preds = []
    for idx, model in enumerate(models):
        pred_probs = model.predict(X_test_scaled, verbose=0)
        pred = (pred_probs > 0.5).astype(int).flatten()
        all_preds.append(pred)
        logging.info(f"模型{idx+1}测试集预测完成")
    
    # 转换为numpy数组 (n_models, n_samples)
    all_preds_np = np.array(all_preds)
    # 投票：统计每个样本的1的数量，超过半数则为1，否则为0
    vote_counts = np.sum(all_preds_np, axis=0)
    final_pred = (vote_counts > len(models)/2).astype(int)
    
    return final_pred, all_preds_np


def compute_confusion_matrix(y_true, y_pred):
    """替代sklearn的confusion_matrix，二分类专用"""
    y_true = y_true.flatten()
    y_pred = y_pred.flatten()
    
    TP = np.sum((y_true == 1) & (y_pred == 1))
    TN = np.sum((y_true == 0) & (y_pred == 0))
    FP = np.sum((y_true == 0) & (y_pred == 1))
    FN = np.sum((y_true == 1) & (y_pred == 0))
    
    cm = np.array([[TN, FP], [FN, TP]])
    return cm


def evaluate_ensemble_model(models, features_test, test_labels):
    """评估集成模型性能并打印详细指标"""
    try:
        # 多模型投票推理
        final_pred, all_preds_np = voting_inference(models, features_test)
        
        # 计算单个模型性能指标
        single_model_metrics = []
        for idx, model in enumerate(models):
            pred_probs = model.predict(features_test, verbose=0)
            pred = (pred_probs > 0.5).astype(int).flatten()
            
            cm = compute_confusion_matrix(test_labels, pred)
            TP = cm[1, 1]
            TN = cm[0, 0]
            FP = cm[0, 1]
            FN = cm[1, 0]
            
            accuracy = (TP + TN) / (TP + TN + FP + FN) if (TP + TN + FP + FN) > 0 else 0
            precision = TP / (TP + FP) if (TP + FP) > 0 else 0
            sensitivity = TP / (TP + FN) if (TP + FN) > 0 else 0
            specificity = TN / (TN + FP) if (TN + FP) > 0 else 0
            f1 = 2 * TP / (2 * TP + FP + FN) if (2 * TP + FP + FN) > 0 else 0
            
            single_model_metrics.append({
                "model_idx": idx+1,
                "confusion_matrix": cm,
                "accuracy": accuracy,
                "precision": precision,
                "sensitivity": sensitivity,
                "specificity": specificity,
                "f1": f1
            })
        
        # 计算投票后的性能指标
        cm_voting = compute_confusion_matrix(test_labels, final_pred)
        TP_v = cm_voting[1, 1]
        TN_v = cm_voting[0, 0]
        FP_v = cm_voting[0, 1]
        FN_v = cm_voting[1, 0]
        
        accuracy_voting = (TP_v + TN_v) / (TP_v + TN_v + FP_v + FN_v) if (TP_v + TN_v + FP_v + FN_v) > 0 else 0
        precision_voting = TP_v / (TP_v + FP_v) if (TP_v + FP_v) > 0 else 0
        sensitivity_voting = TP_v / (TP_v + FN_v) if (TP_v + FN_v) > 0 else 0
        specificity_voting = TN_v / (TN_v + FP_v) if (TN_v + FP_v) > 0 else 0
        f1_voting = 2 * TP_v / (2 * TP_v + FP_v + FN_v) if (2 * TP_v + FP_v + FN_v) > 0 else 0
        
        # 打印评估结果
        logging.info("\n==================== 单个模型评估结果 ====================")
        for metrics in single_model_metrics:
            logging.info(f"\n模型{metrics['model_idx']}：")
            logging.info(f"混淆矩阵：\n{metrics['confusion_matrix']}")
            logging.info(f"准确率: {metrics['accuracy']:.4f}")
            logging.info(f"精确率: {metrics['precision']:.4f}")
            logging.info(f"召回率: {metrics['sensitivity']:.4f}")
            logging.info(f"特异度: {metrics['specificity']:.4f}")
            logging.info(f"F1分数: {metrics['f1']:.4f}")
        
        logging.info("\n==================== 集成投票评估结果 ====================")
        logging.info(f"投票后混淆矩阵：\n{cm_voting}")
        logging.info(f"准确率(Accuracy)：         {accuracy_voting:.4f}")
        logging.info(f"精确率(Precision)：        {precision_voting:.4f}")
        logging.info(f"召回率(Sensitivity/Recall)：      {sensitivity_voting:.4f}")
        logging.info(f"特异度(Specificity)：      {specificity_voting:.4f}")
        logging.info(f"F1分数(F1-score)：         {f1_voting:.4f}")
        
        return single_model_metrics, {"accuracy": accuracy_voting, "f1": f1_voting}
        
    except Exception as e:
        logging.error(f"模型评估失败：{str(e)}")
        raise


def train_ensemble_model(epochs, sample_size, n_samples, features_train, train_labels, features_test, test_labels):
    """训练集成FNN二分类模型（多模型抽样训练）"""
    
    models = []        # 存储训练好的模型
    
    # 有放回抽样+多模型训练
    logging.info(f"\n===== 开始有放回抽样并训练{n_samples}个模型 =====")
    for i in range(n_samples):
        logging.info(f"\n----- 训练第{i+1}个模型 -----")
        # 有放回抽样
        sampled_features, sampled_labels = random_sample_with_replacement(
            features_train, train_labels, sample_size
        )
        logging.info(f"第{i+1}次抽样完成，样本数：{len(sampled_features)}")
        
        # 构建并训练模型
        model = build_fnn_model()
        history = model.fit(
            sampled_features, sampled_labels,
            epochs=epochs,
            batch_size=8,
            verbose=1
        )
        
        # 保存原始TF模型（如果需要）
        if FLAGS.save_tf_model:
            model_save_path = os.path.join(FLAGS.save_dir, f"saved_model_{i+1}")
            if not tf.io.gfile.exists(model_save_path):
                tf.io.gfile.makedirs(model_save_path)
            model.export(model_save_path)
            logging.info(f"模型{i+1}保存成功：{model_save_path}")
        
        models.append(model)
    
    # 评估模型性能
    single_metrics, ensemble_metrics = evaluate_ensemble_model(models, features_test, test_labels)
    
    return models, single_metrics, ensemble_metrics


def convert_tflite_model(model, model_idx):
    """将训练好的TF模型转换为TFLite格式"""
    try:
        converter = tf.lite.TFLiteConverter.from_keras_model(model)
        tflite_model = converter.convert()
        return tflite_model
    except Exception as e:
        logging.error(f"模型{model_idx} TFLite转换失败：{str(e)}")
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


def main(_):
    """主函数：执行完整的模型训练流程"""
    # 设置日志级别，确保调试信息输出
    logging.set_verbosity(logging.INFO)
    
    try:
        # 1. 加载和预处理数据
        features_train, train_labels, features_test, test_labels, scaler = get_data(
            FLAGS.train_data_path, FLAGS.test_data_path
        )
        
        # 2. 训练集成模型
        trained_models, single_metrics, ensemble_metrics = train_ensemble_model(
            FLAGS.epochs,
            FLAGS.sample_size,
            FLAGS.n_samples,
            features_train, 
            train_labels, 
            features_test, 
            test_labels
        )

        # 3. 转换并保存每个模型的TFLite版本
        for idx, model in enumerate(trained_models):
            tflite_model = convert_tflite_model(model, idx+1)
            save_tflite_model(tflite_model,
                              FLAGS.save_dir,
                              model_name=f"fnn_ensemble_model_{idx+1}.tflite")
        
        # 4. 保存标准化器
        scaler_path = os.path.join(FLAGS.save_dir, "scaler.npy")
        with tf.io.gfile.GFile(scaler_path, "wb") as f:
            np.save(f, [scaler.mean_, scaler.scale_])
        
        # 验证保存结果
        if tf.io.gfile.exists(scaler_path):
            logging.info(f"标准化器保存成功 - 均值：{scaler.mean_.round(4)}，标准差：{scaler.scale_.round(4)}")
        else:
            logging.error(f"标准化器保存失败 - 文件未找到：{scaler_path}")
        
        logging.info(f"\n训练流程执行完成！集成模型最终F1分数：{ensemble_metrics['f1']:.4f}")
        
    except Exception as e:
        logging.error(f"训练流程执行失败：{str(e)}")
        raise


if __name__ == "__main__":
    app.run(main)