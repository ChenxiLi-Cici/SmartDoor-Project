#include "input.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

// function prototype
//项目其他地方存在这样一个函数，请允许我在这里调用它
// *uid： 接收一块内存地址，函数会把读取到的 UID 字节写进去。
// *uidLen： 同样接收一个地址，函数会通过这个地址写回 UID 的实际长度
extern int PN532_Get_UID(uint8_t *uid, uint8_t *uidLen);
extern ADC_HandleTypeDef hadc2;
extern ADC_HandleTypeDef hadc3;

static bool adc_read_once(ADC_HandleTypeDef *hadc, uint16_t *value);

static uint32_t last_ldr_print_ms = 0; //记录上一次打印 LDR 数值是什么时候

static const uint8_t normal_card_uid[] = {0x9D, 0x9E, 0x29, 0x07};
static const uint8_t admin_card_uid[] = {0x1C, 0xEE, 0x0A, 0x07};

static bool ldr_is_armed = false;
static bool card_present_latched = false; //增加一个变量，防止同一张卡持续重复触发

void
sensors_init(void)
{
    ldr_is_armed = false;
    card_present_latched = false;
}

CardType_t
nfc_poll_card(void)
{
    uint8_t uid[7] = {0};
    uint8_t uid_len = 0;

    if (!PN532_Get_UID(uid, &uid_len)) { //没读到卡
        card_present_latched = false;
        return CARD_NONE;
    }

    if (card_present_latched) { //同一张卡不重复读
        return CARD_NONE;
    }

    card_present_latched = true; //记录新卡

    printf("NFC UID:"); //开始打印uid

    for (uint8_t i = 0; i < uid_len; i++) {
        printf(" %02X", uid[i]); //每次打印一个字节，02:至少显示两位，不足则补位--“01”“A2”“07”“91”这样
    }

    printf("\r\n");

    if (uid_len == sizeof(normal_card_uid) && memcmp(uid, normal_card_uid, sizeof(normal_card_uid)) == 0) { // 检测到是normal员工卡
		printf("Card type: NORMAL\r\n");
		return CARD_NORMAL;
    }

	if (uid_len == sizeof(admin_card_uid) && memcmp(uid, admin_card_uid, sizeof(admin_card_uid)) == 0) { // 检测到是admin管理员卡
		printf("Card type: ADMIN\r\n");
		return CARD_ADMIN;
	}

	printf("Card type: INVALID\r\n"); //都不是，返回invalid
	return CARD_INVALID;
}

void
ldr_arm(bool armed)
{
    ldr_is_armed = armed;

    if (armed)
    {
        /*
         * Reset LDR counters, edge-detection state
         * and passage timeout here later.
         */
    }
}

Event_t
ldr_poll(void)
{
    uint16_t ldr1_value = 0; //分别存储两只 LDR 的 ADC 结果
    uint16_t ldr2_value = 0; //分别存储两只 LDR 的 ADC 结果

    if (!ldr_is_armed) { //检查LDR是否开启
        return EVT_NONE; //如果 LDR 没有开启，就什么都不做。
    }

    if (!adc_read_once(&hadc2, &ldr1_value)) { //使用 ADC2 读取 LDR1，并把结果保存到 ldr1_value。LDR1 读取失败 → 本轮不产生任何事件
        return EVT_NONE;
    }

    if (!adc_read_once(&hadc3, &ldr2_value)) { ////使用 ADC3 读取 LDR2，并把结果保存到 ldr2_value。LDR2 读取失败 → 本轮不产生任何事件
        return EVT_NONE;
    }

    uint32_t now = HAL_GetTick();

    if ((now - last_ldr_print_ms) >= 200) { //如果已经过去至少 200 ms，就允许打印
        last_ldr_print_ms = now;

        printf("LDR1=%u LDR2=%u\r\n", (unsigned int)ldr1_value, (unsigned int)ldr2_value);
    }

    return EVT_NONE; //暂时先返回NONE。
}

//读取任意一个 ADC 的小工具函数
// *hadc: 表示要读取哪个 ADC
// *value: 把读取结果传回去的地址
static bool
adc_read_once(ADC_HandleTypeDef *hadc, uint16_t *value)
{
    if (HAL_ADC_Start(hadc) != HAL_OK) { //如果返回值不是 HAL_OK，说明 ADC 没有成功启动，因此直接返回 false。
        return false;
    }

    if (HAL_ADC_PollForConversion(hadc, 2) != HAL_OK) { //等待 ADC 完成转换，但最多等待 2 ms。
        HAL_ADC_Stop(hadc);
        return false;
    }

    *value = (uint16_t)HAL_ADC_GetValue(hadc); //取得转换后的数据，把数据写进这个地址对应的变量

    HAL_ADC_Stop(hadc); //这次转换结束后停止 ADC，并返回 true，表示成功取得数值
    return true;
}
