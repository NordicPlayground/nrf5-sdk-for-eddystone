#ifndef EDDYSTONE_H__
#define EDDYSTONE_H__

#include <stdint.h>
#include "app_util_platform.h"

/*BLE Spec GAP defs in units of ms*/
#define MAX_ADV_INTERVAL                       (10240)
#define MIN_CONN_ADV_INTERVAL                  (20)
#define MIN_NON_CONN_ADV_INTERVAL              (100)

/*Eddystone specific defs*/
#define EDDYSTONE_UUID              			0xFEAA                            /**< UUID for Eddystone beacons according to specification. */

#define EDDYSTONE_UID_FRAME_TYPE    			0x00                              /**< UID frame type is fixed at 0x00. */
#define EDDYSTONE_UID_RFU           			0x00, 0x00                        /**< Reserved for future use according to specification. */

#define EDDYSTONE_URL_FRAME_TYPE    			0x10                              /**< URL Frame type is fixed at 0x10. */
#define EDDYSTONE_URL_SCHEME        			0x00                              /**< 0x00 = "http://www" URL prefix scheme according to specification. */

#define EDDYSTONE_TLM_FRAME_TYPE    			0x20                              /**< TLM frame type is fixed at 0x20. */
#define EDDYSTONE_EID_FRAME_TYPE    			0x30                              /**< EID frame type is fixed at 0x30. */

#define EDDYSTONE_FRAME_TYPE_LENGTH             (1)

#define EDDYSTONE_UID_LENGTH                    (20)
#define EDDYSTONE_UID_NAMESPACE_LENGTH          (10)
#define EDDYSTONE_UID_INSTANCE_LENGTH           (6)
#define EDDYSTONE_UID_RFU_LENGTH                (2)

#define EDDYSTONE_URL_LENGTH                    (20)
#define EDDYSTONE_URL_URL_SCHEME_LENGTH         (1)
#define EDDYSTONE_URL_ENCODED_URL_LENGTH        (17)

#define EDDYSTONE_TLM_LENGTH                    (14)
#define EDDYSTONE_TLM_VBATT_LENGTH				(2)
#define EDDYSTONE_TLM_TEMP_LENGTH				(2)
#define EDDYSTONE_TLM_ADV_CNT_LENGTH			(4)
#define EDDYSTONE_TLM_SEC_CNT_LENGTH			(4)

#define EDDYSTONE_EID_LENGTH                    (10)
#define EDDYSTONE_EID_ID_LENGTH                 (8)

#define EDDYSTONE_ETLM_LENGTH                   (19)
#define EDDYSTONE_ETLM_ECRYPTED_LENGTH          (EDDYSTONE_TLM_VBATT_LENGTH +   \
                                                 EDDYSTONE_TLM_TEMP_LENGTH +    \
                                                 EDDYSTONE_TLM_ADV_CNT_LENGTH + \
                                                 EDDYSTONE_TLM_SEC_CNT_LENGTH)

#define EDDYSTONE_ETLM_RFU                      (0x00)
#define EDDYSTONE_SPEC_VERSION_BYTE             (0x00)

typedef enum
{
    EDDYSTONE_FRAME_TYPE_UID = EDDYSTONE_UID_FRAME_TYPE,
    EDDYSTONE_FRAME_TYPE_URL = EDDYSTONE_URL_FRAME_TYPE,
    EDDYSTONE_FRAME_TYPE_TLM = EDDYSTONE_TLM_FRAME_TYPE,
    EDDYSTONE_FRAME_TYPE_EID = EDDYSTONE_EID_FRAME_TYPE
} eddystone_frame_type_t;

typedef enum
{
	EDDYSTONE_TLM_VERSION_TLM = 0x00,
	EDDYSTONE_TLM_VERSION_ETLM = 0x01
} eddystone_tlm_version_t;

typedef PACKED(struct)
{
    eddystone_frame_type_t frame_type;
    int8_t                 ranging_data; 	//Calibrated TX Power at 0m
    int8_t                 namespace[EDDYSTONE_UID_NAMESPACE_LENGTH];
    int8_t                 instance[EDDYSTONE_UID_INSTANCE_LENGTH];
    int8_t                 rfu[EDDYSTONE_UID_RFU_LENGTH];
} eddystone_uid_frame_t;

typedef PACKED(struct)
{
	eddystone_frame_type_t 	frame_type;
	int8_t					ranging_data;   //Calibrated TX Power at 0m
	int8_t					url_scheme;
	int8_t					encoded_url[EDDYSTONE_URL_ENCODED_URL_LENGTH];
} eddystone_url_frame_t;

typedef PACKED(struct)
{
	eddystone_frame_type_t 	frame_type;
	eddystone_tlm_version_t version;
	int8_t					vbatt[EDDYSTONE_TLM_VBATT_LENGTH];
	int8_t					temp[EDDYSTONE_TLM_TEMP_LENGTH];
	int8_t					adv_cnt[EDDYSTONE_TLM_ADV_CNT_LENGTH];
	int8_t					sec_cnt[EDDYSTONE_TLM_SEC_CNT_LENGTH];
} eddystone_tlm_frame_t;

typedef PACKED(struct)
{
	eddystone_frame_type_t 	frame_type;
	int8_t                  ranging_data; 	//Calibrated TX Power at 0m
    int8_t                  eid[EDDYSTONE_EID_ID_LENGTH];
} eddystone_eid_frame_t;

typedef PACKED(struct)
{
	eddystone_frame_type_t 	frame_type;
	eddystone_tlm_version_t version;
	int8_t					encrypted_tlm[EDDYSTONE_ETLM_ECRYPTED_LENGTH];
    int16_t                 random_salt;
    int16_t                 msg_integrity_check;
    int8_t                  rfu;
} eddystone_etlm_frame_t;


#endif
