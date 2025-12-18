/* feature_extraction.c */

#include "feature_extraction.h"
#include <math.h>
#include <string.h>
#include <zephyr/sys/printk.h>
void feature_extraction_init(feature_extraction_t *e,
                             uint32_t fs,
                             uint32_t window_sec)
{
    memset(e, 0, sizeof(*e));

    /* 保存配置 */
    e->fs = fs;
    e->window_sec = window_sec;

    online_moments_init(&e->seg1);
    online_moments_init(&e->seg2);
    online_moments_init(&e->seg3);

    uint32_t total_after_drop = fs * window_sec-fs;
    uint32_t total_after_decim = total_after_drop / 4;
    e->seg_len = total_after_decim / 3;

    e->drop_count = fs;     // drop first 1 second
    e->mean_acc = 0.0;
    e->mean_count = 0;
    e->seg_idx = 0;
    e->seg_pos = 0;
    e->ready = false;
}


/* 每来一个 SPI sample 调用一次 */
bool feature_extraction_push(feature_extraction_t *e, int16_t raw_sample)
{
    /* Stage 1 — 在线均值统计（整个窗口） */
    e->mean_acc += raw_sample;
    e->mean_count++;
    // printk("Mean accumulating: count=%u acc=%.2f\n",e->mean_count, e->mean_acc);
    printk("stats: mean_count=%u seg_idx=%u seg_pos=%u seg_len=%u\n",
       e->mean_count, e->seg_idx, e->seg_pos, e->seg_len);
    /* Stage 2 — 去掉前 fs 个样本 */
    if (e->drop_count > 0) {
        e->drop_count--;
        return false;
    }

    /* Stage 3 — 降采样 (1/4) */
    e->decim_count++;
    if (e->decim_count < 4) {
        return false;
    }
    e->decim_count = 0;

    /* 获取整体均值（随着流推近似） */
    double mean_value = e->mean_acc / (double)e->mean_count;

    /* 与 MATLAB 一致：sample = raw - mean */
    double s = (double)raw_sample - mean_value;

    /* Stage 4 — 写入三段 */
    online_moments_t *curseg =
        (e->seg_idx == 0 ? &e->seg1 :
         e->seg_idx == 1 ? &e->seg2 : &e->seg3);

    online_moments_update(curseg, s);
    e->seg_pos++;

    /* 段结束判断 */
    if (e->seg_pos >= e->seg_len) {
        e->seg_idx++;
        e->seg_pos = 0;

        if (e->seg_idx >= 3) {
            /* 30 秒全部完成 */
            e->ready = true;
            return true;
        }
    }

    return false;
}

void feature_extraction_get(double out[6], feature_extraction_t *e)
{
    /* 前三维 kurtosis，后三维 skewness */
    out[0] = (double)online_moments_kurtosis_raw(&e->seg1);
    out[1] = (double)online_moments_kurtosis_raw(&e->seg2);
    out[2] = (double)online_moments_kurtosis_raw(&e->seg3);

    out[3] = (double)online_moments_skewness(&e->seg1);
    out[4] = (double)online_moments_skewness(&e->seg2);
    out[5] = (double)online_moments_skewness(&e->seg3);
}

void feature_extraction_reset(feature_extraction_t *e)
{
    /* 只清空运行态，不动配置 */
    e->drop_count = e->fs;
    e->decim_count = 0;

    e->mean_acc = 0.0;
    e->mean_count = 0;
    e->mean_all = 0.0;

    online_moments_init(&e->seg1);
    online_moments_init(&e->seg2);
    online_moments_init(&e->seg3);

    e->seg_idx = 0;
    e->seg_pos = 0;
    e->ready = false;
}



// bool feature_extraction_push(feature_extraction_t *e, int16_t raw_sample)
// {
//     /* =========================
//      * Stage 0 — 均值统计阶段
//      * ========================= */
//     if (!e->mean_ready) {
//         e->mean_acc += raw_sample;
//         e->mean_count++;

//         /* 用满整个窗口的数据算均值（与 MATLAB 等价） */
//         if (e->mean_count >= e->fs * e->window_sec) {
//             e->mean_all = e->mean_acc / (double)e->mean_count;
//             e->mean_ready = true;
//         }
//         return false;
//     }

//     /* =========================
//      * Stage 1 — 丢弃前 fs 个样本
//      * ========================= */
//     if (e->drop_count > 0) {
//         e->drop_count--;
//         return false;
//     }

//     /* =========================
//      * Stage 2 — 降采样 1/4
//      * ========================= */
//     e->decim_count++;
//     if (e->decim_count < 4) {
//         return false;
//     }
//     e->decim_count = 0;

//     /* =========================
//      * Stage 3 — 去均值（固定 mean）
//      * ========================= */
//     double s = (double)raw_sample - e->mean_all;

//     /* =========================
//      * Stage 4 — Online moments
//      * ========================= */
//     online_moments_t *curseg =
//         (e->seg_idx == 0 ? &e->seg1 :
//          e->seg_idx == 1 ? &e->seg2 : &e->seg3);

//     online_moments_update(curseg, s);
//     e->seg_pos++;

//     if (e->seg_pos >= e->seg_len) {
//         e->seg_idx++;
//         e->seg_pos = 0;

//         if (e->seg_idx >= 3) {
//             e->ready = true;
//             return true;
//         }
//     }

//     return false;
// }
