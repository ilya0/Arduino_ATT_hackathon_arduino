/*
 * Copyright (c) 2015, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/** ============================================================================
 *  @file       RF.h
 *
 *  @brief      RF driver for CC13xx family
 *
 * # Driver include #
 *  The RF header file should be included in an application as follows:
 *  @code
 *  #include <ti/drivers/rf/RF.h>
 *  @endcode
 *
 * # Overview #
 * The general RF API should used in application code, i.e. RF_open()
 * is used instead of RF_open(). The board file will define the device
 * specific config, and casting in the general API will ensure that the correct
 * device specific functions are called.
 * This is also reflected in the example code in [Use Cases](@ref USE_CASES).
 *
 * # General Behavior #
 * Before using the RF driver:
 *   - The RF driver is initialized by calling RF_open().
 *
 * After RF operation has ended:
 *   - Release system dependencies for RF by calling RF_close().
 *
 * # Error handling #
 *
 * # Power Management #
 * The TI-RTOS power management framework will try to put the device into the most
 * power efficient mode whenever possible. Please see the technical reference
 * manual for further details on each power mode.
 *
 * The RF driver sets power constraints during operation to keep the device out
 * of standby. When the operation has finished, power constraints are released.
 *
 * # Supported Functions #
 * | Generic API function | Description                                       |
 * |----------------------|---------------------------------------------------|
 * | RF_open()            | Open client connection to RF driver               |
 * | RF_getCurrTime()     | Return current radio timer value                  |
 * | RF_postCmd()         | Post an RF operation (chain) to the command queue |
 * | RF_waitCmd()         | Wait for posted command to complete               |
 * | RF_runCmd()          | Runs synchronously a (chain of) RF operation(s)   |
 *
 *  @note All calls should go through the generic API
 *
 *  # Not Supported Functionality #
 *  The RF driver currently does not support:
 *  - RF_abortCmd()
 *  - RF_yield()
 *  - RF_control()
 *  - RF_close()
 *
 * # Use Cases \anchor USE_CASES #
 *
 *  # Instrumentation #
 *  The RF driver interface produces log statements if instrumentation is
 *  enabled.
 *
 *  Diagnostics Mask | Log details                      |
 *  ---------------- | -------------------------------- |
 *  Diags_USER1      | basic RF operations performed    |
 *  Diags_USER2      | detailed RF operations performed |
 *
 *  ============================================================================
 */
#ifndef ti_drivers_rf__include
#define ti_drivers_rf__include

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <driverlib/rf_common_cmd.h>
#include <driverlib/rf_prop_cmd.h>

#define RF_NO_TIMEOUT 0xFFFFFFFF

/// @brief Alias for the data type of the header common to all radio operations
typedef rfc_radioOp_t RF_Op;

/// Struct defining operating mode of RF driver
typedef struct {
    void (*cpePatchFxn)(void);
    void (*mcePatchFxn)(void);
    void (*rfePatchFxn)(void);
    bool (*isOpValidFxn)(RF_Op* pOp);
} RF_Mode;

/// Priority of RF commands
typedef enum {
    RF_PriorityHighest = 0, ///< Highest, use sparingly
    RF_PriorityHigh    = 1, ///< High, time-critical commands in synchronous protocols
    RF_PriorityNormal  = 2, ///< Normal, usually best choice
    RF_PriorityLow     = 3  ///< Low, use for infinite or background commands
} RF_Priority;


/// RF Events reported to callback functions or from RF_runCmd()
typedef enum {
    // TODO: This needs to be better defined
    RF_EventCmdDone      = 0,      ///< Last radio operation in command chain finished (non-maskeable)
    RF_EventRxEntryDone = (1<<0),  ///< Last RX operation data received

    RF_EventCmdError     = (1<<5), ///< Some error reported during command/operation queueing/parsing
    RF_EventCmdAborted   = (1<<6), ///< Radio command (chain) aborted while in progress
    RF_EventCmdCancelled = (1<<7), ///< Radio command (chain) cancelled before dispatch
} RF_Event;


/// Event mask type (construct mask with combinations of #RF_Event)
typedef uint8_t RF_EventMask;


/// Union of the different flavors of RADIO_SETUP commands
typedef union {
    rfc_command_t               naked;      // Can be used simply to get RF operation ID
    rfc_CMD_RADIO_SETUP_t       common;     // TODO: Does this cover BLE/IEEE?
    rfc_CMD_PROP_RADIO_SETUP_t  prop;
} RF_RadioSetup;


/// @brief A command handle that is returned from RF_postCmd()
/// Used by RF_waitCmd() and RF_abortCmd(). A negative value indicates an error
typedef int16_t RF_CmdHandle;


/** @brief RF parameter struct
 *  RF parameters are used with the RF_open() call.
 */
typedef struct {
    uint32_t nInactivityTimeout;   ///< Inactivity timeout in us
} RF_Params;


/** @brief Struct used to store RF client state and configuration
 *  Pointer to a RF_Object is used as handles (#RF_Handle) in interactions with
 *  the RF driver
 *  @note Must reside in persistent memory
 *  @note Except configuration fields before call to RF_open(), fields must never
 *        be modified directly
 */
typedef struct {
    /// Configuration
    struct {
        uint32_t            nInactivityTimeout;   ///< Inactivity timeout in us
        RF_Mode*            pRfMode;              ///< Mode of operation
        RF_RadioSetup*      pOpSetup;             ///< Radio setup radio operation, only ram right now.
    } config;
    /// State & variables
    struct {
        struct {
            rfc_CMD_FS_t        cmdFs;            ///< FS command encapsulating FS state
            union {
                struct {
                    uint8_t     foo;
                } prop;                           ///< Proprietary mode state
                struct {
                    uint8_t     foo;
                } ble;                            ///< BLE mode state
                struct {
                    uint8_t     foo;
                } ieee;                           ///< IEEE mode state
            };
        } mode_state;                                ///< (Mode-specific) state structure
        Semaphore_Struct        semSync;            ///< Semaphore used by runCmd() and waitCmd()
        RF_Event volatile       eventSync;          ///< Event mask/value used by runCmd() and waitCmd()
        Clock_Struct            clkInactivity;      ///< Clock used for inactivity timeouts
        RF_CmdHandle volatile   chLastPosted;       ///< Command handle of most recently posted command
        bool                    bYielded;           ///< Client has indicated that there are no more commands
    } state;
} RF_Object;


/// @brief A handle that is returned from a RF_open() call
/// Used for further RF client interaction with the RF driver
typedef RF_Object* RF_Handle;


/** @brief RF callback function pointer type
 *  RF callbacks can occur at the completion of posted RF operation (chain). The
 *  callback is called from SWI context and provides the relevant #RF_Handle,
 *  pointer to the relevant radio operation as well as an #RF_Event that indicates
 *  what has occurred.
 */
typedef void (*RF_Callback)(RF_Handle h, RF_CmdHandle ch, RF_Event e);


/** @brief  Open client connection to RF driver
 *
 *  Allows a RF client (high-level driver or application) to request access to
 *  RF hardware.
 *
 *  @note Can only be called from task context
 *
 *  @param handle   Pointer to a RF_Object that will hold the state for this
 *                  RF client. The object must be in persistent and writeable
 *                  memory
 *  @param params   Pointer to an RF_Params object that is initialized with desired RF
 *                  parameters.
 *  @return         A handle for further RF driver calls or NULL if a client connection
 *                  was not possible or the supplied parameters invalid
 */
extern RF_Handle RF_open(RF_Object *pObj, RF_Params *params);


/** @brief  Close client connection to RF driver
 *
 *  Allows a RF client (high-level driver or application) to close its connection
 *  to the RF driver. Doing so will abort any running operations and cancel any
 *  pending operations.
 *
 *  @note Can only be called from task context
 *
 *  @param h  Handle previously returned by RF_open()
 */
extern void RF_close(RF_Handle h);



/** @brief  Return current radio timer value
 *
 *  If the radio is powered returns the current radio timer value, if not returns
 *  a conservative estimate of the current radio timer value
 *
 *  @note Can be called from any context
 *
 *  @return     Current radio timer value
 */
extern uint32_t RF_getCurrentTime(void);



/** @brief  Post an RF operation (chain) to the command queue
 *  Post an #RF_Op to the RF command queue of the client with handle h. The
 *  command can be the first in a chain of RF operations or a standalone RF operation.
 *  If a chain of operations are posted they are treated atomically, i.e. either all
 *  or none of the chained operations are run.
 *  All operations must be posted in strictly increasing chronological order. Function returns
 *  immediately.
 *
 *  Limitations apply to the operations posted:
 *  - The operation must be in the set supported in the chosen radio mode when
 *    RF_open() was called
 *  - Only a subset of radio operations are supported
 *    - TODO
 *  - Only some of the trigger modes are supported
 *    - TODO
 *  - Only some of the conditional execution modes are supported
 *    - TODO
 *
 *  @note Can be called from task or SWI context
 *
 *  @param h     Handle previously returned by RF_open()
 *  @param pOp   Pointer to the #RF_Op. Must normally be in persistent and writeable memory
 *  @param ePri  Priority of this RF command (used for arbitration in multi-client systems)
 *  @param pCallback   Callback function called upon command completion (and some other events).
 *               If RF_postCmd() fails no callback is made
 *  @return      A handle to the RF command. Negative value indicates an error
 */
extern RF_CmdHandle RF_postCmd(RF_Handle h, RF_Op* pOp, RF_Priority ePri, RF_Callback pCb);



/** @brief  Wait for posted command to complete
 *  Wait until completion of RF command identified by handle ch for client identified
 *  by handle h to complete. Some RF operations (or chains of operations) post
 *  additional events during execution which, if enabled in event mask bmEvent,
 *  will make RF_waitCmd() return early. In this case, multiple calls to RF_waitCmd()
 *  for a single command can be made.
 *  If RF_waitCmd() is called for a command that registered a callback function it
 *  will take precedence and the callback function will never be called.
 *
 *  @note Can only be called from task context
 *  @note Only one call to RF_waitCmd() or RF_runCmd() can be made at a time for
 *        each client
 *  @note Returns immediately if command has already run or has been aborted/cancelled
 *
 *  @param h       Handle previously returned by RF_open()
 *  @param ch      Command handle previously returned by RF_postCmd().
 *  @param bmEvent Bitmask of events that make RF_waitCmd() return. The command done
 *                 event can not be masked away.
 *  @return        The relevant RF_Event (including RF_Event_Op_Done) for the command
 *                 or last command in a chain
 */
extern RF_Event RF_waitCmd(RF_Handle h, RF_CmdHandle ch, RF_EventMask bmEvent);



/** @brief  Runs synchronously a (chain of) RF operation(s)
 *  Allows a (chain of) operation(s) to be posted to the command queue and then waits
 *  for it to complete.
 *
 *  @note Can only be called from task context
 *  @note Only one call to RF_waitCmd() or RF_runCmd() can be made at a time for
 *        each client
 *
 *  @param h     Handle previously returned by RF_open()
 *  @param pOp   Pointer to the #RF_Op. Must normally be in persistent and writeable memory
 *  @param ePri  Priority of this RF command (used for arbitration in multi-client systems)
 *  @return      The relevant RF_Event (including RF_Event_Op_Done) for the command
 *               or last command in a chain
 */
extern RF_Event RF_runCmd(RF_Handle h, RF_Op* pOp, RF_Priority ePri);



/** @brief  Abort/cancel command and any subsequent commands in command queue
 *  If command is running, aborts it and then cancels all later commands in queue.
 *  If command has not yet run, cancels it and all later commands in queue.
 *  If command has already run or been aborted/cancelled, has no effect.
 *  The callbacks for the aborted/cancelled commands are called in chronological
 *  order.
 *
 *  @note Can be called from task context or SWI context
 *
 *  @param h       Handle previously returned by RF_open()
 *  @param ch      Command handle previously returned by RF_postCmd().
 */
extern void RF_abortCmd(RF_Handle h, RF_CmdHandle ch);



/** @brief  Signal that radio client is not going to issue more commands in a while
 *  Hint to RF driver that, irrespective of inactivity timeout, no new further
 *  commands will be issued for a while and thus the radio can be powered down at
 *  the earliest convenience.
 *
 *  @note Can be called from task context or SWI context
 *
 *  @param h       Handle previously returned by RF_open()
 */
extern void RF_yield(RF_Handle h);



/*!
 *  @brief  Function to initialize the RF_Params struct to its defaults
 *
 *  @param  params      An pointer to RF_Params structure for
 *                      initialization
 *
 *  Defaults values are:
 *      nInactivityTimeout = RF_NO_TIMEOUT;
 */
extern void RF_Params_init(RF_Params *params);


// Do not interfere with the app if they include the family Hwi module
#undef ti_sysbios_family_arm_m3_Hwi__nolocalnames

#ifdef __cplusplus
}
#endif

#endif /* ti_drivers_rf__include */
