#!/usr/bin/env python3
"""
ECG 数据流处理模拟器
模拟 SPI 接收 → 缓冲 → 处理窗口 → 特征提取 → 蓝牙输出的完整流程
"""

import numpy as np
from collections import deque
import sys

# ==================== 配置 ====================
ECG_FS = 300           # 采样率 (Hz)
WINDOW_SEC = 30        # 30秒窗口
ECG_BUFFER_SIZE = ECG_FS * 2  # 2秒缓冲 = 600
TOTAL_SAMPLES = ECG_FS * WINDOW_SEC  # 单个窗口 9000 个样本
NUM_WINDOWS = 4        # 模拟 4 个完整窗口以验证流程
TOTAL_SIMULATION_SAMPLES = TOTAL_SAMPLES * NUM_WINDOWS  # 共 36000 个样本
BLE_QUEUE_SIZE = 10

# ==================== ECG 缓冲队列 ====================
class ECGBuffer:
    def __init__(self, size):
        self.buffer = deque(maxlen=size)
        self.total_received = 0
        self.total_lost = 0
        self.last_seq = 0

    def push(self, sample):
        """模拟 ecg_buffer_push"""
        # 检查丢帧
        if self.total_received > 0:
            expected_seq = self.last_seq + 1
            delta = sample['seq_num'] - expected_seq
            if delta > 0:
                print(f"  ⚠️  Lost {delta} frames (expected {expected_seq}, got {sample['seq_num']})")
                self.total_lost += delta
            elif delta < 0:
                print(f"  ⚠️  Out-of-order frame (expected {expected_seq}, got {sample['seq_num']})")
        
        self.last_seq = sample['seq_num']
        self.total_received += 1
        self.buffer.append(sample)
        
        # 缓冲满告警
        if len(self.buffer) > ECG_BUFFER_SIZE * 0.8:
            print(f"  ⚠️  Buffer high: {len(self.buffer)}/{ECG_BUFFER_SIZE}")

    def pop(self):
        """模拟 ecg_buffer_pop"""
        if len(self.buffer) == 0:
            return None
        return self.buffer.popleft()

    def count(self):
        return len(self.buffer)

    def stats(self):
        return {
            'received': self.total_received,
            'lost': self.total_lost,
            'loss_percent': (self.total_lost * 100.0) / self.total_received if self.total_received > 0 else 0
        }


# ==================== 窗口完整性管理 ====================
class WindowManager:
    def __init__(self, fs, window_sec):
        self.fs = fs
        self.window_sec = window_sec
        self.expected_samples = fs * window_sec
        self.sample_count = 0
        self.seq_last = 0
        self.seq_gaps = 0
        self.windows_completed = 0
        self.windows_invalid = 0

    def push(self, seq_num):
        """模拟 window_manager_push"""
        if self.sample_count > 0:
            expected_seq = self.seq_last + 1
            seq_delta = seq_num - expected_seq
            if seq_delta > 1:
                self.seq_gaps += seq_delta - 1
                print(f"  ⚠️  Sequence gap: expected {expected_seq}, got {seq_num}")
            elif seq_delta < 0:
                print(f"  ⚠️  Out-of-order: expected {expected_seq}, got {seq_num}")
        
        self.seq_last = seq_num
        self.sample_count += 1
        
        window_done = self.sample_count >= self.expected_samples
        
        if window_done:
            integrity_percent = (self.sample_count * 100) // self.expected_samples
            is_valid = integrity_percent >= 95
            self.windows_completed += 1
            if not is_valid:
                self.windows_invalid += 1
            
            print(f"  ✓ Window completed: {integrity_percent}% complete, {self.seq_gaps} gaps")
            if not is_valid:
                print(f"    ⚠️  INVALID (integrity < 95%)")
        
        return window_done

    def reset(self):
        self.sample_count = 0
        self.seq_gaps = 0
        self.seq_last = 0


# ==================== 特征提取 (新逻辑：批量处理) ====================
class FeatureExtraction:
    def __init__(self, fs, window_sec):
        self.fs = fs
        self.window_sec = window_sec
        self.expected_total = fs * window_sec  # 9000
        
        # 缓冲区存储9000个样本
        self.buffer = np.zeros(9000, dtype=np.int16)
        self.buffer_pos = 0
        self.ready = False
        
        # 三段统计器
        self.seg_samples = [[], [], []]

    def push(self, raw_sample):
        """模拟 feature_extraction_push - 收集样本到缓冲区"""
        if self.buffer_pos < 9000:
            self.buffer[self.buffer_pos] = raw_sample
            self.buffer_pos += 1
            
            # 当缓冲区刚好满时，立即处理
            if self.buffer_pos == 9000:
                self._process_window()
                self.ready = True
                return True
            return False
        
        # 已处理完成
        return False

    def _process_window(self):
        """模拟 process_window - 严格遵循MATLAB逻辑"""
        # 步骤1: 去均值
        mean_val = np.mean(self.buffer.astype(float))
        buffer_demean = self.buffer.astype(float) - mean_val
        
        # 步骤2: 截取前1秒 (300样本)
        processed = buffer_demean[self.fs:]  # fs=300
        
        # 步骤3: 降采样 1/4
        downsampled = processed[::4]
        
        # 步骤4: 分三段，每段725样本
        seg_size = 725  # 2175 / 3
        self.seg_samples[0] = downsampled[:seg_size]
        self.seg_samples[1] = downsampled[seg_size:2*seg_size]
        self.seg_samples[2] = downsampled[2*seg_size:3*seg_size]

    def get_features(self):
        """计算 kurtosis 和 skewness"""
        features = []
        
        # 计算 Kurtosis
        for samples in self.seg_samples:
            if len(samples) < 2:
                features.append(0.0)
                continue
            
            arr = np.array(samples)
            mean = np.mean(arr)
            std = np.std(arr)
            
            if std > 0:
                m4 = np.mean((arr - mean)**4)
                kurtosis = m4 / (std**4) - 3
            else:
                kurtosis = 0.0
            
            features.append(kurtosis)
        
        # 计算 Skewness
        for samples in self.seg_samples:
            if len(samples) < 2:
                features.append(0.0)
                continue
            
            arr = np.array(samples)
            mean = np.mean(arr)
            std = np.std(arr)
            
            if std > 0:
                m3 = np.mean((arr - mean)**3)
                skewness = m3 / (std**3)
            else:
                skewness = 0.0
            
            features.append(skewness)
        
        return features

    def reset(self):
        """重置所有状态，准备下一个窗口"""
        self.buffer_pos = 0
        self.ready = False
        self.seg_samples = [[], [], []]


# ==================== 数据生成 ====================
def generate_ecg_data(num_samples, fs=300):
    """生成模拟 ECG 数据 (连续多个窗口)"""
    t = np.arange(num_samples) / fs
    
    # 合成信号: 1Hz + 2Hz + 噪声（模拟真实ECG）
    ecg_data = (
        2.0 * np.sin(2 * np.pi * 1.0 * t) +  # 1 Hz 心率基频
        1.0 * np.sin(2 * np.pi * 2.0 * t) +  # 2 Hz 谐波
        0.5 * np.random.randn(num_samples)   # 电极噪声
    )
    
    # 缩放到 int16 范围
    ecg_data = np.clip(ecg_data * 1000, -32768, 32767).astype(np.int16)
    return ecg_data


# ==================== 主模拟循环 ====================
def simulate_ecg_processing():
    print("\n" + "="*65)
    print("  ECG 数据流处理模拟器 (批量处理新逻辑)")
    print("="*65)
    print(f"参数: FS={ECG_FS}Hz, Window={WINDOW_SEC}s, Single window={TOTAL_SAMPLES} samples")
    print(f"模拟: {NUM_WINDOWS} 个完整窗口 = {TOTAL_SIMULATION_SAMPLES} 个样本\n")
    
    # 初始化各模块
    ecg_buffer = ECGBuffer(ECG_BUFFER_SIZE)
    window_mgr = WindowManager(ECG_FS, WINDOW_SEC)
    feature_eng = FeatureExtraction(ECG_FS, WINDOW_SEC)
    
    # 生成 足够长的 ECG 数据
    ecg_data = generate_ecg_data(TOTAL_SIMULATION_SAMPLES)
    
    processed_windows = 0
    total_processed = 0
    
    print(f"📊 开始处理 {TOTAL_SIMULATION_SAMPLES} 个样本（{NUM_WINDOWS} 个窗口）...\n")
    
    for seq_num, raw_ecg in enumerate(ecg_data):
        # ===== SPI 接收 (模拟) =====
        sample = {
            'seq_num': seq_num,
            'ecg_data': int(raw_ecg),
            'timestamp_ms': seq_num * (1000 // ECG_FS)
        }
        
        # ===== 推入缓冲 =====
        ecg_buffer.push(sample)
        total_processed += 1
        
        # ===== 从缓冲弹出并处理 =====
        popped = ecg_buffer.pop()
        if popped is None:
            continue
        
        # ===== 特征提取 (新逻辑：收集9000个样本后批量处理) =====
        feat_done = feature_eng.push(popped['ecg_data'])
        
        # ===== 窗口完整性检查 =====
        window_done = window_mgr.push(popped['seq_num'])
        
        # ===== 检查同步 =====
        if window_done:
            print(f"\n[窗口 #{processed_windows+1}] 窗口完成（样本 {total_processed}）")
            print(f"  特征提取状态: ready={feature_eng.ready}, buffer_pos={feature_eng.buffer_pos}/9000")
            
            if not feature_eng.ready:
                print(f"  ❌ 窗口完成但特征未准备!")
                print(f"     Buffer pos: {feature_eng.buffer_pos}/9000")
                feature_eng.reset()
                window_mgr.reset()
                continue
            
            # ===== 输出结果 =====
            processed_windows += 1
            features = feature_eng.get_features()
            
            print(f"  {'='*60}")
            print(f"  ✅ 窗口 #{processed_windows} - 特征提取完成!")
            print(f"  {'='*60}")
            print(f"  特征结果:")
            print(f"    Kurtosis: [{features[0]:9.4f}, {features[1]:9.4f}, {features[2]:9.4f}]")
            print(f"    Skewness: [{features[3]:9.4f}, {features[4]:9.4f}, {features[5]:9.4f}]")
            print()
            
            # 重置
            feature_eng.reset()
            window_mgr.reset()
            
            if processed_windows >= NUM_WINDOWS:
                print(f"✓ 已完成 {processed_windows} 个完整窗口")
                break
        
        # 进度显示（每 3000 个样本）
        if (total_processed % 3000) == 0:
            print(f"[进度] {total_processed}/{TOTAL_SIMULATION_SAMPLES} samples | "
                  f"Buffer: {ecg_buffer.count()}/{ECG_BUFFER_SIZE} | "
                  f"Windows: {processed_windows} completed")
    
    # ===== 最终统计 =====
    print(f"\n{'='*65}")
    print("📈 最终统计")
    print(f"{'='*65}")
    
    buf_stats = ecg_buffer.stats()
    print(f"SPI接收:   {buf_stats['received']:5d} 帧, 丢失: {buf_stats['lost']:3d} 帧 ({buf_stats['loss_percent']:5.2f}%)")
    print(f"窗口管理:  {window_mgr.windows_completed} 完成, {window_mgr.windows_invalid} 无效")
    print(f"特征提取:  {processed_windows} 个完整的 30s 窗口已处理")
    print()


if __name__ == "__main__":
    try:
        simulate_ecg_processing()
        print("✓ 模拟完成")
        sys.exit(0)
    except Exception as e:
        print(f"❌ 错误: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
