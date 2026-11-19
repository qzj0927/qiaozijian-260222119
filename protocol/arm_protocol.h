/**
 * @file    arm_protocol.h
 * @brief   机械臂上位机通讯协议 V1.0（任务书附件一）
 *
 * 帧格式（两种链路共用，小端）：
 *   偏移 0      帧头     1 字节   固定 0xA5
 *   偏移 1      帧类型   1 字节   0x10/0x20/0x21/0x22/0x23/0x24
 *   偏移 2      载荷长度 1 字节   载荷字节数 N，不含帧头/类型/CRC
 *   偏移 3      载荷     N 字节
 *   偏移 3+N    CRC8      1 字节   对第 0 字节至第 3+N-1 字节逐字节计算
 *
 * 角度统一用 int16、单位 0.1°（900 表示 90.0°）。
 *
 * 本模块只做编解码，不碰 HAL、不碰 USB，因此可以在 PC 上用 gcc 直接单测。
 */
#ifndef PROTOCOL_ARM_PROTOCOL_H
#define PROTOCOL_ARM_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------- 常量 ---------------- */

#define ARM_FRAME_HEAD       0xA5u   /**< 帧头 */
#define ARM_FRAME_OVERHEAD   4u      /**< 帧头 + 类型 + 长度 + CRC */
#define ARM_MAX_PAYLOAD      32u     /**< 本工程协议最大载荷（0x10 为 19 字节）*/

/* 消息类型码 */
#define ARM_MSG_STATUS       0x10u   /**< 状态上报 设备->上位机 */
#define ARM_MSG_TARGET       0x20u   /**< 目标角度 上位机->设备 */
#define ARM_MSG_MODE         0x21u   /**< 模式切换 */
#define ARM_MSG_ESTOP        0x22u   /**< 急停 */
#define ARM_MSG_HEARTBEAT    0x23u   /**< 心跳，原样回发 */
#define ARM_MSG_VERSION      0x24u   /**< 版本查询/应答 */

/* 各消息载荷长度（协议固定值，解码时必须逐字节相等，否则视为非法帧） */
#define ARM_LEN_STATUS       19u
#define ARM_LEN_TARGET       11u
#define ARM_LEN_MODE          1u
#define ARM_LEN_ESTOP         1u
#define ARM_LEN_HEARTBEAT     1u
#define ARM_LEN_VERSION       2u

/* 版本号：设备侧固定 0x01、0x00 */
#define ARM_VERSION_MAJOR    0x01u
#define ARM_VERSION_MINOR    0x00u

/* 状态位定义（状态上报载荷偏移 18） */
#define ARM_ST_AUTO          (1u << 0)  /**< 0 手动，1 自动 */
#define ARM_ST_HOMING        (1u << 1)  /**< 正在回零 */
#define ARM_ST_FAULT         (1u << 2)  /**< 故障态 */
#define ARM_ST_MOVING        (1u << 3)  /**< 正在运动 */
#define ARM_ST_I2C_FAIL      (1u << 4)  /**< I2C 传感器失效 */
#define ARM_ST_CMD_TIMEOUT   (1u << 5)  /**< 指令超时 */

/* 模式切换取值（0x21 载荷） */
#define ARM_MODE_MANUAL      0u
#define ARM_MODE_AUTO        1u
#define ARM_MODE_HOMING      2u
#define ARM_MODE_FAULT_RESET 3u

/* 速度规划方式（0x20 载荷偏移 10） */
#define ARM_PROFILE_STEP     0u  /**< 阶跃，仅调试用 */
#define ARM_PROFILE_TRAP     1u  /**< 梯形 */
#define ARM_PROFILE_SCURVE   2u  /**< S 曲线 */

/* ---------------- 数据结构 ---------------- */

/** 关节序号，与协议字段顺序一致，不要随意调整 */
typedef enum {
    ARM_AXIS_BASE = 0,   /**< 底座旋转 */
    ARM_AXIS_SHOULDER,   /**< 肩 */
    ARM_AXIS_ELBOW,      /**< 肘 */
    ARM_AXIS_GRIPPER,    /**< 夹爪 */
    ARM_AXIS_COUNT
} arm_axis_id_t;

/** 状态上报 0x10 的语义结构 */
typedef struct {
    int16_t q_act[ARM_AXIS_COUNT];   /**< 实际角度 0.1° */
    int16_t q_tgt[ARM_AXIS_COUNT];   /**< 目标角度 0.1° */
    int16_t pitch;                   /**< 末端实测俯仰 0.1°，无传感器填 0 */
    uint8_t status;                  /**< 状态位，见 ARM_ST_xxx */
} arm_status_t;

/** 目标角度 0x20 的语义结构 */
typedef struct {
    int16_t  q_tgt[ARM_AXIS_COUNT];  /**< 目标角度 0.1° */
    uint16_t duration_ms;            /**< 预计运动时间，0 表示按最大速度 */
    uint8_t  profile;                /**< ARM_PROFILE_xxx */
} arm_target_t;

/** 解出的一帧（只在本模块内流转，避免上层直接摸字节） */
typedef struct {
    uint8_t type;
    uint8_t len;
    uint8_t payload[ARM_MAX_PAYLOAD];
} arm_frame_t;

/** 增量式解析器的内部状态 */
typedef enum {
    ARM_PARSE_HEAD = 0,
    ARM_PARSE_TYPE,
    ARM_PARSE_LEN,
    ARM_PARSE_PAYLOAD,
    ARM_PARSE_CRC
} arm_parse_state_t;

/** 字节流解析器：CDC 是变长字节流，必须支持逐字节喂入 */
typedef struct {
    arm_parse_state_t state;
    uint8_t index;                       /**< raw 缓冲区当前写入位置 */
    uint8_t len;                         /**< 本帧载荷长度 */
    uint8_t raw[3u + ARM_MAX_PAYLOAD];   /**< 帧头..载荷末尾，供 CRC 计算 */
} arm_parser_t;

/* ---------------- 编码 ---------------- */

/**
 * @brief 组帧：帧头 + 类型 + 长度 + 载荷 + CRC
 * @return 写入的字节数；out 空间不足或参数非法时返回 0
 */
uint32_t arm_encode(uint8_t type,
                    const uint8_t *payload,
                    uint8_t len,
                    uint8_t *out,
                    uint32_t out_size);

/** @brief 组 0x10 状态帧，返回帧长或 0 */
uint32_t arm_encode_status(const arm_status_t *st, uint8_t *out, uint32_t out_size);

/** @brief 组 0x24 版本应答帧，返回帧长或 0 */
uint32_t arm_encode_version(uint8_t *out, uint32_t out_size);

/* ---------------- 解码 ---------------- */

/** @brief 解析 0x10，成功返回 true；字段越界/长度不符返回 false */
bool arm_decode_status(const arm_frame_t *frame, arm_status_t *st);

/** @brief 解析 0x20；profile 越界时按梯形处理并仍返回 true */
bool arm_decode_target(const arm_frame_t *frame, arm_target_t *tgt);

/* ---------------- 字节流解析 ---------------- */

/** @brief 复位解析器（上电、重连、出错后调用） */
void arm_parser_init(arm_parser_t *parser);

/**
 * @brief 喂入一个字节
 * @param out 解析成功时写入完整帧
 * @return  1 = 收到一帧合法帧；0 = 还需要更多字节；-1 = 本帧非法（已自动复位，
 *          调用者不应回复、也不应把这次失败计入超时）
 */
int arm_parser_push(arm_parser_t *parser, uint8_t byte, arm_frame_t *out);

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_ARM_PROTOCOL_H */
