#include "input.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

// function prototype
//项目其他地方存在这样一个函数，请允许我在这里调用它
// *uid： 接收一块内存地址，函数会把读取到的 UID 字节写进去。
// *uidLen： 同样接收一个地址，函数会通过这个地址写回 UID 的实际长度
extern int PN532_Get_UID(uint8_t *uid, uint8_t *uidLen);

static const uint8_t normal_card_uid[] = {0x9D, 0x9E, 0x29, 0x07};
static const uint8_t admin_card_uid[] = {0x1C, 0xEE, 0x0A, 0x07};

static bool card_present_swipe = false; //增加一个变量，防止同一张卡持续重复触发

void sensors_init(void)
{
    card_present_swipe = false;
}

CardType_t nfc_poll_card(void)
{
    uint8_t uid[7] = {0};
    uint8_t uid_len = 0;

    if (!PN532_Get_UID(uid, &uid_len)) { //没读到卡
        card_present_swipe = false;
        return CARD_NONE;
    }

    if (card_present_swipe) { //同一张卡不重复读
        return CARD_NONE;
    }

    card_present_swipe = true; //记录新卡

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
