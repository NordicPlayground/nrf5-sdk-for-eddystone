
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


// RFC6234
#include "sha.h"


 // #define ETLM_PRINT_TEST
#define ECDH_PRINT_TEST
#define CRYPTO_TEST
 // #define TEST_VECTOR
 // #define ETLM_DEBUG_SESH

#define STATIC_LOCK_CODE
// #define UNIQUE_LOCK_CODE

#ifdef SECURITY_DEBUG
    #include "SEGGER_RTT.h"
    #include "print_array.h"
    #define DEBUG_PRINTF SEGGER_RTT_printf
    #define PRINT_ARRAY  print_array
#else
    #define DEBUG_PRINTF(...)
    #define PRINT_ARRAY(...)
#endif

#define  SECURITY_TIMER_TIMEOUT  APP_TIMER_TICKS(1000, APP_TIMER_PRESCALER)
#define  MS_PER_TICK             ((1+APP_TIMER_PRESCALER)*1000)/32768
#define  RTC1_TICKS_MAX          16777216
#define  TK_ROLLOVER             0x10000

static eddystone_security_init_t m_security_init;

static nrf_ecb_hal_data_t m_aes_ecb_lk;    //AES encryption struct of global lock key

/**@brief timing structure*/
typedef struct
{
    uint32_t    seconds;
    uint8_t     k_scaler;
} eddystone_security_timing_t;

typedef struct
{
    nrf_ecb_hal_data_t          aes_ecb_ik;
    nrf_ecb_hal_data_t          aes_ecb_tk;
    uint8_t                     eid[EDDYSTONE_EID_ID_LENGTH];
    eddystone_security_timing_t timing;
    bool                        is_occupied;
} eddystone_security_slot_t;

static eddystone_security_slot_t m_security_slot[APP_MAX_EID_SLOTS];

typedef struct
{
    uint8_t private[ECS_ECDH_KEY_SIZE];
    uint8_t public[ECS_ECDH_KEY_SIZE];
} ecdh_key_pair_t;

typedef struct
{
    ecdh_key_pair_t ecdh_key_pair;
} eddystone_security_ecdh_t;

static eddystone_security_ecdh_t m_ecdh;

APP_TIMER_DEF(m_eddystone_security_timer);   //Security timer used to incrememnt the 32-bit second counter

//Forward Declaration:
static uint32_t eddystone_security_temp_key_generate(uint8_t slot_no);
static uint32_t eddystone_security_eid_generate(uint8_t slot_no);
static void eddystone_security_lock_code_init(uint8_t * p_lock_buff);
static void eddystone_security_update_time(void * p_context);

ret_code_t eddystone_security_init(eddystone_security_init_t * p_init)
{
    if (p_init->msg_cb != NULL)
    {
        uint32_t err_code;

        //Generate the lock code from FICR + RNG or fetch it from flash if exists
        eddystone_security_lock_code_init(m_aes_ecb_lk.key);

        DEBUG_PRINTF(0, "Lock Key in Security Module: ", 0);
        PRINT_ARRAY(m_aes_ecb_lk.key, ECS_AES_KEY_SIZE);

        m_security_init.msg_cb       = p_init->msg_cb;

        memset(&m_ecdh,0,sizeof(eddystone_security_ecdh_t));

        //Read ECDH key pair from flash
        uint8_t pub_key_buff[ECS_ECDH_KEY_SIZE];
        uint8_t priv_key_buff[ECS_ECDH_KEY_SIZE];

        DEBUG_PRINTF(0, "ECDH Keys from Flash: ", 0);
        err_code = eddystone_flash_access_ecdh_key_pair(priv_key_buff,
                                                        pub_key_buff,
                                                        EDDYSTONE_FLASH_ACCESS_READ);

        FLASH_OP_WAIT();

        DEBUG_PRINTF(0, "Private Key from Flash: ", 0);
        PRINT_ARRAY(priv_key_buff, ECS_ECDH_KEY_SIZE);

        DEBUG_PRINTF(0, "Public Key from Flash: ", 0);
        PRINT_ARRAY(pub_key_buff, ECS_ECDH_KEY_SIZE);

        if(!eddystone_flash_read_is_empty(priv_key_buff,ECS_ECDH_KEY_SIZE)
            &&
           !eddystone_flash_read_is_empty(pub_key_buff,ECS_ECDH_KEY_SIZE))
        {
            memcpy(m_ecdh.ecdh_key_pair.private,priv_key_buff,ECS_ECDH_KEY_SIZE);
            memcpy(m_ecdh.ecdh_key_pair.public,pub_key_buff,ECS_ECDH_KEY_SIZE);
            m_security_init.msg_cb(0, EDDYSTONE_SECURITY_MSG_ECDH);
        }

        for (uint8_t i = 0; i < APP_MAX_EID_SLOTS; i++)
        {
            m_security_slot[i].timing.seconds = 65280;
            //Initial time as recommended by google to test TK rollover behaviour
        }

        err_code = app_timer_create(&m_eddystone_security_timer,
                                    APP_TIMER_MODE_REPEATED,
                                    eddystone_security_update_time);

        APP_ERROR_CHECK(err_code);

        err_code = app_timer_start(m_eddystone_security_timer, SECURITY_TIMER_TIMEOUT, NULL);
        return err_code;
    }
    return NRF_ERROR_NULL;
}

void eddystone_security_eid_slots_restore(uint8_t slot_no, eddystone_eid_config_t * p_restore_data)
{
    m_security_slot[slot_no].timing.k_scaler = p_restore_data->k_scaler;
    m_security_slot[slot_no].timing.seconds = p_restore_data->seconds;
    memcpy(m_security_slot[slot_no].aes_ecb_ik.key, p_restore_data->ik, ECS_AES_KEY_SIZE);
    m_security_slot[slot_no].is_occupied = true;
    m_security_init.msg_cb(slot_no, EDDYSTONE_SECURITY_MSG_IK);
    eddystone_security_temp_key_generate(slot_no);
    eddystone_security_eid_generate(slot_no);
}

/**@brief Updates all active EID slots' timer*/
static void eddystone_security_update_time(void * p_context)
{
    static uint32_t us_delay = 0;
    const uint32_t US_PER_S = 1000000;
    static uint32_t timer_persist = 0;

    //For every 1 second interrupt, there is 30 us delay with timer prescaler set at 0.
    us_delay += 30;

    if (APP_TIMER_PRESCALER != 0)
    {
        //If the prescaler is not 0, then a new us_delay increment needs to be calculated...
        //Trigger a run time error here to prevent developers from blindly changing the prescaler
        APP_ERROR_CHECK(NRF_ERROR_INVALID_PARAM);
    }

    //Cycle through the slots
    for (uint8_t i = 0; i < APP_MAX_EID_SLOTS; i++)
    {
        if (m_security_slot[i].is_occupied)
        {
            m_security_slot[i].timing.seconds++;

            if (m_security_slot[i].timing.seconds % TK_ROLLOVER == 0)
            {
                eddystone_security_temp_key_generate(i);
            }

            if ((m_security_slot[i].timing.seconds % (2 << (m_security_slot[i].timing.k_scaler - 1))) == 0)
            {
                eddystone_security_eid_generate(i);
            }

            //when us_delay accumulates to more than 1 second, add 1 more sec to the clock.
            //Also do the TK roll over and K scaler timer checks again
            if(us_delay >= US_PER_S)
            {
                m_security_slot[i].timing.seconds++;
                us_delay -= US_PER_S;

                if (m_security_slot[i].timing.seconds % TK_ROLLOVER == 0)
                {
                    eddystone_security_temp_key_generate(i);
                }

                if ((m_security_slot[i].timing.seconds % (2 << (m_security_slot[i].timing.k_scaler - 1))) == 0)
                {
                    eddystone_security_eid_generate(i);
                }
            }
        }
    }

    //Every 24 hr, write the new EID timer to flash
    timer_persist++;
    const uint32_t TWENTY_FOUR_HOURS = 60*60*24;
    if (timer_persist >= TWENTY_FOUR_HOURS)
    {
        for(uint8_t i = 0; i <APP_MAX_EID_SLOTS; i++)
        {
            if(m_security_slot[i].is_occupied)
            {
                m_security_init.msg_cb(i, EDDYSTONE_SECURITY_MSG_STORE_TIME);
            }
        }
        timer_persist = 0;
    }
}

/**@brief Generates a device-unique beacon lock code from DEVICEID
 *        and RNG and copies it to the buffer, if no lock key exists
 *        in flash already.
 */
static void eddystone_security_lock_code_init(uint8_t * p_lock_buff)
{
    ret_code_t err_code;

    DEBUG_PRINTF(0, "Reading Lock Key From Flash \r\n",0);
    err_code = eddystone_flash_access_lock_key(p_lock_buff, EDDYSTONE_FLASH_ACCESS_READ);
    APP_ERROR_CHECK(err_code);

    FLASH_OP_WAIT();

    //If no lock keys exist, then generate one and copy it to buffer
    if(eddystone_flash_read_is_empty(p_lock_buff, ECS_AES_KEY_SIZE))
    {
        uint8_t  cpy_offset = ECS_AES_KEY_SIZE/2;

        #ifdef UNIQUE_LOCK_CODE
        uint32_t device_id[2] = {NRF_FICR->DEVICEID[0],NRF_FICR->DEVICEID[1]};
        uint8_t  random_num[8] = {0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08};
        #endif

        #ifdef STATIC_LOCK_CODE
        uint8_t device_id[8] = {0xFF, 0xFF,0xFF, 0xFF,0xFF, 0xFF,0xFF, 0xFF};
        uint8_t  random_num[8] = {0xFF, 0xFF,0xFF, 0xFF,0xFF, 0xFF,0xFF, 0xFF};
        #endif

        memcpy(p_lock_buff, device_id, sizeof(device_id));
        memcpy(p_lock_buff + cpy_offset, random_num, sizeof(random_num));

        DEBUG_PRINTF(0, "Writing Lock Key to Flash \r\n",0);

        err_code = eddystone_flash_access_lock_key(p_lock_buff, EDDYSTONE_FLASH_ACCESS_WRITE);
        APP_ERROR_CHECK(err_code);

        FLASH_OP_WAIT();
    }
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
    return eddystone_flash_access_lock_key(m_aes_ecb_lk.key, EDDYSTONE_FLASH_ACCESS_WRITE);
}


ret_code_t eddystone_security_unlock_prepare( uint8_t * p_challenge )
{
    memcpy(m_aes_ecb_lk.cleartext, p_challenge, ECS_AES_KEY_SIZE);
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
    memset(m_security_slot[slot_no].aes_ecb_tk.cleartext, 0, ECS_AES_KEY_SIZE);
    m_security_slot[slot_no].aes_ecb_tk.cleartext[11] = m_security_slot[slot_no].timing.k_scaler;

    uint32_t k_bits_cleared_time = (m_security_slot[slot_no].timing.seconds >> m_security_slot[slot_no].timing.k_scaler) << m_security_slot[slot_no].timing.k_scaler;

    m_security_slot[slot_no].aes_ecb_tk.cleartext[12] = (uint8_t)((k_bits_cleared_time >> 24) & 0xff);
    m_security_slot[slot_no].aes_ecb_tk.cleartext[13] = (uint8_t)((k_bits_cleared_time >> 16) & 0xff);
    m_security_slot[slot_no].aes_ecb_tk.cleartext[14] = (uint8_t)((k_bits_cleared_time >> 8) & 0xff);
    m_security_slot[slot_no].aes_ecb_tk.cleartext[15] = (uint8_t)((k_bits_cleared_time) & 0xff);

    eddystone_security_ecb_block_encrypt(&m_security_slot[slot_no].aes_ecb_tk);
    memcpy(m_security_slot[slot_no].eid, m_security_slot[slot_no].aes_ecb_tk.ciphertext, EDDYSTONE_EID_ID_LENGTH);

    DEBUG_PRINTF(0, "Slot [%d] - EID: ", slot_no);
    for (uint8_t i = 0; i < EDDYSTONE_EID_ID_LENGTH; i++)
    {
        DEBUG_PRINTF(0, "0x%02x, ", (m_security_slot[slot_no].eid[i]));
    }
    DEBUG_PRINTF(0, "\r\n", 0);

    m_security_init.msg_cb(slot_no, EDDYSTONE_SECURITY_MSG_EID);

    return NRF_SUCCESS;
}

/**@brief Generates a temporary key with the Identity key*/
static ret_code_t eddystone_security_temp_key_generate(uint8_t slot_no)
{
    memset(m_security_slot[slot_no].aes_ecb_ik.cleartext, 0, ECS_AES_KEY_SIZE);
    m_security_slot[slot_no].aes_ecb_ik.cleartext[11] = 0xFF;
    m_security_slot[slot_no].aes_ecb_ik.cleartext[14] = (uint8_t)((m_security_slot[slot_no].timing.seconds >> 24) & 0xff);
    m_security_slot[slot_no].aes_ecb_ik.cleartext[15] = (uint8_t)((m_security_slot[slot_no].timing.seconds >> 16) & 0xff);
    eddystone_security_ecb_block_encrypt(&m_security_slot[slot_no].aes_ecb_ik);
    memcpy(m_security_slot[slot_no].aes_ecb_tk.key, m_security_slot[slot_no].aes_ecb_ik.ciphertext, ECS_AES_KEY_SIZE);

    DEBUG_PRINTF(0,"Slot [%d] - Temp Key:",slot_no);
    for (uint8_t i = 0; i < 16; i++)
    {
        DEBUG_PRINTF(0,"0x%02x, ",m_security_slot[slot_no].aes_ecb_tk.key[i]);
    }
    DEBUG_PRINTF(0,"\r\n",0);

    return NRF_SUCCESS;
}

ret_code_t eddystone_security_shared_ik_receive( uint8_t slot_no, uint8_t * p_encrypted_ik, uint8_t scaler_k )
{

    m_security_slot[slot_no].is_occupied = true;
    m_security_slot[slot_no].timing.k_scaler = scaler_k;

    AES128_ECB_decrypt(p_encrypted_ik, m_aes_ecb_lk.key, m_security_slot[slot_no].aes_ecb_ik.key);

    DEBUG_PRINTF(0,"Identity Key:",0);
    for (uint8_t i = 0; i < ECS_AES_KEY_SIZE; i++)
    {
        DEBUG_PRINTF(0,"0x%02x, ", m_security_slot[slot_no].aes_ecb_ik.key[i]);
    }
    DEBUG_PRINTF(0,"\r\n",0);

    eddystone_security_temp_key_generate(slot_no);
    eddystone_security_eid_generate(slot_no);

    m_security_init.msg_cb(slot_no, EDDYSTONE_SECURITY_MSG_IK);

    return NRF_SUCCESS;
}

/**@brief Generates a the private/public ECDH key pair
 *
 * @param[out]  p_priv_buffer   buffer of size 32 bytes to hold the private key
 * @param[out]  p_pub_buffer    buffer of size 32 bytes to hold the public key
 */
static void eddystone_beacon_ecdh_pair_generate(uint8_t * p_priv_buffer, uint8_t * p_pub_buffer)
{
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

    sd_rand_application_vector_get(p_priv_buffer, pool_size);

    if (pool_size < ECS_ECDH_KEY_SIZE)
    {
        sd_rand_application_bytes_available_get(&bytes_available);
        //    sd_rand_application_bytes_available_get(&bytes_available);
        while (bytes_available < (ECS_ECDH_KEY_SIZE-pool_size))
        {
           //wait for SD to acquire enough RNs
           sd_rand_application_bytes_available_get(&bytes_available);
        }
        sd_rand_application_vector_get(p_priv_buffer + pool_size, ECS_ECDH_KEY_SIZE-pool_size);
    }

    //Create beacon public 32-byte ECDH key from private 32-byte ECDH key
    cf_curve25519_mul_base(p_pub_buffer, p_priv_buffer);

    #ifdef ECDH_PRINT_TEST

    SEGGER_RTT_printf(0, "\r\n********* 4. Generate Beacon public 32-byte ECDH\r\n");
    SEGGER_RTT_printf(0, "\r\nBEACON PRIVATE ECDH:\r\n ");
    PRINT_ARRAY(p_priv_buffer, 32);
    SEGGER_RTT_printf(0, "\r\nBEACON PUBLIC ECDH:\r\n ");
    PRINT_ARRAY(p_pub_buffer, 32);

    #endif /*ECDH_PRINT_TEST*/
}

ret_code_t eddystone_security_client_pub_ecdh_receive( uint8_t slot_no, uint8_t * p_pub_ecdh, uint8_t scaler_k )
{
    static uint8_t attempt_counter = 0;

    m_security_slot[slot_no].is_occupied = true;
    m_security_slot[slot_no].timing.k_scaler = scaler_k;

    uint8_t zeros[ECS_ECDH_KEY_SIZE] = {0};                // Array of zeros for checking if there are already keys

    uint8_t beacon_private[ECS_ECDH_KEY_SIZE];             // Beacon private ECDH key
    uint8_t beacon_public[ECS_ECDH_KEY_SIZE];              // Beacon public ECDH key
    uint8_t phone_public[ECS_ECDH_KEY_SIZE];               // Phone public ECDH key
    uint8_t shared[ECS_ECDH_KEY_SIZE];                     // Shared secret ECDH key
    const uint8_t salt[1] = {0x01};                        // Salt
    uint8_t identity_key[ECS_AES_KEY_SIZE];                // Identity Key

    //Get public 32-byte service ECDH key from phone
    memcpy(phone_public, p_pub_ecdh, ECS_ECDH_KEY_SIZE);

    if (memcmp(m_ecdh.ecdh_key_pair.public,zeros,ECS_ECDH_KEY_SIZE) == 0)
    {
        eddystone_beacon_ecdh_pair_generate(beacon_private, beacon_public);
        memcpy(m_ecdh.ecdh_key_pair.private, beacon_private,ECS_ECDH_KEY_SIZE);
        memcpy(m_ecdh.ecdh_key_pair.public,  beacon_public,ECS_ECDH_KEY_SIZE);
    }
    else
    {
        memcpy(beacon_private, m_ecdh.ecdh_key_pair.private, ECS_ECDH_KEY_SIZE);
        memcpy(beacon_public, m_ecdh.ecdh_key_pair.public, ECS_ECDH_KEY_SIZE);
    }

    //Generate shared 32-byte ECDH secret from beacon private service ECDH key and phone public ECDH key
    cf_curve25519_mul(shared,  beacon_private, phone_public);

    #ifdef ECDH_PRINT_TEST

    SEGGER_RTT_printf(0, "\r\n\r\n********* 5. Generate Shared 32-byte ECDH\r\n");
    SEGGER_RTT_printf(0, "\r\nPHONE PUBLIC ECDH:\r\n ");
    PRINT_ARRAY(phone_public, 32);
    SEGGER_RTT_printf(0, "\r\nBEACON PRIVATE ECDH:\r\n ");
    PRINT_ARRAY(beacon_private, 32);
    SEGGER_RTT_printf(0, "\r\nSHARED ECDH KEY:\r\n ");
    PRINT_ARRAY(shared, 32);


    #endif /*ECDH_PRINT_TEST*/


    //Generate key material using shared ECDH secret as salt and public_keys as key material. RFC 2104 HMAC-SHA256.
    uint8_t digest[64];
    uint8_t public_keys[64];
    memcpy(public_keys, phone_public, 32);
    memcpy(public_keys+32, beacon_public, 32);

    hmac(SHA256, shared, 32, public_keys, 64, digest);

    /* Zero check of the shared secret becoming zero, try generating a new key pair if so. Max attempt limit twice */
    uint8_t empty_check[32] = {0};

    if(memcmp(empty_check, shared, 32) == 0)
    {
        if (attempt_counter < 2)
        {
            attempt_counter++;
            DEBUG_PRINTF(0, "Key Regen Attempt: %d \r\n", attempt_counter);
            eddystone_beacon_ecdh_pair_generate(beacon_private, beacon_public);
        }
    }
    else
    {
        attempt_counter = 0;
    }

    #ifdef ECDH_PRINT_TEST

    SEGGER_RTT_printf(0, "\r\n\r\n********* 6. Generate key material from shared ECDH secret using RFC 2104 HMAC-SHA256 without salt\r\n");
    SEGGER_RTT_printf(0, "\r\nHMAC PUBLIC KEYS:\r\n ");
    PRINT_ARRAY((uint8_t*)public_keys, 64);
    SEGGER_RTT_printf(0, "\r\nHMAC SHARED KEY INPUT:\r\n ");
    PRINT_ARRAY((uint8_t*)shared, 32);
    SEGGER_RTT_printf(0, "\r\nHMAC DIGEST OUTPUT:\r\n ");
    PRINT_ARRAY((uint8_t*)digest, 32);
    #endif /*ECDH_PRINT_TEST*/


    //Generate 16-byte Identity Key from shared ECDH secret using RFC 2104 HMAC-SHA256 and salt
    uint8_t digest_salted[64];
    hmac(SHA256, salt, 1, digest, 32, digest_salted);
    DEBUG_PRINTF(0,"  hmac(SHA256, salt, 1, digest, 32, digest_salted);  \r\n" ,0);

    #ifdef ECDH_PRINT_TEST
    SEGGER_RTT_printf(0, "\r\n\r\n********* 7. Generate 16-byte key material from shared ECDH secret using RFC 2104 HMAC-SHA256 and salt\r\n");
    SEGGER_RTT_printf(0, "\r\nHMAC DIGEST INPUT:\r\n ");
    PRINT_ARRAY((uint8_t*)digest, 32);
    SEGGER_RTT_printf(0, "\r\nHMAC DIGEST SALTED OUTPUT:\r\n ");
    PRINT_ARRAY((uint8_t*)digest_salted, 16);
    #endif /*ECDH_PRINT_TEST*/

    for(uint8_t i = 0; i<16; i++)
    {
        identity_key[i] = digest_salted[i];
    }

    memcpy(m_security_slot[slot_no].aes_ecb_ik.key, identity_key, ECS_AES_KEY_SIZE);

    DEBUG_PRINTF(0,"Identity Key:",0);
    for (uint8_t i = 0; i < ECS_AES_KEY_SIZE; i++)
    {
        DEBUG_PRINTF(0,"0x%02x, ", m_security_slot[slot_no].aes_ecb_ik.key[i]);
    }
    DEBUG_PRINTF(0,"\r\n",0);

    eddystone_security_temp_key_generate(slot_no);
    eddystone_security_eid_generate(slot_no);

    m_security_init.msg_cb(slot_no, EDDYSTONE_SECURITY_MSG_ECDH);
    m_security_init.msg_cb(slot_no, EDDYSTONE_SECURITY_MSG_IK);

    return eddystone_security_ecdh_pair_preserve();
}

void eddystone_security_pub_ecdh_get(uint8_t slot_no, uint8_t * p_edch_buffer)
{
    memcpy(p_edch_buffer, m_ecdh.ecdh_key_pair.public, ECS_ECDH_KEY_SIZE);
}

uint32_t eddystone_security_clock_get(uint8_t slot_no)
{
    return m_security_slot[slot_no].timing.seconds;
}

void eddystone_security_eid_slot_destroy(uint8_t slot_no)
{
    DEBUG_PRINTF(0,"Slot [%d] - Destroying EID state if slot was EID \r\n", slot_no);
    memset(&m_security_slot[slot_no],0,sizeof(eddystone_security_slot_t));
}

ret_code_t eddystone_security_ecdh_pair_preserve( void )
{
    return eddystone_flash_access_ecdh_key_pair(m_ecdh.ecdh_key_pair.private,
                                                m_ecdh.ecdh_key_pair.public,
                                                EDDYSTONE_FLASH_ACCESS_WRITE);
}

void eddystone_security_eid_config_get( uint8_t slot_no, eddystone_eid_config_t * p_config )
{
    p_config->frame_type = EDDYSTONE_FRAME_TYPE_EID;
    p_config->k_scaler = m_security_slot[slot_no].timing.k_scaler;
    p_config->seconds = m_security_slot[slot_no].timing.seconds;
    memcpy(p_config->ik, m_security_slot[slot_no].aes_ecb_ik.key, ECS_AES_KEY_SIZE);
}

uint8_t eddystone_security_scaler_get(uint8_t slot_no)
{
    return m_security_slot[slot_no].timing.k_scaler;
}

void eddystone_security_eid_get(uint8_t slot_no, uint8_t * p_eid_buffer)
{
    memcpy(p_eid_buffer, m_security_slot[slot_no].eid, EDDYSTONE_EID_ID_LENGTH);
}

void eddystone_security_encrypted_eid_id_key_get(uint8_t slot_no, uint8_t * p_key_buffer)
{
    memcpy(m_aes_ecb_lk.cleartext, m_security_slot[slot_no].aes_ecb_ik.key,ECS_AES_KEY_SIZE);
    eddystone_security_ecb_block_encrypt(&m_aes_ecb_lk);
    memcpy(p_key_buffer,m_aes_ecb_lk.ciphertext, ECS_AES_KEY_SIZE);
}

void eddystone_security_plain_eid_id_key_get(uint8_t slot_no, uint8_t * p_key_buffer)
{
    memcpy(p_key_buffer, m_security_slot[slot_no].aes_ecb_ik.key, ECS_AES_KEY_SIZE);
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

    memcpy(key, &m_security_slot[ik_slot_no].aes_ecb_ik.key, EIK_SIZE);

    uint8_t nonce[NONCE_SIZE]     = {0};                            // Nonce. This must not repeat for a given key.
    size_t nnonce                       = NONCE_SIZE;               // Length of nonce.First 4 bytes are beacon time base with k-bits cleared
                                                                    // Last two bits are randomly generated

    //Take the current timestamp and clear the lowest K bits, use it as nonce
    uint32_t k_bits_cleared_time = (m_security_slot[ik_slot_no].timing.seconds
                                    >> m_security_slot[ik_slot_no].timing.k_scaler)
                                    << m_security_slot[ik_slot_no].timing.k_scaler;


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
    PRINT_ARRAY((uint8_t *)plain, TLM_DATA_SIZE);
    SEGGER_RTT_printf(0, "NONCE/SALT: ");
    PRINT_ARRAY((uint8_t *)nonce, NONCE_SIZE);
    SEGGER_RTT_printf(0, "KEY/EIK: ");
    PRINT_ARRAY((uint8_t *)key, EIK_SIZE);
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
      PRINT_ARRAY(cipher, TLM_DATA_SIZE);
      SEGGER_RTT_printf(0, "TAG: ");
      PRINT_ARRAY(tag, sizeof(tag));

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
      PRINT_ARRAY(decrypted_tlm, TLM_DATA_SIZE);
      SEGGER_RTT_printf(0, "TAG: ");
      PRINT_ARRAY(tag, sizeof(tag));
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
