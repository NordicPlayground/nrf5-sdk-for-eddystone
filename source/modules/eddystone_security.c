
#include <stdint.h>
#include <stdbool.h>

#include "eddystone_security.h"
#include "nrf_soc.h"
#include "pstorage.h"
#include "eddystone_flash.h"
#include "nrf_soc.h"
#include "tiny-aes128-c\aes.h"
#include "app_timer.h"
#include "eddystone_app_config.h"
#include "macros_common.h"
#include "SEGGER_RTT.h"
#include "debug_config.h"

//EAX Crypto
#include "modes.h"
#include "aes.h"

//ECDH CRypto
// Cifra
#include "curve25519.h"
#include "handy.h"
#include "testutil.h"

// RFC6234
#include "sha.h"
#include "testutil.h"

// #define TIMER_TEST
 // #define ETLM_PRINT_TEST
 // #define ECDH_TEST
 // #define CRYPTO_TEST
 // #define TEST_VECTOR
 // #define ETLM_DEBUG_SESH

#define STATIC_LOCK_CODE
// #define UNIQUE_LOCK_CODE

#ifdef SECURITY_DEBUG
    #include "SEGGER_RTT.h"
    #define DEBUG_PRINTF SEGGER_RTT_printf
#else
    #define DEBUG_PRINTF(...)
#endif

#define  SECURITY_TIMER_TIMEOUT  APP_TIMER_TICKS(1000, APP_TIMER_PRESCALER)
#define  MS_PER_TICK             ((1+APP_TIMER_PRESCALER)*1000)/32768
#define  RTC1_TICKS_MAX          16777216
#define  TK_ROLLOVER             65536

static eddystone_security_init_t m_security_init;

static nrf_ecb_hal_data_t m_aes_ecb_lk;    //AES encryption struct of global lock key

/**@brief timing structure*/
typedef struct
{
    uint32_t    time_seconds;
    uint8_t     k_scaler;
} eddystone_security_timing_t;

typedef struct
{
    nrf_ecb_hal_data_t          aes_ecb_ik;
    nrf_ecb_hal_data_t          aes_ecb_tk;
    uint8_t                     eid[EDDYSTONE_EID_ID_LENGTH];
    uint8_t                     pub_ecdh_key[ECS_ECDH_KEY_SIZE];
    eddystone_security_timing_t timing;
    bool                        is_occupied;
} eddystone_security_slot_t;

static eddystone_security_slot_t m_security_slots[APP_MAX_EID_SLOTS];

APP_TIMER_DEF(m_eddystone_security_timer);   //Security timer used to incrememnt the 32-bit second counter

//Forward Declaration:
static uint32_t eddystone_security_temp_key_generate(uint8_t slot_no);
static uint32_t eddystone_security_eid_generate(uint8_t slot_no);

#ifdef CRYPTO_TEST
static void print_array(uint8_t *p_data, uint32_t size)
{
    // Helper table for pretty printing
    uint8_t ascii_table[] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};

    for (uint32_t i = 0; i < size; i++)
    {
        uint8_t h,l;
        SEGGER_RTT_WriteString(0, "0x");
        h = ascii_table[((p_data[i] & 0xF0)>>4)];
        l = ascii_table[(p_data[i] & 0x0F)];
        SEGGER_RTT_Write(0, (char *)&h, 1);
        SEGGER_RTT_Write(0, (char *)&l, 1);
        SEGGER_RTT_WriteString(0, " ");
    }
    SEGGER_RTT_WriteString(0, "\r\n");
}
#endif

/**@brief Generates a device-unique beacon lock code from DEVICEID*/
static void eddystone_security_lock_code_init(uint8_t * p_lock_buff)
{
    uint8_t  cpy_offset = ECS_AES_KEY_SIZE/2;

    #ifdef UNIQUE_LOCK_CODE
    uint32_t device_id[2] = {NRF_FICR->DEVICEID[0],NRF_FICR->DEVICEID[1]};
    uint8_t  random_num[8] = {0x00};

    uint8_t  bytes_available;

    sd_rand_application_bytes_available_get(&bytes_available);
    while (bytes_available < ECS_AES_KEY_SIZE/2)
    {
       //wait for SD to acquire enough RNs
       sd_rand_application_bytes_available_get(&bytes_available);
    }

    sd_rand_application_vector_get(random_num, ECS_AES_KEY_SIZE/2);

    #endif

    #ifdef STATIC_LOCK_CODE
    uint8_t device_id[8] = {0xFF, 0xFF,0xFF, 0xFF,0xFF, 0xFF,0xFF, 0xFF};
    uint8_t  random_num[8] = {0xFF, 0xFF,0xFF, 0xFF,0xFF, 0xFF,0xFF, 0xFF};
    #endif

    memcpy(p_lock_buff, device_id, sizeof(device_id));
    memcpy(p_lock_buff + cpy_offset, random_num, sizeof(random_num));

    DEBUG_PRINTF(0, "Init Lock Key from FICR: ", 0);
    for (uint8_t i = 0; i < ECS_AES_KEY_SIZE; i++)
    {
        DEBUG_PRINTF(0, "0x%02x, ", *(p_lock_buff+i));
    }
    DEBUG_PRINTF(0, "\r\n", 0);
}

/**@brief Updates all active EID slots' timer*/
static void eddystone_security_update_time(void * p_context)
{
    uint32_t current_tick = 0;
    uint32_t tick_diff;
    float    ms = 0;
    app_timer_cnt_get(&current_tick);

    static float    ms_remainder_from_seconds = 0;
    static uint32_t previous_tick = 0;

    if (current_tick > previous_tick)
    {
        //ticks since last update
        tick_diff = current_tick - previous_tick;
    }
    else if (current_tick < previous_tick)
    {
        //RTC counter has overflown
        tick_diff = current_tick + (RTC1_TICKS_MAX - previous_tick);
    }

    //convert ticks to ms
    ms = (float)((float)tick_diff*(float)MS_PER_TICK + ms_remainder_from_seconds);

    if (ms >= 1000)
    {
        ms_remainder_from_seconds = ms - (uint32_t)(ms);
        //DEBUG_PRINTF(0, "EID CLOCK  : %d ms \r\n", (uint32_t)(ms));
        //Cycle through the slots
        for (uint8_t i = 0; i < APP_MAX_EID_SLOTS; i++)
        {
            if (m_security_slots[i].is_occupied)
            {
                m_security_slots[i].timing.time_seconds += (uint32_t)((ms)/1000);
                // DEBUG_PRINTF(0, "Slot[%d] Time: %d s \r\n", i, m_security_slots[i].timing.time_seconds);
                // DEBUG_PRINTF(0, "Rotation Time - Slot[%d]: %d s \r\n", i, (2 <<  m_security_slots[i].timing.k_scaler - 1));
                #ifndef TIMER_TEST

                if (m_security_slots[i].timing.time_seconds == TK_ROLLOVER)
                {
                    eddystone_security_temp_key_generate(i);
                }

                if (m_security_slots[i].timing.time_seconds % (2 << (m_security_slots[i].timing.k_scaler - 1)) == 0)
                {

                    eddystone_security_eid_generate(i);
                }
                #endif
            }
        }

    }
    else
    {
        ms_remainder_from_seconds = ms;
        // DEBUG_PRINTF(0, "EID CLOCK DELAYED : %d ms \r\n", 1000 - (uint32_t)(ms));
    }

    previous_tick = current_tick;
}

ret_code_t eddystone_security_init(eddystone_security_init_t * p_init)
{
    if (p_init->msg_cb != NULL)
    {
        uint32_t err_code;
        //Generate the lock code from FICR + RNG
        eddystone_security_lock_code_init(m_aes_ecb_lk.key);
        //Initialize the flash module and check if an key already exists
        err_code = eddystone_flash_init(m_aes_ecb_lk.key);
        APP_ERROR_CHECK(err_code);

        uint8_t ik_buff[ECS_AES_KEY_SIZE];
        uint8_t empty_check[ECS_AES_KEY_SIZE];

        memset(empty_check, 0xFF, ECS_AES_KEY_SIZE);

        m_security_init.msg_cb       = p_init->msg_cb;
        m_security_init.eid_slots_max   = p_init->eid_slots_max; //Not currently used, more or less a sanity check atm

        if (m_security_init.eid_slots_max > APP_MAX_EID_SLOTS)
        {
            return NRF_ERROR_INVALID_PARAM;
        }

        uint8_t scaler_buff[FLASH_BLOCK_SIZE];
        err_code = eddystone_scalers_flash_load(scaler_buff);
        APP_ERROR_CHECK(err_code);

        for (uint8_t i = 0; i < APP_MAX_EID_SLOTS; i++)
        {
            m_security_slots[i].is_occupied = false;
            //Close to TK rollover for easy testig of behaviour
            m_security_slots[i].timing.time_seconds = 65530;

            //Load any existing IKs from flash
            err_code = eddystone_key_flash_load(i, EDDYSTONE_FLASH_DATA_TYPE_IK, ik_buff);
            APP_ERROR_CHECK(err_code);

            while (eddystone_flash_is_busy())
            {
                //wait for flash
            }

            if (memcmp(ik_buff, empty_check, ECS_AES_KEY_SIZE) != 0)
            {
                memcpy(m_security_slots[i].aes_ecb_ik.key, ik_buff, ECS_AES_KEY_SIZE);
                m_security_slots[i].timing.k_scaler = scaler_buff[i];
                m_security_slots[i].is_occupied = true;

                eddystone_security_temp_key_generate(i);
                eddystone_security_eid_generate(i);
            }

        }
        DEBUG_PRINTF(0, "Lock Key in Security Module: ", 0);
        for (uint8_t i = 0; i < ECS_AES_KEY_SIZE; i++)
        {
            DEBUG_PRINTF(0, "0x%02x, ", (m_aes_ecb_lk.key[i]));
        }
        DEBUG_PRINTF(0, "\r\n", 0);

        err_code = app_timer_create(&m_eddystone_security_timer,
                                    APP_TIMER_MODE_REPEATED,
                                    eddystone_security_update_time);

        APP_ERROR_CHECK(err_code);

        err_code = app_timer_start(m_eddystone_security_timer, SECURITY_TIMER_TIMEOUT, NULL);

        return err_code;
    }

    return NRF_ERROR_NULL;
}

static ret_code_t eddystone_security_ecb_block_encrypt( nrf_ecb_hal_data_t * p_encrypt_blk )
{
    #if defined NRF52 && USE_ECB_ENCRYPT_HW == 1
        return sd_ecb_block_encrypt(p_encrypt_blk);

    #elif defined NRF51 && USE_ECB_ENCRYPT_HW == 1
        return sd_ecb_block_encrypt(p_encrypt_blk);

    #elif !USE_ECB_ENCRYPT_HW
        AES128_ECB_encrypt(p_encrypt_blk->cleartext, p_encrypt_blk->key, p_encrypt_blk->ciphertext);
        return NRF_SUCCESS;
    #endif
}

ret_code_t eddystone_security_lock_code_update( uint8_t * p_ecrypted_key )
{
    uint8_t temp_buff[ECS_AES_KEY_SIZE] = {0};
    AES128_ECB_decrypt(p_ecrypted_key, m_aes_ecb_lk.key, temp_buff);

    DEBUG_PRINTF(0,"New Lock Key:",0);
    for (uint8_t i = 0; i < 16; i++)
    {
        DEBUG_PRINTF(0,"0x%02x, ",temp_buff[i]);
    }
    DEBUG_PRINTF(0,"\r\n",0);

    memcpy(m_aes_ecb_lk.key, temp_buff, ECS_AES_KEY_SIZE);
    return eddystone_key_flash_store(0, EDDYSTONE_FLASH_DATA_TYPE_LK, m_aes_ecb_lk.key);
}

ret_code_t eddystone_security_unlock_prepare( uint8_t * p_challenge )
{
    memcpy(m_aes_ecb_lk.cleartext, p_challenge, ECS_AES_KEY_SIZE);

    DEBUG_PRINTF(0,"Before Encryption - Challenge Used:",0);
    for (uint8_t i = 0; i < 16; i++)
    {
        DEBUG_PRINTF(0,"0x%02x, ", m_aes_ecb_lk.cleartext[i]);
    }
    DEBUG_PRINTF(0,"\r\n",0);

    DEBUG_PRINTF(0,"Before Encryption - Lock Key Used:",0);
    for (uint8_t i = 0; i < 16; i++)
    {
        DEBUG_PRINTF(0,"0x%02x, ",m_aes_ecb_lk.key[i]);
    }
    DEBUG_PRINTF(0,"\r\n",0);

    return eddystone_security_ecb_block_encrypt(&m_aes_ecb_lk);
}

void eddystone_security_unlock_verify( uint8_t * p_unlock_token )
{
    DEBUG_PRINTF(0,"Ciphertext:",0);
    for (uint8_t i = 0; i < 16; i++)
    {
        DEBUG_PRINTF(0,"0x%02x, ",m_aes_ecb_lk.ciphertext[i]);
    }
    DEBUG_PRINTF(0,"\r\n",0);
    DEBUG_PRINTF(0,"Received Token:",0);
    for (uint8_t i = 0; i < 16; i++)
    {
        DEBUG_PRINTF(0,"0x%02x, ", *(p_unlock_token+i));
    }
    DEBUG_PRINTF(0,"\r\n",0);

    if (memcmp(p_unlock_token, m_aes_ecb_lk.ciphertext, ECS_AES_KEY_SIZE) == 0)
    {
        DEBUG_PRINTF(0,"Unlocked!\r\n",0);
        m_security_init.msg_cb(0, EDDYSTONE_SECURITY_MSG_UNLOCKED);
    }
}

ret_code_t eddystone_security_random_challenge_generate( uint8_t * p_rand_chlg_buff )
{
    ret_code_t err_code;
    err_code = sd_rand_application_vector_get(p_rand_chlg_buff,ECS_AES_KEY_SIZE);

    DEBUG_PRINTF(0, "Challenge: ", 0);
    for (uint8_t i = 0; i < ECS_AES_KEY_SIZE; i++)
    {
        DEBUG_PRINTF(0, "0x%02x, ", *(p_rand_chlg_buff+i));
    }
    DEBUG_PRINTF(0, "\r\n", 0);

    return err_code;
}

/**@brief Generates a EID with the Temporary Key*/
static ret_code_t eddystone_security_eid_generate(uint8_t slot_no)
{
    memset(m_security_slots[slot_no].aes_ecb_tk.cleartext, 0, ECS_AES_KEY_SIZE);
    m_security_slots[slot_no].aes_ecb_tk.cleartext[11] = m_security_slots[slot_no].timing.k_scaler;

    uint32_t k_bits_cleared_time = (m_security_slots[slot_no].timing.time_seconds >> m_security_slots[slot_no].timing.k_scaler) << m_security_slots[slot_no].timing.k_scaler;

    m_security_slots[slot_no].aes_ecb_tk.cleartext[12] = (uint8_t)((k_bits_cleared_time >> 24) & 0xff);
    m_security_slots[slot_no].aes_ecb_tk.cleartext[13] = (uint8_t)((k_bits_cleared_time >> 16) & 0xff);
    m_security_slots[slot_no].aes_ecb_tk.cleartext[14] = (uint8_t)((k_bits_cleared_time >> 8) & 0xff);
    m_security_slots[slot_no].aes_ecb_tk.cleartext[15] = (uint8_t)((k_bits_cleared_time) & 0xff);

    eddystone_security_ecb_block_encrypt(&m_security_slots[slot_no].aes_ecb_tk);
    memcpy(m_security_slots[slot_no].eid, m_security_slots[slot_no].aes_ecb_tk.ciphertext, EDDYSTONE_EID_ID_LENGTH);

    DEBUG_PRINTF(0, "Slot [%d] - EID: ", slot_no);
    for (uint8_t i = 0; i < EDDYSTONE_EID_ID_LENGTH; i++)
    {
        DEBUG_PRINTF(0, "0x%02x, ", (m_security_slots[slot_no].eid[i]));
    }
    DEBUG_PRINTF(0, "\r\n", 0);

    m_security_init.msg_cb(slot_no, EDDYSTONE_SECURITY_MSG_EID);

    return NRF_SUCCESS;
}

/**@brief Generates a temporary key with the Identity key*/
static ret_code_t eddystone_security_temp_key_generate(uint8_t slot_no)
{
    memset(m_security_slots[slot_no].aes_ecb_ik.cleartext, 0, ECS_AES_KEY_SIZE);
    m_security_slots[slot_no].aes_ecb_ik.cleartext[11] = 0xFF;
    m_security_slots[slot_no].aes_ecb_ik.cleartext[14] = (uint8_t)((m_security_slots[slot_no].timing.time_seconds >> 24) & 0xff);
    m_security_slots[slot_no].aes_ecb_ik.cleartext[15] = (uint8_t)((m_security_slots[slot_no].timing.time_seconds >> 16) & 0xff);
    eddystone_security_ecb_block_encrypt(&m_security_slots[slot_no].aes_ecb_ik);
    memcpy(m_security_slots[slot_no].aes_ecb_tk.key, m_security_slots[slot_no].aes_ecb_ik.ciphertext, ECS_AES_KEY_SIZE);

    DEBUG_PRINTF(0,"Slot [%d] - Temp Key:",slot_no);
    for (uint8_t i = 0; i < 16; i++)
    {
        DEBUG_PRINTF(0,"0x%02x, ",m_security_slots[slot_no].aes_ecb_tk.key[i]);
    }
    DEBUG_PRINTF(0,"\r\n",0);

    return NRF_SUCCESS;
}

ret_code_t eddystone_security_shared_ik_receive( uint8_t slot_no, uint8_t * p_encrypted_ik, uint8_t scaler_k )
{

    m_security_slots[slot_no].is_occupied = true;
    m_security_slots[slot_no].timing.k_scaler = scaler_k;

    AES128_ECB_decrypt(p_encrypted_ik, m_aes_ecb_lk.key, m_security_slots[slot_no].aes_ecb_ik.key);

    DEBUG_PRINTF(0,"Identity Key:",0);
    for (uint8_t i = 0; i < ECS_AES_KEY_SIZE; i++)
    {
        DEBUG_PRINTF(0,"0x%02x, ", m_security_slots[slot_no].aes_ecb_ik.key[i]);
    }
    DEBUG_PRINTF(0,"\r\n",0);

    eddystone_security_temp_key_generate(slot_no);
    eddystone_security_eid_generate(slot_no);

    m_security_init.msg_cb(slot_no, EDDYSTONE_SECURITY_MSG_IK);

    return NRF_SUCCESS;
}

ret_code_t eddystone_security_client_pub_ecdh_receive( uint8_t slot_no, uint8_t * p_pub_ecdh, uint8_t scaler_k )
{

    static uint8_t attempt_counter = 0;

    m_security_slots[slot_no].is_occupied = true;
    m_security_slots[slot_no].timing.k_scaler = scaler_k;

    uint8_t beacon_private[ECS_ECDH_KEY_SIZE];               // Beacon private ECDH key
    uint8_t beacon_public[ECS_ECDH_KEY_SIZE];              // Beacon public ECDH key
    uint8_t phone_public[ECS_ECDH_KEY_SIZE];                 // Phone public ECDH key
    uint8_t shared[ECS_ECDH_KEY_SIZE];                     // Shared secret ECDH key
//    uint8_t expect[ECS_ECDH_KEY_SIZE];                     // Expected result test vector
    const uint8_t salt[1] = {0x01};                        // Salt
    uint8_t identity_key[ECS_AES_KEY_SIZE];                // Identity Key

    #ifndef TEST_VECTOR
    //Get public 32-byte service ECDH key from phone
    memcpy(phone_public, p_pub_ecdh, ECS_ECDH_KEY_SIZE);
    //Generate random beacon private key
    uint8_t  pool_size;
    uint8_t  bytes_available;

    sd_rand_application_pool_capacity_get(&pool_size);
    sd_rand_application_bytes_available_get(&bytes_available);
    DEBUG_PRINTF(0,"RNG Pool Size: %d \r\n", pool_size);
    DEBUG_PRINTF(0,"RNG Bytes Avail: %d \r\n", bytes_available);

    while (bytes_available < pool_size)
    {
       //wait for SD to acquire enough RNs
       sd_rand_application_bytes_available_get(&bytes_available);
    }

    sd_rand_application_vector_get(beacon_private, pool_size);

    if (pool_size < ECS_ECDH_KEY_SIZE)
    {
        sd_rand_application_bytes_available_get(&bytes_available);
        //    sd_rand_application_bytes_available_get(&bytes_available);
        while (bytes_available < (ECS_ECDH_KEY_SIZE-pool_size))
        {
           //wait for SD to acquire enough RNs
           sd_rand_application_bytes_available_get(&bytes_available);
        }
        sd_rand_application_vector_get(beacon_private + pool_size, ECS_ECDH_KEY_SIZE-pool_size);
    }

    #endif

    //Create beacon public 32-byte ECDH key from private 32-byte ECDH key
    cf_curve25519_mul_base(beacon_public, beacon_private);


    #ifdef ECDH_TEST

    SEGGER_RTT_printf(0, "\r\n********* 4. Generate Beacon public 32-byte ECDH\r\n");
    SEGGER_RTT_printf(0, "\r\nBEACON PRIVATE ECDH:\r\n ");
    print_array(beacon_private, 32);
    SEGGER_RTT_printf(0, "\r\nBEACON PUBLIC ECDH:\r\n ");
    print_array(beacon_public, 32);

    #endif /*ECDH_TEST*/

    //Generate shared 32-byte ECDH secret from beacon private service ECDH key and phone public ECDH key
    cf_curve25519_mul(shared,  beacon_private, phone_public);

    #ifdef ECDH_TEST

    SEGGER_RTT_printf(0, "\r\n\r\n********* 5. Generate Shared 32-byte ECDH\r\n");
    SEGGER_RTT_printf(0, "\r\nPHONE PUBLIC ECDH:\r\n ");
    print_array(phone_public, 32);
    SEGGER_RTT_printf(0, "\r\nBEACON PRIVATE ECDH:\r\n ");
    print_array(beacon_private, 32);
    SEGGER_RTT_printf(0, "\r\nSHARED ECDH KEY:\r\n ");
    print_array(shared, 32);
    // SEGGER_RTT_printf(0, "\r\nEXPECT:\r\n ");
    // print_array(expect, 32);

    #endif /*ECDH_TEST*/


    //Generate key material using shared ECDH secret as salt and public_keys as key material. RFC 2104 HMAC-SHA256.
    uint8_t digest[64];
    uint8_t public_keys[64];
    memcpy(public_keys, phone_public, 32);
    memcpy(public_keys+32, beacon_public, 32);

    hmac(SHA256, shared, 32, public_keys, 64, digest);

    /* Zero check of the shared secret becoming zero, try again if so. Max attempt limit twice */
    uint8_t empty_check[32] = {0};

    if(memcmp(empty_check, shared, 32) == 0)
    {
        if (attempt_counter < 2)
        {
            attempt_counter++;
            DEBUG_PRINTF(0, "Key Regen Attempt: %d \r\n", attempt_counter);
            eddystone_security_client_pub_ecdh_receive(slot_no, p_pub_ecdh, scaler_k);
        }
    }
    else
    {
        attempt_counter = 0;
    }

    #ifdef ECDH_TEST

    SEGGER_RTT_printf(0, "\r\n\r\n********* 6. Generate key material from shared ECDH secret using RFC 2104 HMAC-SHA256 without salt\r\n");
    SEGGER_RTT_printf(0, "\r\nHMAC RETURN CODE: %d", sha_err_code);
    SEGGER_RTT_printf(0, "\r\nHMAC PUBLIC KEYS:\r\n ");
    print_array((uint8_t*)public_keys, 64);
    SEGGER_RTT_printf(0, "\r\nHMAC SHARED KEY INPUT:\r\n ");
    print_array((uint8_t*)shared, 32);
    SEGGER_RTT_printf(0, "\r\nHMAC DIGEST OUTPUT:\r\n ");
    print_array((uint8_t*)digest, 32);
    // SEGGER_RTT_printf(0, "\r\nEXPECT:\r\n ");
    // print_array(expect, 32);
    #endif /*ECDH_TEST*/


    //Generate 16-byte Identity Key from shared ECDH secret using RFC 2104 HMAC-SHA256 and salt
    uint8_t digest_salted[64];
    hmac(SHA256, salt, 1, digest, 32, digest_salted);
    DEBUG_PRINTF(0,"  hmac(SHA256, salt, 1, digest, 32, digest_salted);  \r\n" ,0);

    #ifdef ECDH_TEST
    SEGGER_RTT_printf(0, "\r\n\r\n********* 7. Generate 16-byte key material from shared ECDH secret using RFC 2104 HMAC-SHA256 and salt\r\n");
    SEGGER_RTT_printf(0, "\r\nHMAC DIGEST INPUT:\r\n ");
    print_array((uint8_t*)digest, 32);
    SEGGER_RTT_printf(0, "\r\nHMAC DIGEST SALTED OUTPUT:\r\n ");
    print_array((uint8_t*)digest_salted, 16);
    // SEGGER_RTT_printf(0, "\r\nEXPECT:\r\n ");
    // print_array(expect, 32);
    #endif /*ECDH_TEST*/


    for(uint8_t i = 0; i<16; i++)
    {
        identity_key[i] = digest_salted[i];
    }

    memcpy(m_security_slots[slot_no].aes_ecb_ik.key, identity_key, ECS_AES_KEY_SIZE);

    DEBUG_PRINTF(0,"Identity Key:",0);
    for (uint8_t i = 0; i < ECS_AES_KEY_SIZE; i++)
    {
        DEBUG_PRINTF(0,"0x%02x, ", m_security_slots[slot_no].aes_ecb_ik.key[i]);
    }
    DEBUG_PRINTF(0,"\r\n",0);

    memcpy(m_security_slots[slot_no].pub_ecdh_key, beacon_public, ECS_ECDH_KEY_SIZE);

    eddystone_security_temp_key_generate(slot_no);
    eddystone_security_eid_generate(slot_no);

    m_security_init.msg_cb(slot_no, EDDYSTONE_SECURITY_MSG_ECDH);
    m_security_init.msg_cb(slot_no, EDDYSTONE_SECURITY_MSG_IK);

    return NRF_SUCCESS;
}

void eddystone_security_pub_ecdh_get(uint8_t slot_no, uint8_t * p_edch_buffer)
{
    memcpy(p_edch_buffer, m_security_slots[slot_no].pub_ecdh_key, ECS_ECDH_KEY_SIZE);
}

uint32_t eddystone_security_clock_get(uint8_t slot_no)
{
    return m_security_slots[slot_no].timing.time_seconds;
}

void eddystone_security_eid_state_destroy(uint8_t slot_no)
{
    DEBUG_PRINTF(0,"Slot [%d] - Destroying EID state if slot was EID \r\n", slot_no);
    memset(&m_security_slots[slot_no],0,sizeof(eddystone_security_slot_t));
}

ret_code_t eddystone_security_eid_states_preserve(void)
{
    ret_code_t err_code;
    for(uint8_t i = 0; i < APP_MAX_EID_SLOTS; i++)
    {
        if (m_security_slots[i].is_occupied == true)
        {
            DEBUG_PRINTF(0,"Preserved EID Slots:",0);
            DEBUG_PRINTF(0,"[%d], ", i);

            err_code = eddystone_key_flash_store(i, EDDYSTONE_FLASH_DATA_TYPE_IK, m_security_slots[i].aes_ecb_ik.key);
            RETURN_IF_ERROR(err_code);

            err_code = eddystone_scaler_flash_store(i, m_security_slots[i].timing.k_scaler);
            RETURN_IF_ERROR(err_code);

            DEBUG_PRINTF(0,"\r\n",0);
        }
        else
        {
            DEBUG_PRINTF(0,"Emptied EID Slots:",0);
            DEBUG_PRINTF(0,"[%d], ", i);
            err_code = eddystone_key_flash_clear(i);
            RETURN_IF_ERROR(err_code);

            err_code = eddystone_scaler_flash_store(i, 0x00);
            RETURN_IF_ERROR(err_code);
        }
    }
    return NRF_SUCCESS;
}

uint8_t eddystone_security_scaler_get(uint8_t slot_no)
{
    return m_security_slots[slot_no].timing.k_scaler;
}

void eddystone_security_eid_get(uint8_t slot_no, uint8_t * p_eid_buffer)
{
    memcpy(p_eid_buffer, m_security_slots[slot_no].eid, EDDYSTONE_EID_ID_LENGTH);
}

void eddystone_security_encrypted_eid_id_key_get(uint8_t slot_no, uint8_t * p_key_buffer)
{
    memcpy(m_aes_ecb_lk.cleartext, m_security_slots[slot_no].aes_ecb_ik.key,ECS_AES_KEY_SIZE);
    eddystone_security_ecb_block_encrypt(&m_aes_ecb_lk);
    memcpy(p_key_buffer,m_aes_ecb_lk.ciphertext, ECS_AES_KEY_SIZE);
}

void eddystone_security_tlm_to_etlm( uint8_t ik_slot_no, eddystone_tlm_frame_t * p_tlm, eddystone_etlm_frame_t * p_etlm)
{
    //Data Preparation
    //--------------------------------------------------------------------------
    const uint8_t NONCE_SIZE    = 6;
    const uint8_t TAG_SIZE      = 2;
    const uint8_t SALT_SIZE     = 2;
    const uint8_t TLM_DATA_SIZE = EDDYSTONE_TLM_LENGTH - 2;
    const uint8_t EIK_SIZE      = ECS_AES_KEY_SIZE;

    cf_prp prp;                                                     // Describe the block cipher to use.

    uint8_t plain[TLM_DATA_SIZE]  = {0};                            // plaintext tlm, without the frame byte and version
    size_t nplain                 = TLM_DATA_SIZE;                  // Length of message plaintext.

    memcpy(plain, (uint8_t *)&p_tlm->vbatt, sizeof(plain));

    const uint8_t header          = 0;                              // Additionally authenticated data (AAD).
    size_t nheader                = 0;                              // Length of header (AAD). May be zero.

    uint8_t key[EIK_SIZE]         = {0};                            // Encryption/decryption key: EIK
    size_t  nkey                         = EIK_SIZE;                // Length of encryption/decryption key.

    memcpy(key, &m_security_slots[ik_slot_no].aes_ecb_ik.key, EIK_SIZE);

    uint8_t nonce[NONCE_SIZE]     = {0};                            // Nonce. This must not repeat for a given key.
    size_t nnonce                       = NONCE_SIZE;               // Length of nonce.First 4 bytes are beacon time base with k-bits cleared
                                                                    // Last two bits are randomly generated

    //Take the current timestamp and clear the lowest K bits, use it as nonce
    uint32_t k_bits_cleared_time = (m_security_slots[ik_slot_no].timing.time_seconds
                                    >> m_security_slots[ik_slot_no].timing.k_scaler)
                                    << m_security_slots[ik_slot_no].timing.k_scaler;


    nonce[0] = (uint8_t)((k_bits_cleared_time >> 24) & 0xff);
    nonce[1] = (uint8_t)((k_bits_cleared_time >> 16) & 0xff);
    nonce[2] = (uint8_t)((k_bits_cleared_time >> 8) & 0xff);
    nonce[3] = (uint8_t)((k_bits_cleared_time) & 0xff);

    //Generate random salt
    uint8_t salt[SALT_SIZE] = {0};
    sd_rand_application_vector_get(salt, SALT_SIZE);
    memcpy(&nonce[4], salt, SALT_SIZE);

    uint8_t cipher[EDDYSTONE_ETLM_ECRYPTED_LENGTH];                 // Ciphertext output. nplain bytes are written.
    uint8_t tag[TAG_SIZE] = {0};                                    // Authentication tag. ntag bytes are written.
    size_t  ntag                         = TAG_SIZE;                // Length of authentication tag.
    #ifdef ETLM_DEBUG_SESH
    uint8_t decrypted_tlm[TLM_DATA_SIZE];                           // Decryption result.
    #endif


    #ifdef ETLM_DEBUG_SESH
    uint8_t hardcode_tlm[12] = {0, 0, 28, 64, 0, 0, 0, 72, 0, 0, 0, 115};
    uint8_t hardcode_nonce[6] = {0,1,0,0,0xF6,0x83};
    uint8_t hardcode_eik[16] = {0x58, 0x94, 0x17, 0xB0, 0x32, 0x4B, 0x1B, 0x71, 0xD7, 0xA6,0x75, 0x18, 0x52, 0x86, 0x7A, 0xE8};

    memcpy(plain, hardcode_tlm, 12);
    memcpy(nonce, hardcode_nonce, 6);
    memcpy(key, hardcode_eik, 16);
    #endif

    //Encryption
    //--------------------------------------------------------------------------
    cf_aes_context ctx;
    cf_aes_init(&ctx, key, nkey);

    prp.encrypt = (cf_prp_block)cf_aes_encrypt;   // Encryption context
    prp.decrypt = (cf_prp_block)cf_aes_decrypt;   // Decryption context
    prp.blocksz = ECS_AES_KEY_SIZE;

    #ifdef ETLM_PRINT_TEST
    SEGGER_RTT_printf(0, "\r\n\r\nAES-128-EAX Encryption/Decryption Example using CIFRA Library\r\n");

    SEGGER_RTT_printf(0, "\r\nData for encryption\r\n");
    SEGGER_RTT_printf(0, "PLAINTEXT/TLM: ");
    print_array((uint8_t *)plain, TLM_DATA_SIZE);
    SEGGER_RTT_printf(0, "NONCE/SALT: ");
    print_array((uint8_t *)nonce, NONCE_SIZE);
    SEGGER_RTT_printf(0, "KEY/EIK: ");
    print_array((uint8_t *)key, EIK_SIZE);
    #endif
    cf_eax_encrypt( &prp,
                    &ctx,
                    plain,      // Plaintext input, aka TLM
                    nplain,     // Length of TLM
                    &header,    // Empty
                    nheader,    // Empty
                    nonce,      // Nonce input
                    nnonce,     // Length of nonce
                    cipher,     // Encrypted output
                    tag,        // Authentication tag output
                    ntag        // Length of authentication tag
                  );

      #ifdef ETLM_PRINT_TEST
      SEGGER_RTT_printf(0, "\r\nEncryption result\r\n");
      SEGGER_RTT_printf(0, "eTLM: ");
      print_array(cipher, TLM_DATA_SIZE);
      SEGGER_RTT_printf(0, "TAG: ");
      print_array(tag, sizeof(tag));

      cf_eax_decrypt( &prp,
                      &ctx,
                      cipher,         // Encrypted input
                      nplain,         // Length of encrypted input
                      &header,        // Empty
                      nheader,        // Empty
                      nonce,          // Nonce input
                      nnonce,         // Length of nonce
                      tag,            // Authentication tag input
                      ntag,           // Length of authentication tag
                      decrypted_tlm   // Decryption result
                    );

      SEGGER_RTT_printf(0, "\r\nDecryption result\r\n");
      SEGGER_RTT_printf(0, "PLAINTEXT/TLM: ");
      print_array(decrypted_tlm, TLM_DATA_SIZE);
      SEGGER_RTT_printf(0, "TAG: ");
      print_array(tag, sizeof(tag));
      #endif


    //Construct the eTLM
    //--------------------------------------------------------------------------
    p_etlm->frame_type = p_tlm->frame_type;
    p_etlm->version = EDDYSTONE_TLM_VERSION_ETLM;
    memcpy(p_etlm->encrypted_tlm, cipher, EDDYSTONE_ETLM_ECRYPTED_LENGTH);
    memcpy((uint8_t *)&p_etlm->random_salt, salt, SALT_SIZE);
    memcpy((uint8_t *)&p_etlm->msg_integrity_check,tag,TAG_SIZE);
    p_etlm->rfu = EDDYSTONE_ETLM_RFU;
}
