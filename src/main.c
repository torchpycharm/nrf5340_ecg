#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "spi_comm.h"
#include "feature_extraction.h"
#include <zephyr/sys/printk.h>
LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);
static feature_extraction_t feature_eng;
spi_frame_t frame;
/* 你的板卡 SPI 采样率（来自数据源，即树莓派）*/
#define ECG_FS 300         /* 或实际采样率，请与你树莓派发送一致 */
#define WINDOW_SEC 30      /* 每 30 秒做一次特征全集 */

int main(void)
{
    LOG_INF("APP start");
    printk("=== MAIN START ===\n");
    if (spi_comm_init() != 0) {
        LOG_ERR("Failed to init SPI");
        return -1;
    }
    feature_extraction_init(&feature_eng, ECG_FS, WINDOW_SEC);
    LOG_INF("Feature engine initialized");
    while (1) {
        printk("=== LOOP START ===\n");
        int ret = spi_comm_read_frame(&frame);
       if (ret != 0) {
            /* 忽略解析错误、校验错误、缺帧等 */
            k_sleep(K_MSEC(1));
            continue;
        }
            /* 恢复 16-bit ECG sample（假设 data_h high byte, data_l low byte，小端） */
        int16_t ecg = (int16_t)((frame.data_h << 8) | frame.data_l);
        printk("Received ECG sample: %d (id=%d type=%d)", ecg, frame.id, frame.type);
        // int ecg=1;
        
        /* --------------------------
         *   2. push sample into feature engine
         * -------------------------- */
        bool window_done = feature_extraction_push(&feature_eng, ecg);
        //  k_sleep(K_MSEC(1000));
        if (!window_done) {
            /* 仍在收集 30 秒数据 */
            printk("Collecting feature window...\n");
            continue;
        }

        /* --------------------------
         *   3. 30 秒窗口数据已满 → 立即提取 6 个特征
         * -------------------------- */
        double feats[6];
        feature_extraction_get(feats, &feature_eng);

        printk("=== 30s features ready ===\n");
        printk("Kurtosis: %.6f %.6f %.6f\n", feats[0], feats[1], feats[2]);
        printk("Skewness: %.6f %.6f %.6f\n", feats[3], feats[4], feats[5]);

        /* --------------------------
         *   4. （可选）标准化 features
         * --------------------------
         * feats[i] = (feats[i] - mean_all[i]) / std_all[i];
         * 注意：mean_all / std_all 必须来自 MATLAB 训练集
         */

        /* --------------------------
         *   5. 推理模型（C实现的决策树 / SVM / boosting）
         * --------------------------
         * int result = model_predict(feats);
         * printk("Prediction result = %d", result);
         */

        /* --------------------------
         *   6. reset for next 30s window
         * -------------------------- */
        feature_extraction_reset(&feature_eng);
        printk("Feature window reset\n");
    }

    return 0;
}
