//--------------------------------------------------------------------------
// Copyright (C) 2016-2016 Cisco and/or its affiliates. All rights reserved.
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License Version 2 as published
// by the Free Software Foundation.  You may not use, modify or distribute
// this program under any other version of the GNU General Public License.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//--------------------------------------------------------------------------

// dce_smb_transaction.cc author Maya Dagon <mdagon@cisco.com>
// based on work by Todd Wease

// Smb transaction commands processing

#include "dce_smb_transaction_utils.h"
#include "dce_smb_utils.h"
#include "dce_smb_module.h"

#include "main/snort_debug.h"
#include "utils/util.h"
#include "detection/detect.h"

#define DCE2_SMB_TRANS__NONE    0x00
#define DCE2_SMB_TRANS__DATA    0x01
#define DCE2_SMB_TRANS__PARAMS  0x02
#define DCE2_SMB_TRANS__BOTH    (DCE2_SMB_TRANS__DATA|DCE2_SMB_TRANS__PARAMS)

enum SmbNtTransactSubcommand
{
    NT_TRANSACT_UNKNOWN_0000            = 0x0000,
    NT_TRANSACT_CREATE                  = 0x0001,
    NT_TRANSACT_IOCTL                   = 0x0002,
    NT_TRANSACT_SET_SECURITY_DESC       = 0x0003,
    NT_TRANSACT_NOTIFY_CHANGE           = 0x0004,
    NT_TRANSACT_RENAME                  = 0x0005,
    NT_TRANSACT_QUERY_SECURITY_DESC     = 0x0006,
    NT_TRANSACT_SUBCOM_MAX              = 0x0007
} SmbNtTransactSubcommand;

/********************************************************************
 * Global variables
 ********************************************************************/
const char* smb_transaction_sub_command_strings[TRANS_SUBCOM_MAX] =
{
    "Unknown",                               // 0x0000
    "TRANS_SET_NMPIPE_STATE",                // 0x0001
    "Unknown",                               // 0x0002
    "Unknown",                               // 0x0003
    "Unknown",                               // 0x0004
    "Unknown",                               // 0x0005
    "Unknown",                               // 0x0006
    "Unknown",                               // 0x0007
    "Unknown",                               // 0x0008
    "Unknown",                               // 0x0009
    "Unknown",                               // 0x000A
    "Unknown",                               // 0x000B
    "Unknown",                               // 0x000C
    "Unknown",                               // 0x000D
    "Unknown",                               // 0x000E
    "Unknown",                               // 0x000F
    "Unknown",                               // 0x0010
    "TRANS_RAW_READ_NMPIPE",                 // 0x0011
    "Unknown",                               // 0x0012
    "Unknown",                               // 0x0013
    "Unknown",                               // 0x0014
    "Unknown",                               // 0x0015
    "Unknown",                               // 0x0016
    "Unknown",                               // 0x0017
    "Unknown",                               // 0x0018
    "Unknown",                               // 0x0019
    "Unknown",                               // 0x001A
    "Unknown",                               // 0x001B
    "Unknown",                               // 0x001C
    "Unknown",                               // 0x001D
    "Unknown",                               // 0x001E
    "Unknown",                               // 0x001F
    "Unknown",                               // 0x0020
    "TRANS_QUERY_NMPIPE_STATE",              // 0x0021
    "TRANS_QUERY_NMPIPE_INFO",               // 0x0022
    "TRANS_PEEK_NMPIPE",                     // 0x0023
    "Unknown",                               // 0x0024
    "Unknown",                               // 0x0025
    "TRANS_TRANSACT_NMPIPE",                 // 0x0026
    "Unknown",                               // 0x0027
    "Unknown",                               // 0x0028
    "Unknown",                               // 0x0029
    "Unknown",                               // 0x002A
    "Unknown",                               // 0x002B
    "Unknown",                               // 0x002C
    "Unknown",                               // 0x002D
    "Unknown",                               // 0x002E
    "Unknown",                               // 0x002F
    "Unknown",                               // 0x0030
    "TRANS_RAW_WRITE_NMPIPE",                // 0x0031
    "Unknown",                               // 0x0032
    "Unknown",                               // 0x0033
    "Unknown",                               // 0x0034
    "Unknown",                               // 0x0035
    "TRANS_READ_NMPIPE",                     // 0x0036
    "TRANS_WRITE_NMPIPE",                    // 0x0037
    "Unknown",                               // 0x0038
    "Unknown",                               // 0x0039
    "Unknown",                               // 0x003A
    "Unknown",                               // 0x003B
    "Unknown",                               // 0x003C
    "Unknown",                               // 0x003D
    "Unknown",                               // 0x003E
    "Unknown",                               // 0x003F
    "Unknown",                               // 0x0040
    "Unknown",                               // 0x0041
    "Unknown",                               // 0x0042
    "Unknown",                               // 0x0043
    "Unknown",                               // 0x0044
    "Unknown",                               // 0x0045
    "Unknown",                               // 0x0046
    "Unknown",                               // 0x0047
    "Unknown",                               // 0x0048
    "Unknown",                               // 0x0049
    "Unknown",                               // 0x004A
    "Unknown",                               // 0x004B
    "Unknown",                               // 0x004C
    "Unknown",                               // 0x004D
    "Unknown",                               // 0x004E
    "Unknown",                               // 0x004F
    "Unknown",                               // 0x0050
    "Unknown",                               // 0x0051
    "Unknown",                               // 0x0052
    "TRANS_WAIT_NMPIPE",                     // 0x0053
    "TRANS_CALL_NMPIPE"                      // 0x0054
};

const char* smb_transaction2_sub_command_strings[TRANS2_SUBCOM_MAX] =
{
    "TRANS2_OPEN2",                          // 0x0000
    "TRANS2_FIND_FIRST2",                    // 0x0001
    "TRANS2_FIND_NEXT2",                     // 0x0002
    "TRANS2_QUERY_FS_INFORMATION",           // 0x0003
    "TRANS2_SET_FS_INFORMATION",             // 0x0004
    "TRANS2_QUERY_PATH_INFORMATION",         // 0x0005
    "TRANS2_SET_PATH_INFORMATION",           // 0x0006
    "TRANS2_QUERY_FILE_INFORMATION",         // 0x0007
    "TRANS2_SET_FILE_INFORMATION",           // 0x0008
    "TRANS2_FSCTL",                          // 0x0009
    "TRANS2_IOCTL2",                         // 0x000A
    "TRANS2_FIND_NOTIFY_FIRST",              // 0x000B
    "TRANS2_FIND_NOTIFY_NEXT",               // 0x000C
    "TRANS2_CREATE_DIRECTORY",               // 0x000D
    "TRANS2_SESSION_SETUP",                  // 0x000E
    "Unknown",                               // 0x000F
    "TRANS2_GET_DFS_REFERRAL",               // 0x0010
    "TRANS2_REPORT_DFS_INCONSISTENCY"        // 0x0011
};

const char* smb_nt_transact_sub_command_strings[NT_TRANSACT_SUBCOM_MAX] =
{
    "Unknown",                               // 0x0000
    "NT_TRANSACT_CREATE",                    // 0x0001
    "NT_TRANSACT_IOCTL",                     // 0x0002
    "NT_TRANSACT_SET_SECURITY_DESC",         // 0x0003
    "NT_TRANSACT_NOTIFY_CHANGE",             // 0x0004
    "NT_TRANSACT_RENAME",                    // 0x0005
    "NT_TRANSACT_QUERY_SECURITY_DESC"        // 0x0006
};

/********************************************************************
 * Private function prototypes
 ********************************************************************/
static DCE2_Ret DCE2_SmbUpdateTransRequest(DCE2_SmbSsnData*, const SmbNtHdr*,
    const DCE2_SmbComInfo*, const uint8_t*, uint32_t);
static DCE2_Ret DCE2_SmbUpdateTransResponse(DCE2_SmbSsnData*, const SmbNtHdr*,
    const DCE2_SmbComInfo*, const uint8_t*, uint32_t);
static DCE2_Ret DCE2_SmbTransactionReq(DCE2_SmbSsnData*,
    DCE2_SmbTransactionTracker*, const uint8_t*, uint32_t,
    const uint8_t*, uint32_t);
static DCE2_Ret DCE2_SmbUpdateTransSecondary(DCE2_SmbSsnData*, const SmbNtHdr*,
    const DCE2_SmbComInfo*, const uint8_t*, uint32_t);
static DCE2_Ret DCE2_SmbNtTransactCreateReq(DCE2_SmbSsnData*,
    const uint8_t*, uint32_t, bool);
static DCE2_Ret DCE2_SmbTrans2Open2Req(DCE2_SmbSsnData*,
    const uint8_t*, uint32_t, bool);
static DCE2_Ret DCE2_SmbTrans2QueryFileInfoReq(DCE2_SmbSsnData*,
    const uint8_t*, uint32_t);
static DCE2_Ret DCE2_SmbTrans2SetFileInfoReq(DCE2_SmbSsnData*,
    const uint8_t*, uint32_t, const uint8_t*, uint32_t);

/*********************************************************************
 * Private functions
 ********************************************************************/
// SMB_COM_TRANSACTION Request
static DCE2_Ret DCE2_SmbTransactionReq(DCE2_SmbSsnData* ssd,
    DCE2_SmbTransactionTracker* ttracker,
    const uint8_t* data_ptr, uint32_t data_len,
    const uint8_t* param_ptr, uint32_t param_len)
{
    switch (ttracker->subcom)
    {
    case TRANS_TRANSACT_NMPIPE:
    case TRANS_WRITE_NMPIPE:
        if (DCE2_SmbProcessRequestData(ssd, 0,
            data_ptr, data_len, 0) != DCE2_RET__SUCCESS)
            return DCE2_RET__ERROR;
        break;

    case TRANS_SET_NMPIPE_STATE:
        // Only two parameters but more seems okay
        if (param_len >= 2)
        {
            if ((alignedNtohs((uint16_t*)param_ptr) & PIPE_STATE_MESSAGE_MODE))
                ttracker->pipe_byte_mode = false;
            else
                ttracker->pipe_byte_mode = true;

            // Won't get a response
            if (DCE2_SsnIsWindowsPolicy(&ssd->sd) && ttracker->one_way)
            {
                DebugFormat(DEBUG_DCE_SMB,
                    "Setting pipe to %s mode\n",
                    ttracker->pipe_byte_mode ? "byte" : "message");

                ssd->cur_rtracker->ftracker->fp_byte_mode = ttracker->pipe_byte_mode;
            }
        }
        break;

    case TRANS_READ_NMPIPE:
        break;

    default:
        return DCE2_RET__IGNORE;
    }

    if (DCE2_SsnIsWindowsPolicy(&ssd->sd) && ttracker->one_way && ttracker->disconnect_tid)
        DCE2_SmbRemoveTid(ssd, ssd->cur_rtracker->tid);

    return DCE2_RET__SUCCESS;
}

// NT_TRANSACT_CREATE
static DCE2_Ret DCE2_SmbNtTransactCreateReq(DCE2_SmbSsnData* ssd,
    const uint8_t* param_ptr, uint32_t param_len, bool unicode)
{
    uint32_t pad = 0;
    uint32_t file_name_length;
    const uint8_t* param_start = param_ptr;

    if (param_len < sizeof(SmbNtTransactCreateReqParams))
        return DCE2_RET__ERROR;

    if (!DCE2_SmbIsTidIPC(ssd, ssd->cur_rtracker->tid))
    {
        uint32_t ext_file_attrs =
            SmbNtTransactCreateReqFileAttrs((SmbNtTransactCreateReqParams*)param_ptr);

        if (SmbEvasiveFileAttrs(ext_file_attrs))
            dce_alert(GID_DCE2, DCE2_SMB_EVASIVE_FILE_ATTRS,
                (dce2CommonStats*)&dce2_smb_stats);

        // If the file is going to be accessed sequentially, track it.
        if (SmbNtTransactCreateReqSequentialOnly((SmbNtTransactCreateReqParams*)param_ptr))
            ssd->cur_rtracker->sequential_only = true;

        ssd->cur_rtracker->file_size =
            SmbNtTransactCreateReqAllocSize((SmbNtTransactCreateReqParams*)param_ptr);
    }

    file_name_length =
        SmbNtTransactCreateReqFileNameLength((SmbNtTransactCreateReqParams*)param_ptr);

    if (file_name_length > DCE2_SMB_MAX_PATH_LEN)
        return DCE2_RET__ERROR;

    DCE2_MOVE(param_ptr, param_len, sizeof(SmbNtTransactCreateReqParams));

    if (unicode)
        pad = (param_ptr - param_start) & 1;

    if (param_len < (pad + file_name_length))
        return DCE2_RET__ERROR;

    DCE2_MOVE(param_ptr, param_len, pad);

    ssd->cur_rtracker->file_name =
        DCE2_SmbGetString(param_ptr, file_name_length, unicode, false);

    return DCE2_RET__SUCCESS;
}

/********************************************************************
 * Function: DCE2_SmbUpdateTransSecondary()
 *
 * Purpose:
 *  Handles common checks and updates of transaction secondary
 *  requests - SMB_COM_TRANSACTION_SECONDARY,
 *  SMB_COM_TRANSACTION2_SECONDARY and
 *  SMB_COM_NT_TRANSACT_SECONDARY
 *
 * Arguments:
 *  DCE2_SmbSsnData *       - pointer to SMB session data
 *  const SmbNtHdr *        - pointer to SMB header
 *  const DCE2_SmbComInfo * - pointer to com info structure
 *  const uint8_t *         - pointer to data
 *  uint32_t                - data length
 *
 * Returns:
 *  DCE2_Ret
 *      DCE2_RET__IGNORE if we don't process the subcommand
 *      DCE2_RET__FULL if the transaction is complete
 *      DCE2_RET__ERROR if an error occurred.
 *      DCE2_RET__SUCCESS if ok (but not complete).
 *
 ********************************************************************/
static DCE2_Ret DCE2_SmbUpdateTransSecondary(DCE2_SmbSsnData* ssd,
    const SmbNtHdr* smb_hdr, const DCE2_SmbComInfo* com_info,
    const uint8_t* nb_ptr, uint32_t nb_len)
{
    uint16_t com_size = DCE2_ComInfoCommandSize(com_info);
    uint16_t byte_count = DCE2_ComInfoByteCount(com_info);
    uint32_t tdcnt, doff, dcnt, ddisp;
    uint32_t tpcnt, poff, pcnt, pdisp;
    DCE2_SmbTransactionTracker* ttracker = &ssd->cur_rtracker->ttracker;
    uint16_t sub_com = ttracker->subcom;
    int data_params = DCE2_SMB_TRANS__NONE;
    uint8_t smb_com = DCE2_ComInfoSmbCom(com_info);

    switch (smb_com)
    {
    case SMB_COM_TRANSACTION_SECONDARY:
        tdcnt = SmbTransactionSecondaryReqTotalDataCnt((SmbTransactionSecondaryReq*)nb_ptr);
        doff = SmbTransactionSecondaryReqDataOff((SmbTransactionSecondaryReq*)nb_ptr);
        dcnt = SmbTransactionSecondaryReqDataCnt((SmbTransactionSecondaryReq*)nb_ptr);
        ddisp = SmbTransactionSecondaryReqDataDisp((SmbTransactionSecondaryReq*)nb_ptr);
        tpcnt = SmbTransactionSecondaryReqTotalParamCnt((SmbTransactionSecondaryReq*)nb_ptr);
        poff = SmbTransactionSecondaryReqParamOff((SmbTransactionSecondaryReq*)nb_ptr);
        pcnt = SmbTransactionSecondaryReqParamCnt((SmbTransactionSecondaryReq*)nb_ptr);
        pdisp = SmbTransactionSecondaryReqParamDisp((SmbTransactionSecondaryReq*)nb_ptr);

        switch (sub_com)
        {
        case TRANS_TRANSACT_NMPIPE:
        case TRANS_WRITE_NMPIPE:
            data_params = DCE2_SMB_TRANS__DATA;
            break;
        case TRANS_SET_NMPIPE_STATE:
            data_params = DCE2_SMB_TRANS__PARAMS;
            break;
        default:
            return DCE2_RET__IGNORE;
        }
        break;

    case SMB_COM_TRANSACTION2_SECONDARY:
        tdcnt = SmbTransaction2SecondaryReqTotalDataCnt((SmbTransaction2SecondaryReq*)nb_ptr);
        doff = SmbTransaction2SecondaryReqDataOff((SmbTransaction2SecondaryReq*)nb_ptr);
        dcnt = SmbTransaction2SecondaryReqDataCnt((SmbTransaction2SecondaryReq*)nb_ptr);
        ddisp = SmbTransaction2SecondaryReqDataDisp((SmbTransaction2SecondaryReq*)nb_ptr);
        tpcnt = SmbTransaction2SecondaryReqTotalParamCnt((SmbTransaction2SecondaryReq*)nb_ptr);
        poff = SmbTransaction2SecondaryReqParamOff((SmbTransaction2SecondaryReq*)nb_ptr);
        pcnt = SmbTransaction2SecondaryReqParamCnt((SmbTransaction2SecondaryReq*)nb_ptr);
        pdisp = SmbTransaction2SecondaryReqParamDisp((SmbTransaction2SecondaryReq*)nb_ptr);

        switch (sub_com)
        {
        case TRANS2_OPEN2:
        case TRANS2_QUERY_FILE_INFORMATION:
            data_params = DCE2_SMB_TRANS__PARAMS;
            break;
        case TRANS2_SET_FILE_INFORMATION:
            data_params = DCE2_SMB_TRANS__BOTH;
            break;
        default:
            return DCE2_RET__IGNORE;
        }
        break;

    case SMB_COM_NT_TRANSACT_SECONDARY:
        tdcnt = SmbNtTransactSecondaryReqTotalDataCnt((SmbNtTransactSecondaryReq*)nb_ptr);
        doff = SmbNtTransactSecondaryReqDataOff((SmbNtTransactSecondaryReq*)nb_ptr);
        dcnt = SmbNtTransactSecondaryReqDataCnt((SmbNtTransactSecondaryReq*)nb_ptr);
        ddisp = SmbNtTransactSecondaryReqDataDisp((SmbNtTransactSecondaryReq*)nb_ptr);
        tpcnt = SmbNtTransactSecondaryReqTotalParamCnt((SmbNtTransactSecondaryReq*)nb_ptr);
        poff = SmbNtTransactSecondaryReqParamOff((SmbNtTransactSecondaryReq*)nb_ptr);
        pcnt = SmbNtTransactSecondaryReqParamCnt((SmbNtTransactSecondaryReq*)nb_ptr);
        pdisp = SmbNtTransactSecondaryReqParamDisp((SmbNtTransactSecondaryReq*)nb_ptr);

        switch (sub_com)
        {
        case NT_TRANSACT_CREATE:
            data_params = DCE2_SMB_TRANS__PARAMS;
            break;
        default:
            return DCE2_RET__IGNORE;
        }
        break;

    default:
        return DCE2_RET__ERROR;
    }

    if (DCE2_SsnIsSambaPolicy(&ssd->sd))
    {
        // If the total count decreases, Samba will reset this to the new
        // total count.
        if (tdcnt < ttracker->tdcnt)
            ttracker->tdcnt = tdcnt;
        if (tpcnt < ttracker->tpcnt)
            ttracker->tpcnt = tpcnt;
    }
    else
    {
        // Windows always uses the total data count from the first transaction.
        tdcnt = (uint16_t)ttracker->tdcnt;
        tpcnt = (uint16_t)ttracker->tpcnt;
    }

    DCE2_MOVE(nb_ptr, nb_len, com_size);

    if (DCE2_SmbValidateTransactionFields((uint8_t*)smb_hdr, nb_ptr, nb_len,
        byte_count, tdcnt, tpcnt, dcnt, doff, ddisp, pcnt, poff, pdisp) != DCE2_RET__SUCCESS)
        return DCE2_RET__ERROR;

    if (DCE2_SmbValidateTransactionSent(ttracker->dsent, dcnt, ttracker->tdcnt,
        ttracker->psent, pcnt, ttracker->tpcnt) != DCE2_RET__SUCCESS)
        return DCE2_RET__IGNORE;

    ttracker->dsent += dcnt;
    ttracker->psent += pcnt;

    DebugFormat(DEBUG_DCE_SMB, "Data displacement: %u, "
        "Data count: %u, Total data count: %u\n"
        "Parameter displacement: %u, "
        "Parameter count: %u, Total parameter count: %u\n",
        ddisp, dcnt, tdcnt, pdisp, pcnt, tpcnt);

    if (data_params & DCE2_SMB_TRANS__DATA)
    {
        DCE2_MOVE(nb_ptr, nb_len, ((uint8_t*)smb_hdr + doff) - nb_ptr);

        if ((dcnt != 0)
            && (DCE2_SmbBufferTransactionData(ttracker, nb_ptr, dcnt, ddisp)
            != DCE2_RET__SUCCESS))
        {
            return DCE2_RET__ERROR;
        }
    }

    if (data_params & DCE2_SMB_TRANS__PARAMS)
    {
        DCE2_MOVE(nb_ptr, nb_len, ((uint8_t*)smb_hdr + poff) - nb_ptr);

        if ((pcnt != 0)
            && (DCE2_SmbBufferTransactionParameters(ttracker, nb_ptr, pcnt, pdisp)
            != DCE2_RET__SUCCESS))
        {
            return DCE2_RET__ERROR;
        }
    }

    if ((ttracker->dsent == ttracker->tdcnt)
        && (ttracker->psent == ttracker->tpcnt))
    {
        return DCE2_RET__FULL;
    }

    return DCE2_RET__SUCCESS;
}

/********************************************************************
 * Function: DCE2_SmbUpdateTransRequest()
 *
 * Purpose:
 *  Handles common checks and updates of transaction requests -
 *  SMB_COM_TRANSACTION, SMB_COM_TRANSACTION2 and SMB_COM_NT_TRANSACT
 *
 * Arguments:
 *  DCE2_SmbSsnData *       - pointer to SMB session data
 *  const SmbNtHdr *        - pointer to SMB header
 *  const DCE2_SmbComInfo * - pointer to com info structure
 *  const uint8_t *         - pointer to data
 *  uint32_t                - data length
 *
 * Returns:
 *  DCE2_Ret
 *      DCE2_RET__IGNORE if we don't process the subcommand
 *      DCE2_RET__FULL if the transaction is complete
 *      DCE2_RET__ERROR if an error occurred.
 *      DCE2_RET__SUCCESS if ok (but not complete).
 *
 ********************************************************************/
static DCE2_Ret DCE2_SmbUpdateTransRequest(DCE2_SmbSsnData* ssd,
    const SmbNtHdr* smb_hdr, const DCE2_SmbComInfo* com_info,
    const uint8_t* nb_ptr, uint32_t nb_len)
{
    uint32_t tpcnt, pcnt, poff;
    uint32_t tdcnt, dcnt, doff;
    uint16_t com_size = DCE2_ComInfoCommandSize(com_info);
    uint16_t byte_count = DCE2_ComInfoByteCount(com_info);
    uint16_t fid;
    uint8_t setup_count;
    DCE2_SmbTransactionTracker* ttracker = &ssd->cur_rtracker->ttracker;
    uint16_t sub_com =0;
    int data_params = DCE2_SMB_TRANS__NONE;
    uint8_t smb_com = DCE2_ComInfoSmbCom(com_info);

    switch (smb_com)
    {
    case SMB_COM_TRANSACTION:
        sub_com = SmbTransactionReqSubCom((SmbTransactionReq*)nb_ptr);
        fid = SmbTransactionReqFid((SmbTransactionReq*)nb_ptr);
        setup_count = SmbTransactionReqSetupCnt((SmbTransactionReq*)nb_ptr);
        tdcnt = SmbTransactionReqTotalDataCnt((SmbTransactionReq*)nb_ptr);
        doff = SmbTransactionReqDataOff((SmbTransactionReq*)nb_ptr);
        dcnt = SmbTransactionReqDataCnt((SmbTransactionReq*)nb_ptr);
        tpcnt = SmbTransactionReqTotalParamCnt((SmbTransactionReq*)nb_ptr);
        pcnt = SmbTransactionReqParamCnt((SmbTransactionReq*)nb_ptr);
        poff = SmbTransactionReqParamOff((SmbTransactionReq*)nb_ptr);

        DebugFormat(DEBUG_DCE_SMB,
            "Transaction subcommand: %s (0x%04X)\n",
            (sub_com < TRANS_SUBCOM_MAX)
            ? smb_transaction_sub_command_strings[sub_com]
            : "Unknown", sub_com);

        ssd->cur_rtracker->ftracker = DCE2_SmbGetFileTracker(ssd, fid);
        if (ssd->cur_rtracker->ftracker == nullptr)
            return DCE2_RET__IGNORE;

        switch (sub_com)
        {
        case TRANS_TRANSACT_NMPIPE:
            if (DCE2_SsnIsWindowsPolicy(&ssd->sd)
                && ssd->cur_rtracker->ftracker->fp_byte_mode)
            {
                DebugMessage(DEBUG_DCE_SMB, "Pipe is in byte "
                    "mode - TRANS_TRANSACT_NMPIPE won't work\n");
                return DCE2_RET__ERROR;
            }
            data_params = DCE2_SMB_TRANS__DATA;
            break;

        case TRANS_READ_NMPIPE:
            dce_alert(GID_DCE2, DCE2_SMB_UNUSUAL_COMMAND_USED, (dce2CommonStats*)&dce2_smb_stats);
            break;

        case TRANS_SET_NMPIPE_STATE:
            data_params = DCE2_SMB_TRANS__PARAMS;
            break;

        case TRANS_WRITE_NMPIPE:
            dce_alert(GID_DCE2, DCE2_SMB_UNUSUAL_COMMAND_USED, (dce2CommonStats*)&dce2_smb_stats);
            data_params = DCE2_SMB_TRANS__DATA;
            break;

        // Not implemented according to MS-CIFS
        case TRANS_RAW_READ_NMPIPE:

        // Can only write 2 NULL bytes and subsequent writes return pipe disconnected
        case TRANS_RAW_WRITE_NMPIPE:

        // Can at most do a DCE/RPC bind
        case TRANS_CALL_NMPIPE:
            dce_alert(GID_DCE2, DCE2_SMB_DEPR_COMMAND_USED, (dce2CommonStats*)&dce2_smb_stats);
        // Aren't looking at these or the three above
        case TRANS_QUERY_NMPIPE_STATE:
        case TRANS_QUERY_NMPIPE_INFO:
        case TRANS_PEEK_NMPIPE:
        case TRANS_WAIT_NMPIPE:
        default:
            // Don't want to track the response
            return DCE2_RET__IGNORE;
        }

        // Servers return error if incorrect setup count
        if (setup_count != 2)
        {
            dce_alert(GID_DCE2, DCE2_SMB_INVALID_SETUP_COUNT, (dce2CommonStats*)&dce2_smb_stats);
            return DCE2_RET__ERROR;
        }

        DCE2_MOVE(nb_ptr, nb_len, com_size);

        // Samba validates the Name which should be \PIPE\ and errors
        // if not.  Windows doesn't care.
        // And Samba uses the ByteCount to validate
        if (DCE2_SsnIsSambaPolicy(&ssd->sd)
            && (DCE2_SmbTransactionGetName(nb_ptr, nb_len,
            byte_count, SmbUnicode(smb_hdr)) != DCE2_RET__SUCCESS))
        {
            DebugMessage(DEBUG_DCE_SMB, "Failed to validate "
                "pipe name for Samba.\n");
            return DCE2_RET__ERROR;
        }
        break;

    case SMB_COM_TRANSACTION2:
        sub_com = SmbTransaction2ReqSubCom((SmbTransaction2Req*)nb_ptr);
        setup_count = SmbTransaction2ReqSetupCnt((SmbTransaction2Req*)nb_ptr);
        tdcnt = SmbTransaction2ReqTotalDataCnt((SmbTransaction2Req*)nb_ptr);
        doff = SmbTransaction2ReqDataOff((SmbTransaction2Req*)nb_ptr);
        dcnt = SmbTransaction2ReqDataCnt((SmbTransaction2Req*)nb_ptr);
        tpcnt = SmbTransaction2ReqTotalParamCnt((SmbTransaction2Req*)nb_ptr);
        pcnt = SmbTransaction2ReqParamCnt((SmbTransaction2Req*)nb_ptr);
        poff = SmbTransaction2ReqParamOff((SmbTransaction2Req*)nb_ptr);

        DebugFormat(DEBUG_DCE_SMB,
            "Transaction2 subcommand: %s (0x%04X)\n",
            (sub_com < TRANS2_SUBCOM_MAX)
            ? smb_transaction2_sub_command_strings[sub_com]
            : "Unknown", sub_com);

        switch (sub_com)
        {
        case TRANS2_OPEN2:
            dce_alert(GID_DCE2, DCE2_SMB_UNUSUAL_COMMAND_USED, (dce2CommonStats*)&dce2_smb_stats);
            data_params = DCE2_SMB_TRANS__PARAMS;
            break;
        case TRANS2_QUERY_FILE_INFORMATION:
            data_params = DCE2_SMB_TRANS__PARAMS;
            break;
        case TRANS2_SET_FILE_INFORMATION:
            data_params = DCE2_SMB_TRANS__BOTH;
            break;
        case TRANS2_FIND_FIRST2:
        case TRANS2_FIND_NEXT2:
        case TRANS2_QUERY_FS_INFORMATION:
        case TRANS2_SET_FS_INFORMATION:
        case TRANS2_QUERY_PATH_INFORMATION:
        case TRANS2_SET_PATH_INFORMATION:
        case TRANS2_FSCTL:
        case TRANS2_IOCTL2:
        case TRANS2_FIND_NOTIFY_FIRST:
        case TRANS2_FIND_NOTIFY_NEXT:
        case TRANS2_CREATE_DIRECTORY:
        case TRANS2_SESSION_SETUP:
        case TRANS2_GET_DFS_REFERRAL:
        case TRANS2_REPORT_DFS_INCONSISTENCY:
        default:
            // Don't want to process this transaction any more
            return DCE2_RET__IGNORE;
        }

        if (setup_count != 1)
        {
            dce_alert(GID_DCE2, DCE2_SMB_INVALID_SETUP_COUNT, (dce2CommonStats*)&dce2_smb_stats);
            return DCE2_RET__ERROR;
        }

        DCE2_MOVE(nb_ptr, nb_len, com_size);

        break;

    case SMB_COM_NT_TRANSACT:
        sub_com = SmbNtTransactReqSubCom((SmbNtTransactReq*)nb_ptr);
        setup_count = SmbNtTransactReqSetupCnt((SmbNtTransactReq*)nb_ptr);
        tdcnt = SmbNtTransactReqTotalDataCnt((SmbNtTransactReq*)nb_ptr);
        doff = SmbNtTransactReqDataOff((SmbNtTransactReq*)nb_ptr);
        dcnt = SmbNtTransactReqDataCnt((SmbNtTransactReq*)nb_ptr);
        tpcnt = SmbNtTransactReqTotalParamCnt((SmbNtTransactReq*)nb_ptr);
        pcnt = SmbNtTransactReqParamCnt((SmbNtTransactReq*)nb_ptr);
        poff = SmbNtTransactReqParamOff((SmbNtTransactReq*)nb_ptr);

        DebugFormat(DEBUG_DCE_SMB,
            "Nt Transact subcommand: %s (0x%04X)\n",
            (sub_com < NT_TRANSACT_SUBCOM_MAX)
            ? smb_nt_transact_sub_command_strings[sub_com]
            : "Unknown", sub_com);

        switch (sub_com)
        {
        case NT_TRANSACT_CREATE:
            dce_alert(GID_DCE2, DCE2_SMB_UNUSUAL_COMMAND_USED, (dce2CommonStats*)&dce2_smb_stats);
            if (setup_count != 0)
            {
                dce_alert(GID_DCE2, DCE2_SMB_INVALID_SETUP_COUNT,
                    (dce2CommonStats*)&dce2_smb_stats);
                return DCE2_RET__ERROR;
            }
            data_params = DCE2_SMB_TRANS__PARAMS;
            break;
        case NT_TRANSACT_IOCTL:
        case NT_TRANSACT_SET_SECURITY_DESC:
        case NT_TRANSACT_NOTIFY_CHANGE:
        case NT_TRANSACT_RENAME:
        case NT_TRANSACT_QUERY_SECURITY_DESC:
        default:
            // Don't want to process this transaction any more
            return DCE2_RET__IGNORE;
        }

        DCE2_MOVE(nb_ptr, nb_len, com_size);

        break;

    default:
        return DCE2_RET__ERROR;
    }

    if (DCE2_SmbValidateTransactionFields((uint8_t*)smb_hdr, nb_ptr, nb_len,
        byte_count, tdcnt, tpcnt, dcnt, doff, 0, pcnt, poff, 0) != DCE2_RET__SUCCESS)
        return DCE2_RET__ERROR;

    ttracker->smb_type = SMB_TYPE__REQUEST;
    ttracker->subcom = (uint8_t)sub_com;
    ttracker->tdcnt = tdcnt;
    ttracker->dsent = dcnt;
    ttracker->tpcnt = tpcnt;
    ttracker->psent = pcnt;

    DebugFormat(DEBUG_DCE_SMB, "Data count: %u, "
        "Total data count: %u, Param count: %u, "
        "Total param count: %u\n", dcnt, tdcnt, pcnt, tpcnt);

    // Testing shows that Transacts aren't processed until
    // all of the data and parameters are received, so overlapping
    // writes to the same FID can occur as long as the pid/mid are
    // distinct (and that depends on policy).  So we need to buffer
    // data up for each incomplete Transact so data doesn't get mangled
    // together with multiple ones intermixing at the same time.

    if (data_params & DCE2_SMB_TRANS__DATA)
    {
        if (tdcnt == 0)
            dce_alert(GID_DCE2, DCE2_SMB_DCNT_ZERO, (dce2CommonStats*)&dce2_smb_stats);

        DCE2_MOVE(nb_ptr, nb_len, ((uint8_t*)smb_hdr + doff) - nb_ptr);

        // If all of the data and parameters weren't sent, buffer what was sent
        if (((dcnt != tdcnt) || (pcnt != tpcnt)) && (dcnt != 0)
            && (DCE2_SmbBufferTransactionData(ttracker,
            nb_ptr, dcnt, 0) != DCE2_RET__SUCCESS))
        {
            return DCE2_RET__ERROR;
        }
    }

    if (data_params & DCE2_SMB_TRANS__PARAMS)
    {
        if (tpcnt == 0)
            dce_alert(GID_DCE2, DCE2_SMB_DCNT_ZERO, (dce2CommonStats*)&dce2_smb_stats);

        DCE2_MOVE(nb_ptr, nb_len, ((uint8_t*)smb_hdr + poff) - nb_ptr);

        // If all of the data and parameters weren't sent, buffer what was sent
        if (((pcnt != tpcnt) || (dcnt != tdcnt)) && (pcnt != 0)
            && (DCE2_SmbBufferTransactionParameters(ttracker,
            nb_ptr, pcnt, 0) != DCE2_RET__SUCCESS))
        {
            return DCE2_RET__ERROR;
        }
    }

    if ((dcnt == tdcnt) && (pcnt == tpcnt))
        return DCE2_RET__FULL;

    return DCE2_RET__SUCCESS;
}

/********************************************************************
 * Function: DCE2_SmbUpdateTransResponse()
 *
 * Purpose:
 *  Handles common checks and updates of transaction responses -
 *  SMB_COM_TRANSACTION, SMB_COM_TRANSACTION2 and SMB_COM_NT_TRANSACT
 *
 * Arguments:
 *  DCE2_SmbSsnData *       - pointer to SMB session data
 *  const SmbNtHdr *        - pointer to SMB header
 *  const DCE2_SmbComInfo * - pointer to com info structure
 *  const uint8_t *         - pointer to data
 *  uint32_t                - data length
 *
 * Returns:
 *  DCE2_Ret
 *      DCE2_RET__FULL if the transaction is complete
 *      DCE2_RET__ERROR if an error occurred.
 *      DCE2_RET__SUCCESS if ok (but not complete).
 *
 ********************************************************************/
static DCE2_Ret DCE2_SmbUpdateTransResponse(DCE2_SmbSsnData* ssd,
    const SmbNtHdr* smb_hdr, const DCE2_SmbComInfo* com_info,
    const uint8_t* nb_ptr, uint32_t nb_len)
{
    uint32_t tpcnt, pcnt, poff, pdisp;
    uint32_t tdcnt, dcnt, doff, ddisp;
    DCE2_SmbTransactionTracker* ttracker = &ssd->cur_rtracker->ttracker;
    uint16_t sub_com = ttracker->subcom;
    int data_params = DCE2_SMB_TRANS__NONE;
    uint8_t smb_com = DCE2_ComInfoSmbCom(com_info);

    switch (smb_com)
    {
    case SMB_COM_TRANSACTION:
        tdcnt = SmbTransactionRespTotalDataCnt((SmbTransactionResp*)nb_ptr);
        doff = SmbTransactionRespDataOff((SmbTransactionResp*)nb_ptr);
        dcnt = SmbTransactionRespDataCnt((SmbTransactionResp*)nb_ptr);
        ddisp = SmbTransactionRespDataDisp((SmbTransactionResp*)nb_ptr);
        tpcnt = SmbTransactionRespTotalParamCnt((SmbTransactionResp*)nb_ptr);
        pcnt = SmbTransactionRespParamCnt((SmbTransactionResp*)nb_ptr);
        poff = SmbTransactionRespParamOff((SmbTransactionResp*)nb_ptr);
        pdisp = SmbTransactionRespParamDisp((SmbTransactionResp*)nb_ptr);

        DebugFormat(DEBUG_DCE_SMB,
            "Transaction subcommand: %s (0x%04X)\n",
            (sub_com < TRANS_SUBCOM_MAX)
            ? smb_transaction_sub_command_strings[sub_com]
            : "Unknown", sub_com);

        switch (sub_com)
        {
        case TRANS_TRANSACT_NMPIPE:
        case TRANS_READ_NMPIPE:
            data_params = DCE2_SMB_TRANS__DATA;
            break;
        case TRANS_SET_NMPIPE_STATE:
        case TRANS_WRITE_NMPIPE:
            data_params = DCE2_SMB_TRANS__PARAMS;
            break;
        default:
            return DCE2_RET__ERROR;
        }

        break;

    case SMB_COM_TRANSACTION2:
        tpcnt = SmbTransaction2RespTotalParamCnt((SmbTransaction2Resp*)nb_ptr);
        pcnt = SmbTransaction2RespParamCnt((SmbTransaction2Resp*)nb_ptr);
        poff = SmbTransaction2RespParamOff((SmbTransaction2Resp*)nb_ptr);
        pdisp = SmbTransaction2RespParamDisp((SmbTransaction2Resp*)nb_ptr);
        tdcnt = SmbTransaction2RespTotalDataCnt((SmbTransaction2Resp*)nb_ptr);
        dcnt = SmbTransaction2RespDataCnt((SmbTransaction2Resp*)nb_ptr);
        doff = SmbTransaction2RespDataOff((SmbTransaction2Resp*)nb_ptr);
        ddisp = SmbTransaction2RespDataDisp((SmbTransaction2Resp*)nb_ptr);

        DebugFormat(DEBUG_DCE_SMB,
            "Transaction2 subcommand: %s (0x%04X)\n",
            (sub_com < TRANS2_SUBCOM_MAX)
            ? smb_transaction2_sub_command_strings[sub_com]
            : "Unknown", sub_com);

        switch (sub_com)
        {
        case TRANS2_OPEN2:
        case TRANS2_SET_FILE_INFORMATION:
            data_params = DCE2_SMB_TRANS__PARAMS;
            break;
        case TRANS2_QUERY_FILE_INFORMATION:
            data_params = DCE2_SMB_TRANS__DATA;
            break;
        default:
            return DCE2_RET__ERROR;
        }

        break;

    case SMB_COM_NT_TRANSACT:
        tpcnt = SmbNtTransactRespTotalParamCnt((SmbNtTransactResp*)nb_ptr);
        pcnt = SmbNtTransactRespParamCnt((SmbNtTransactResp*)nb_ptr);
        poff = SmbNtTransactRespParamOff((SmbNtTransactResp*)nb_ptr);
        pdisp = SmbNtTransactRespParamDisp((SmbNtTransactResp*)nb_ptr);
        tdcnt = SmbNtTransactRespTotalDataCnt((SmbNtTransactResp*)nb_ptr);
        dcnt = SmbNtTransactRespDataCnt((SmbNtTransactResp*)nb_ptr);
        doff = SmbNtTransactRespDataOff((SmbNtTransactResp*)nb_ptr);
        ddisp = SmbNtTransactRespDataDisp((SmbNtTransactResp*)nb_ptr);

        DebugFormat(DEBUG_DCE_SMB,
            "Nt Transact subcommand: %s (0x%04X)\n",
            (sub_com < NT_TRANSACT_SUBCOM_MAX)
            ? smb_nt_transact_sub_command_strings[sub_com]
            : "Unknown", sub_com);

        switch (sub_com)
        {
        case NT_TRANSACT_CREATE:
            data_params = DCE2_SMB_TRANS__PARAMS;
            break;
        default:
            return DCE2_RET__ERROR;
        }

        break;

    default:
        return DCE2_RET__ERROR;
    }

    DCE2_MOVE(nb_ptr, nb_len, DCE2_ComInfoCommandSize(com_info));

    // From client request
    if (ttracker->smb_type == SMB_TYPE__REQUEST)
    {
        ttracker->smb_type = SMB_TYPE__RESPONSE;
        ttracker->tdcnt = tdcnt;
        ttracker->tpcnt = tpcnt;
        ttracker->dsent = 0;
        ttracker->psent = 0;
        DCE2_BufferDestroy(ttracker->dbuf);
        ttracker->dbuf = nullptr;
        DCE2_BufferDestroy(ttracker->pbuf);
        ttracker->pbuf = nullptr;
    }
    else
    {
        if (tdcnt < ttracker->tdcnt)
            ttracker->tdcnt = tdcnt;
        if (tpcnt < ttracker->tpcnt)
            ttracker->tpcnt = pcnt;
    }

    if (DCE2_SmbValidateTransactionFields((uint8_t*)smb_hdr, nb_ptr, nb_len,
        DCE2_ComInfoByteCount(com_info), tdcnt, tpcnt, dcnt, doff, ddisp,
        pcnt, poff, pdisp) != DCE2_RET__SUCCESS)
        return DCE2_RET__ERROR;

    if (DCE2_SmbValidateTransactionSent(ttracker->dsent, dcnt, ttracker->tdcnt,
        ttracker->psent, pcnt, ttracker->tpcnt) != DCE2_RET__SUCCESS)
        return DCE2_RET__ERROR;

    ttracker->dsent += dcnt;
    ttracker->psent += pcnt;

    DebugFormat(DEBUG_DCE_SMB, "Data displacement: %u, "
        "Data count: %u, Total data count: %u\n"
        "Parameter displacement: %u, "
        "Parameter count: %u, Total parameter count: %u\n",
        ddisp, dcnt, tdcnt, pdisp, pcnt, tpcnt);

    if (data_params & DCE2_SMB_TRANS__DATA)
    {
        DCE2_MOVE(nb_ptr, nb_len, ((uint8_t*)smb_hdr + doff) - nb_ptr);

        if ((ttracker->dsent < ttracker->tdcnt)
            || (ttracker->psent < ttracker->tpcnt)
            || !DCE2_BufferIsEmpty(ttracker->dbuf))
        {
            if ((dcnt != 0)
                && (DCE2_SmbBufferTransactionData(ttracker, nb_ptr, dcnt, ddisp)
                != DCE2_RET__SUCCESS))
            {
                return DCE2_RET__ERROR;
            }
        }
    }

    if (data_params & DCE2_SMB_TRANS__PARAMS)
    {
        DCE2_MOVE(nb_ptr, nb_len, ((uint8_t*)smb_hdr + poff) - nb_ptr);

        if ((ttracker->dsent < ttracker->tdcnt)
            || (ttracker->psent < ttracker->tpcnt)
            || !DCE2_BufferIsEmpty(ttracker->dbuf))
        {
            if ((pcnt != 0)
                && (DCE2_SmbBufferTransactionParameters(ttracker, nb_ptr, pcnt, pdisp)
                != DCE2_RET__SUCCESS))
            {
                return DCE2_RET__ERROR;
            }
        }
    }

    if ((ttracker->dsent == ttracker->tdcnt)
        && (ttracker->psent == ttracker->tpcnt))
    {
        return DCE2_RET__FULL;
    }

    return DCE2_RET__SUCCESS;
}

// TRANS2_OPEN2
static DCE2_Ret DCE2_SmbTrans2Open2Req(DCE2_SmbSsnData* ssd,
    const uint8_t* param_ptr, uint32_t param_len, bool unicode)
{
    if (param_len < sizeof(SmbTrans2Open2ReqParams))
        return DCE2_RET__ERROR;

    if (!DCE2_SmbIsTidIPC(ssd, ssd->cur_rtracker->tid))
    {
        uint16_t file_attrs =
            SmbTrans2Open2ReqFileAttrs((SmbTrans2Open2ReqParams*)param_ptr);

        if (SmbEvasiveFileAttrs(file_attrs))
            dce_alert(GID_DCE2, DCE2_SMB_EVASIVE_FILE_ATTRS,
                (dce2CommonStats*)&dce2_smb_stats);

        ssd->cur_rtracker->file_size =
            SmbTrans2Open2ReqAllocSize((SmbTrans2Open2ReqParams*)param_ptr);
    }

    DCE2_MOVE(param_ptr, param_len, sizeof(SmbTrans2Open2ReqParams));

    ssd->cur_rtracker->file_name =
        DCE2_SmbGetString(param_ptr, param_len, unicode, false);

    return DCE2_RET__SUCCESS;
}

// TRANS2_QUERY_FILE_INFORMATION
static DCE2_Ret DCE2_SmbTrans2QueryFileInfoReq(DCE2_SmbSsnData* ssd,
    const uint8_t* param_ptr, uint32_t param_len)
{
    DCE2_SmbTransactionTracker* ttracker = &ssd->cur_rtracker->ttracker;
    DCE2_SmbFileTracker* ftracker;

    if (param_len < sizeof(SmbTrans2QueryFileInfoReqParams))
        return DCE2_RET__ERROR;

    ftracker = DCE2_SmbFindFileTracker(ssd,
        ssd->cur_rtracker->uid, ssd->cur_rtracker->tid,
        SmbTrans2QueryFileInfoReqFid((SmbTrans2QueryFileInfoReqParams*)param_ptr));

    if ((ftracker == nullptr) || ftracker->is_ipc
        || DCE2_SmbFileUpload(ftracker->ff_file_direction))
        return DCE2_RET__IGNORE;

    ttracker->info_level =
        SmbTrans2QueryFileInfoReqInfoLevel((SmbTrans2QueryFileInfoReqParams*)param_ptr);

    ssd->cur_rtracker->ftracker = ftracker;

    return DCE2_RET__SUCCESS;
}

// TRANS2_SET_FILE_INFORMATION
static DCE2_Ret DCE2_SmbTrans2SetFileInfoReq(DCE2_SmbSsnData* ssd,
    const uint8_t* param_ptr, uint32_t param_len,
    const uint8_t* data_ptr, uint32_t data_len)
{
    DCE2_SmbTransactionTracker* ttracker = &ssd->cur_rtracker->ttracker;
    DCE2_SmbFileTracker* ftracker;

    if ((param_len < sizeof(SmbTrans2SetFileInfoReqParams))
        || (data_len < sizeof(uint64_t)))
        return DCE2_RET__ERROR;

    ttracker->info_level =
        SmbTrans2SetFileInfoReqInfoLevel((SmbTrans2SetFileInfoReqParams*)param_ptr);

    // Check to see if there is an attempt to set READONLY/HIDDEN/SYSTEM
    // attributes on a file
    if (SmbSetFileInfoSetFileBasicInfo(ttracker->info_level)
        && (data_len >= sizeof(SmbSetFileBasicInfo)))
    {
        uint32_t ext_file_attrs =
            SmbSetFileInfoExtFileAttrs((SmbSetFileBasicInfo*)data_ptr);

        if (SmbEvasiveFileAttrs(ext_file_attrs))
            dce_alert(GID_DCE2, DCE2_SMB_EVASIVE_FILE_ATTRS,
                (dce2CommonStats*)&dce2_smb_stats);

        // Don't need to see the response
        return DCE2_RET__IGNORE;
    }

    // Only looking for end of file information for this subcommand
    if (!SmbSetFileInfoEndOfFile(ttracker->info_level))
        return DCE2_RET__IGNORE;

    ftracker = DCE2_SmbFindFileTracker(ssd,
        ssd->cur_rtracker->uid, ssd->cur_rtracker->tid,
        SmbTrans2SetFileInfoReqFid((SmbTrans2SetFileInfoReqParams*)param_ptr));

    if ((ftracker == nullptr) || ftracker->is_ipc
        || DCE2_SmbFileDownload(ftracker->ff_file_direction)
        || (ftracker->ff_bytes_processed != 0))
        return DCE2_RET__IGNORE;

    ssd->cur_rtracker->file_size = alignedNtohq((uint64_t*)data_ptr);
    ssd->cur_rtracker->ftracker = ftracker;

    return DCE2_RET__SUCCESS;
}

/********************************************************************
 * Functions:
 *   DCE2_SmbTransaction()
 *   DCE2_SmbTransactionSecondary()
 *   DCE2_SmbTransaction2()
 *   DCE2_SmbTransaction2Secondary()
 *   DCE2_SmbNtTransact()
 *   DCE2_SmbNtTransactSecondary()
 * Arguments:
 *  DCE2_SmbSsnData *       - SMB session data structure
 *  const SmbNtHdr *        - SMB header structure (packet pointer)
 *  const DCE2_SmbComInfo * - Basic command information structure
 *  uint8_t *               - pointer to start of command (packet pointer)
 *  uint32_t                - remaining NetBIOS length
 *
 * Returns:
 *  DCE2_Ret - DCE2_RET__ERROR if something went wrong and/or processing
 *               should stop
 *             DCE2_RET__SUCCESS if processing should continue
 *
 ********************************************************************/

// SMB_COM_TRANSACTION
DCE2_Ret DCE2_SmbTransaction(DCE2_SmbSsnData* ssd, const SmbNtHdr* smb_hdr,
    const DCE2_SmbComInfo* com_info, const uint8_t* nb_ptr, uint32_t nb_len)
{
    uint16_t com_size = DCE2_ComInfoCommandSize(com_info);
    DCE2_SmbTransactionTracker* ttracker = &ssd->cur_rtracker->ttracker;

    // Got a matching request for an in progress transaction - don't process it,
    // but don't want to remove tracker.
    if (DCE2_ComInfoIsRequest(com_info)
        && !DCE2_SmbIsTransactionComplete(ttracker))
    {
        DebugMessage(DEBUG_DCE_SMB, "Got new transaction request "
            "that matches an in progress transaction - not inspecting.\n");
        return DCE2_RET__ERROR;
    }

    // Avoid decoding/tracking \PIPE\LANMAN requests
    if (DCE2_ComInfoIsRequest(com_info)
        && (DCE2_ComInfoWordCount(com_info) != 16))
    {
        DebugMessage(DEBUG_DCE_SMB, "\\PIPE\\LANMAN request - not inspecting\n");
        return DCE2_RET__IGNORE;
    }

    if (!DCE2_ComInfoCanProcessCommand(com_info))
        return DCE2_RET__ERROR;

    // Interim response is sent if client didn't send all data / parameters
    // in initial Transaction request and will have to complete the request
    // with TransactionSecondary commands.
    if (DCE2_ComInfoIsResponse(com_info)
        && (com_size == sizeof(SmbTransactionInterimResp)))
    {
        DebugMessage(DEBUG_DCE_SMB,
            "  Server Transaction interim response.\n");

        return DCE2_RET__SUCCESS;
    }

    if (DCE2_ComInfoIsRequest(com_info))
    {
        DCE2_Ret status =
            DCE2_SmbUpdateTransRequest(ssd, smb_hdr, com_info, nb_ptr, nb_len);

        if (status != DCE2_RET__FULL)
            return status;

        ttracker->disconnect_tid = SmbTransactionReqDisconnectTid((SmbTransactionReq*)nb_ptr);
        ttracker->one_way = SmbTransactionReqOneWay((SmbTransactionReq*)nb_ptr);

        uint16_t doff = SmbTransactionReqDataOff((SmbTransactionReq*)nb_ptr);
        uint16_t dcnt = SmbTransactionReqDataCnt((SmbTransactionReq*)nb_ptr);
        uint16_t pcnt = SmbTransactionReqParamCnt((SmbTransactionReq*)nb_ptr);
        uint16_t poff = SmbTransactionReqParamOff((SmbTransactionReq*)nb_ptr);

        DCE2_MOVE(nb_ptr, nb_len, ((uint8_t*)smb_hdr + doff) - nb_ptr);
        const uint8_t* data_ptr = nb_ptr;

        DCE2_MOVE(nb_ptr, nb_len, ((uint8_t*)smb_hdr + poff) - nb_ptr);
        const uint8_t* param_ptr = nb_ptr;

        status = DCE2_SmbTransactionReq(ssd, ttracker, data_ptr, dcnt, param_ptr, pcnt);
        if (status != DCE2_RET__SUCCESS)
            return status;
    }
    else
    {
        DCE2_Ret status =
            DCE2_SmbUpdateTransResponse(ssd, smb_hdr, com_info, nb_ptr, nb_len);

        if (status != DCE2_RET__FULL)
            return status;

        switch (ttracker->subcom)
        {
        case TRANS_TRANSACT_NMPIPE:
        case TRANS_READ_NMPIPE:
            if (!DCE2_BufferIsEmpty(ttracker->dbuf))
            {
                const uint8_t* data_ptr = DCE2_BufferData(ttracker->dbuf);
                uint32_t data_len = DCE2_BufferLength(ttracker->dbuf);
                Packet* rpkt = DCE2_SmbGetRpkt(ssd, &data_ptr,
                    &data_len, DCE2_RPKT_TYPE__SMB_TRANS);

                if (rpkt == nullptr)
                    return DCE2_RET__ERROR;

                DebugMessage(DEBUG_DCE_SMB, "Reassembled Transaction response\n");
                DCE2_PrintPktData(rpkt->data, rpkt->dsize);

                status = DCE2_SmbProcessResponseData(ssd, data_ptr, data_len);

                DCE2_SmbReturnRpkt(ssd);

                if (status != DCE2_RET__SUCCESS)
                    return status;
            }
            else
            {
                uint16_t dcnt = SmbTransactionRespDataCnt((SmbTransactionResp*)nb_ptr);
                uint16_t doff = SmbTransactionRespDataOff((SmbTransactionResp*)nb_ptr);

                DCE2_MOVE(nb_ptr, nb_len, ((uint8_t*)smb_hdr + doff) - nb_ptr);

                if (DCE2_SmbProcessResponseData(ssd, nb_ptr, dcnt) != DCE2_RET__SUCCESS)
                    return DCE2_RET__ERROR;
            }

            break;

        case TRANS_SET_NMPIPE_STATE:
            DebugFormat(DEBUG_DCE_SMB, "Setting pipe "
                "to %s mode\n", ttracker->pipe_byte_mode ? "byte" : "message");
            ssd->cur_rtracker->ftracker->fp_byte_mode = ttracker->pipe_byte_mode;
            break;

        case TRANS_WRITE_NMPIPE:
            break;

        default:
            return DCE2_RET__ERROR;
        }

        if (ttracker->disconnect_tid)
            DCE2_SmbRemoveTid(ssd, ssd->cur_rtracker->tid);
    }

    return DCE2_RET__SUCCESS;
}

// SMB_COM_TRANSACTION2
DCE2_Ret DCE2_SmbTransaction2(DCE2_SmbSsnData* ssd, const SmbNtHdr* smb_hdr,
    const DCE2_SmbComInfo* com_info, const uint8_t* nb_ptr, uint32_t nb_len)
{
    uint16_t com_size = DCE2_ComInfoCommandSize(com_info);
    DCE2_SmbTransactionTracker* ttracker = &ssd->cur_rtracker->ttracker;

    // Got a matching request for an in progress transaction - don't process it,
    // but don't want to remove tracker.
    if (DCE2_ComInfoIsRequest(com_info)
        && !DCE2_SmbIsTransactionComplete(ttracker))
    {
        DebugMessage(DEBUG_DCE_SMB, "Got new transaction request "
            "that matches an in progress transaction - not inspecting.\n");
        return DCE2_RET__ERROR;
    }

    if (!DCE2_ComInfoCanProcessCommand(com_info))
        return DCE2_RET__ERROR;

    // Interim response is sent if client didn't send all data / parameters
    // in initial Transaction2 request and will have to complete the request
    // with Transaction2Secondary commands.
    if (DCE2_ComInfoIsResponse(com_info)
        && (com_size == sizeof(SmbTransaction2InterimResp)))
    {
        return DCE2_RET__SUCCESS;
    }

    if (DCE2_ComInfoIsRequest(com_info))
    {
        uint16_t pcnt = SmbTransaction2ReqParamCnt((SmbTransaction2Req*)nb_ptr);
        uint16_t poff = SmbTransaction2ReqParamOff((SmbTransaction2Req*)nb_ptr);
        uint16_t dcnt = SmbTransaction2ReqDataCnt((SmbTransaction2Req*)nb_ptr);
        uint16_t doff = SmbTransaction2ReqDataOff((SmbTransaction2Req*)nb_ptr);
        const uint8_t* data_ptr;
        DCE2_Ret status =
            DCE2_SmbUpdateTransRequest(ssd, smb_hdr, com_info, nb_ptr, nb_len);

        if (status != DCE2_RET__FULL)
            return status;

        DCE2_MOVE(nb_ptr, nb_len, ((uint8_t*)smb_hdr + poff) - nb_ptr);

        switch (ttracker->subcom)
        {
        case TRANS2_OPEN2:
            if (DCE2_SmbTrans2Open2Req(ssd, nb_ptr, pcnt,
                SmbUnicode(smb_hdr)) != DCE2_RET__SUCCESS)
                return DCE2_RET__ERROR;
            break;

        case TRANS2_QUERY_FILE_INFORMATION:
            status = DCE2_SmbTrans2QueryFileInfoReq(ssd, nb_ptr, pcnt);
            if (status != DCE2_RET__SUCCESS)
                return status;
            break;

        case TRANS2_SET_FILE_INFORMATION:
            data_ptr = nb_ptr;
            DCE2_MOVE(data_ptr, nb_len, ((uint8_t*)smb_hdr + doff) - data_ptr);

            status = DCE2_SmbTrans2SetFileInfoReq(ssd, nb_ptr, pcnt, data_ptr, dcnt);
            if (status != DCE2_RET__SUCCESS)
                return status;
            break;

        default:
            return DCE2_RET__IGNORE;
        }
    }
    else
    {
        const uint8_t* ptr;
        uint32_t len;
        DCE2_SmbFileTracker* ftracker = nullptr;
        DCE2_Ret status =
            DCE2_SmbUpdateTransResponse(ssd, smb_hdr, com_info, nb_ptr, nb_len);

        if (status != DCE2_RET__FULL)
            return status;

        switch (ttracker->subcom)
        {
        case TRANS2_OPEN2:
            if (!DCE2_BufferIsEmpty(ttracker->pbuf))
            {
                ptr = DCE2_BufferData(ttracker->pbuf);
                len = DCE2_BufferLength(ttracker->pbuf);
            }
            else
            {
                uint16_t poff = SmbTransaction2RespParamOff((SmbTransaction2Resp*)nb_ptr);
                uint16_t pcnt = SmbTransaction2RespParamCnt((SmbTransaction2Resp*)nb_ptr);

                DCE2_MOVE(nb_ptr, nb_len, ((uint8_t*)smb_hdr + poff) - nb_ptr);

                ptr = nb_ptr;
                len = pcnt;
            }

            if (len < sizeof(SmbTrans2Open2RespParams))
                return DCE2_RET__ERROR;

            if (!DCE2_SmbIsTidIPC(ssd, ssd->cur_rtracker->tid)
                && (SmbFileAttrsDirectory(SmbTrans2Open2RespFileAttrs(
                (SmbTrans2Open2RespParams*)ptr))
                || !SmbResourceTypeDisk(SmbTrans2Open2RespResourceType(
                (SmbTrans2Open2RespParams*)ptr))))
            {
                return DCE2_RET__SUCCESS;
            }

            ftracker = DCE2_SmbNewFileTracker(ssd, ssd->cur_rtracker->uid,
                ssd->cur_rtracker->tid, SmbTrans2Open2RespFid((SmbTrans2Open2RespParams*)ptr));
            if (ftracker == nullptr)
                return DCE2_RET__ERROR;

            ftracker->file_name = ssd->cur_rtracker->file_name;
            ssd->cur_rtracker->file_name = nullptr;

            if (!ftracker->is_ipc)
            {
                uint16_t open_results =
                    SmbTrans2Open2RespActionTaken((SmbTrans2Open2RespParams*)ptr);

                if (SmbOpenResultRead(open_results))
                {
                    ftracker->ff_file_size =
                        SmbTrans2Open2RespFileDataSize((SmbTrans2Open2RespParams*)ptr);
                }
                else
                {
                    ftracker->ff_file_size = ssd->cur_rtracker->file_size;
                    ftracker->ff_file_direction = DCE2_SMB_FILE_DIRECTION__UPLOAD;
                }
            }
            break;

        case TRANS2_QUERY_FILE_INFORMATION:
            ftracker = ssd->cur_rtracker->ftracker;
            if (ftracker == nullptr)
                return DCE2_RET__ERROR;

            if (!DCE2_BufferIsEmpty(ttracker->dbuf))
            {
                ptr = DCE2_BufferData(ttracker->dbuf);
                len = DCE2_BufferLength(ttracker->dbuf);
            }
            else
            {
                uint16_t doff = SmbTransaction2RespDataOff((SmbTransaction2Resp*)nb_ptr);
                uint16_t dcnt = SmbTransaction2RespDataCnt((SmbTransaction2Resp*)nb_ptr);

                DCE2_MOVE(nb_ptr, nb_len, ((uint8_t*)smb_hdr + doff) - nb_ptr);

                ptr = nb_ptr;
                len = dcnt;
            }

            switch (ttracker->info_level)
            {
            case SMB_INFO_STANDARD:
                if (len >= sizeof(SmbQueryInfoStandard))
                {
                    ftracker->ff_file_size =
                        SmbQueryInfoStandardFileDataSize((SmbQueryInfoStandard*)ptr);
                }
                break;
            case SMB_INFO_QUERY_EA_SIZE:
                if (len >= sizeof(SmbQueryInfoQueryEaSize))
                {
                    ftracker->ff_file_size =
                        SmbQueryInfoQueryEaSizeFileDataSize((SmbQueryInfoQueryEaSize*)ptr);
                }
                break;
            case SMB_QUERY_FILE_STANDARD_INFO:
                if (len >= sizeof(SmbQueryFileStandardInfo))
                {
                    ftracker->ff_file_size =
                        SmbQueryFileStandardInfoEndOfFile((SmbQueryFileStandardInfo*)ptr);
                }
                break;
            case SMB_QUERY_FILE_ALL_INFO:
                if (len >= sizeof(SmbQueryFileAllInfo))
                {
                    ftracker->ff_file_size =
                        SmbQueryFileAllInfoEndOfFile((SmbQueryFileAllInfo*)ptr);
                }
                break;
            case SMB_INFO_PT_FILE_STANDARD_INFO:
                if (len >= sizeof(SmbQueryPTFileStreamInfo))
                {
                    ftracker->ff_file_size =
                        SmbQueryPTFileStreamInfoStreamSize((SmbQueryPTFileStreamInfo*)ptr);
                }
                break;
            case SMB_INFO_PT_FILE_STREAM_INFO:
                if (len >= sizeof(SmbQueryFileStandardInfo))
                {
                    ftracker->ff_file_size =
                        SmbQueryFileStandardInfoEndOfFile((SmbQueryFileStandardInfo*)ptr);
                }
                break;
            case SMB_INFO_PT_FILE_ALL_INFO:
                if (len >= sizeof(SmbQueryPTFileAllInfo))
                {
                    ftracker->ff_file_size =
                        SmbQueryPTFileAllInfoEndOfFile((SmbQueryPTFileAllInfo*)ptr);
                }
                break;
            case SMB_INFO_PT_NETWORK_OPEN_INFO:
                if (len >= sizeof(SmbQueryPTNetworkOpenInfo))
                {
                    ftracker->ff_file_size =
                        SmbQueryPTNetworkOpenInfoEndOfFile((SmbQueryPTNetworkOpenInfo*)ptr);
                }
                break;
            default:
                break;
            }
            break;

        case TRANS2_SET_FILE_INFORMATION:
            ftracker = ssd->cur_rtracker->ftracker;
            if (ftracker == nullptr)
                return DCE2_RET__ERROR;

            if (!DCE2_BufferIsEmpty(ttracker->pbuf))
            {
                ptr = DCE2_BufferData(ttracker->pbuf);
                len = DCE2_BufferLength(ttracker->pbuf);
            }
            else
            {
                uint16_t poff = SmbTransaction2RespParamOff((SmbTransaction2Resp*)nb_ptr);
                uint16_t pcnt = SmbTransaction2RespParamCnt((SmbTransaction2Resp*)nb_ptr);

                DCE2_MOVE(nb_ptr, nb_len, ((uint8_t*)smb_hdr + poff) - nb_ptr);

                ptr = nb_ptr;
                len = pcnt;
            }

            // *ptr will be non-zero if there was an error.
            if ((len >= 2) && (*ptr == 0))
                ftracker->ff_file_size = ssd->cur_rtracker->file_size;
            break;

        default:
            break;
        }
    }

    return DCE2_RET__SUCCESS;
}

// SMB_COM_NT_TRANSACT
DCE2_Ret DCE2_SmbNtTransact(DCE2_SmbSsnData* ssd, const SmbNtHdr* smb_hdr,
    const DCE2_SmbComInfo* com_info, const uint8_t* nb_ptr, uint32_t nb_len)
{
    uint16_t com_size = DCE2_ComInfoCommandSize(com_info);
    DCE2_SmbTransactionTracker* ttracker = &ssd->cur_rtracker->ttracker;

    // NOTE: Only looking at NT_TRANSACT_CREATE as another way to open a named pipe

    // Got a matching request for an in progress transaction - don't process it,
    // but don't want to remove tracker.
    if (DCE2_ComInfoIsRequest(com_info)
        && !DCE2_SmbIsTransactionComplete(ttracker))
    {
        DebugMessage(DEBUG_DCE_SMB, "Got new transaction request "
            "that matches an in progress transaction - not inspecting.\n");
        return DCE2_RET__ERROR;
    }

    if (!DCE2_ComInfoCanProcessCommand(com_info))
        return DCE2_RET__ERROR;

    // Interim response is sent if client didn't send all data / parameters
    // in initial NtTransact request and will have to complete the request
    // with NtTransactSecondary commands.
    if (DCE2_ComInfoIsResponse(com_info)
        && (com_size == sizeof(SmbNtTransactInterimResp)))
    {
        return DCE2_RET__SUCCESS;
    }

    if (DCE2_ComInfoIsRequest(com_info))
    {
        uint32_t pcnt = SmbNtTransactReqParamCnt((SmbNtTransactReq*)nb_ptr);
        uint32_t poff = SmbNtTransactReqParamOff((SmbNtTransactReq*)nb_ptr);
        DCE2_Ret status =
            DCE2_SmbUpdateTransRequest(ssd, smb_hdr, com_info, nb_ptr, nb_len);

        if (status != DCE2_RET__FULL)
            return status;

        DCE2_MOVE(nb_ptr, nb_len, ((uint8_t*)smb_hdr + poff) - nb_ptr);

        switch (ttracker->subcom)
        {
        case NT_TRANSACT_CREATE:
            status = DCE2_SmbNtTransactCreateReq(ssd, nb_ptr, pcnt, SmbUnicode(smb_hdr));
            if (status != DCE2_RET__SUCCESS)
                return status;
            break;

        default:
            return DCE2_RET__IGNORE;
        }
    }
    else
    {
        const uint8_t* ptr;
        uint32_t len;
        DCE2_SmbFileTracker* ftracker = nullptr;

        DCE2_Ret status =
            DCE2_SmbUpdateTransResponse(ssd, smb_hdr, com_info, nb_ptr, nb_len);

        if (status != DCE2_RET__FULL)
            return status;

        if (!DCE2_BufferIsEmpty(ttracker->pbuf))
        {
            ptr = DCE2_BufferData(ttracker->pbuf);
            len = DCE2_BufferLength(ttracker->pbuf);
        }
        else
        {
            uint32_t poff = SmbNtTransactRespParamOff((SmbNtTransactResp*)nb_ptr);
            uint32_t pcnt = SmbNtTransactRespParamCnt((SmbNtTransactResp*)nb_ptr);

            DCE2_MOVE(nb_ptr, nb_len, ((uint8_t*)smb_hdr + poff) - nb_ptr);

            ptr = nb_ptr;
            len = pcnt;
        }

        if (len < sizeof(SmbNtTransactCreateRespParams))
            return DCE2_RET__ERROR;

        if (!DCE2_SmbIsTidIPC(ssd, ssd->cur_rtracker->tid))
        {
            const bool is_directory =
                SmbNtTransactCreateRespDirectory((SmbNtTransactCreateRespParams*)ptr);
            const uint16_t resource_type =
                SmbNtTransactCreateRespResourceType((SmbNtTransactCreateRespParams*)ptr);

            if (is_directory || !SmbResourceTypeDisk(resource_type))
                return DCE2_RET__SUCCESS;

            // Give preference to files opened with the sequential only flag set
            if (((ssd->fapi_ftracker == nullptr) || !ssd->fapi_ftracker->ff_sequential_only)
                && ssd->cur_rtracker->sequential_only)
            {
                DCE2_SmbAbortFileAPI(ssd);
            }
        }

        ftracker = DCE2_SmbNewFileTracker(ssd,
            ssd->cur_rtracker->uid, ssd->cur_rtracker->tid,
            SmbNtTransactCreateRespFid((SmbNtTransactCreateRespParams*)ptr));
        if (ftracker == nullptr)
            return DCE2_RET__ERROR;

        ftracker->file_name = ssd->cur_rtracker->file_name;
        ssd->cur_rtracker->file_name = nullptr;

        if (!ftracker->is_ipc)
        {
            uint32_t create_disposition =
                SmbNtTransactCreateRespCreateAction((SmbNtTransactCreateRespParams*)ptr);

            if (SmbCreateDispositionRead(create_disposition))
            {
                ftracker->ff_file_size =
                    SmbNtTransactCreateRespEndOfFile((SmbNtTransactCreateRespParams*)ptr);
            }
            else
            {
                ftracker->ff_file_size = ssd->cur_rtracker->file_size;
                ftracker->ff_file_direction = DCE2_SMB_FILE_DIRECTION__UPLOAD;
            }

            ftracker->ff_sequential_only = ssd->cur_rtracker->sequential_only;
        }
    }

    return DCE2_RET__SUCCESS;
}

// SMB_COM_TRANSACTION_SECONDARY
DCE2_Ret DCE2_SmbTransactionSecondary(DCE2_SmbSsnData* ssd, const SmbNtHdr* smb_hdr,
    const DCE2_SmbComInfo* com_info, const uint8_t* nb_ptr, uint32_t nb_len)
{
    DCE2_SmbTransactionTracker* ttracker = &ssd->cur_rtracker->ttracker;
    DCE2_Ret status;

    if (!DCE2_ComInfoCanProcessCommand(com_info))
        return DCE2_RET__ERROR;

    status = DCE2_SmbUpdateTransSecondary(ssd, smb_hdr, com_info, nb_ptr, nb_len);
    if (status != DCE2_RET__FULL)
        return status;

    switch (ttracker->subcom)
    {
    case TRANS_TRANSACT_NMPIPE:
    case TRANS_WRITE_NMPIPE:
    {
        const uint8_t* data_ptr = DCE2_BufferData(ttracker->dbuf);
        uint32_t data_len = DCE2_BufferLength(ttracker->dbuf);
        Packet* rpkt = DCE2_SmbGetRpkt(ssd, &data_ptr, &data_len, DCE2_RPKT_TYPE__SMB_TRANS);

        if (rpkt == nullptr)
            return DCE2_RET__ERROR;

        DebugMessage(DEBUG_DCE_SMB, "Reassembled Transaction request\n");
        DCE2_PrintPktData(rpkt->data, rpkt->dsize);

        status = DCE2_SmbTransactionReq(ssd, ttracker, data_ptr, data_len,
            DCE2_BufferData(ttracker->pbuf), DCE2_BufferLength(ttracker->pbuf));

        DCE2_SmbReturnRpkt(ssd);
    }
    break;

    default:
        status = DCE2_SmbTransactionReq(ssd, ttracker,
            DCE2_BufferData(ttracker->dbuf), DCE2_BufferLength(ttracker->dbuf),
            DCE2_BufferData(ttracker->pbuf), DCE2_BufferLength(ttracker->pbuf));
        break;
    }

    return status;
}

// SMB_COM_TRANSACTION2_SECONDARY
DCE2_Ret DCE2_SmbTransaction2Secondary(DCE2_SmbSsnData* ssd, const SmbNtHdr* smb_hdr,
    const DCE2_SmbComInfo* com_info, const uint8_t* nb_ptr, uint32_t nb_len)
{
    DCE2_Ret status;
    DCE2_SmbTransactionTracker* ttracker = &ssd->cur_rtracker->ttracker;

    if (!DCE2_ComInfoCanProcessCommand(com_info))
        return DCE2_RET__ERROR;

    status = DCE2_SmbUpdateTransSecondary(ssd, smb_hdr, com_info, nb_ptr, nb_len);
    if (status != DCE2_RET__FULL)
        return status;

    switch (ttracker->subcom)
    {
    case TRANS2_OPEN2:
        status = DCE2_SmbTrans2Open2Req(ssd, DCE2_BufferData(ttracker->pbuf),
            DCE2_BufferLength(ttracker->pbuf), SmbUnicode(smb_hdr));
        if (status != DCE2_RET__SUCCESS)
            return status;
        break;

    case TRANS2_QUERY_FILE_INFORMATION:
        status = DCE2_SmbTrans2QueryFileInfoReq(ssd, DCE2_BufferData(ttracker->pbuf),
            DCE2_BufferLength(ttracker->pbuf));
        if (status != DCE2_RET__SUCCESS)
            return status;
        break;

    case TRANS2_SET_FILE_INFORMATION:
        status = DCE2_SmbTrans2SetFileInfoReq(ssd, DCE2_BufferData(ttracker->pbuf),
            DCE2_BufferLength(ttracker->pbuf),
            DCE2_BufferData(ttracker->dbuf),
            DCE2_BufferLength(ttracker->dbuf));
        if (status != DCE2_RET__SUCCESS)
            return status;
        break;

    default:
        break;
    }

    return DCE2_RET__SUCCESS;
}

// SMB_COM_NT_TRANSACT_SECONDARY
DCE2_Ret DCE2_SmbNtTransactSecondary(DCE2_SmbSsnData* ssd, const SmbNtHdr* smb_hdr,
    const DCE2_SmbComInfo* com_info, const uint8_t* nb_ptr, uint32_t nb_len)
{
    DCE2_Ret status;
    DCE2_SmbTransactionTracker* ttracker = &ssd->cur_rtracker->ttracker;

    if (!DCE2_ComInfoCanProcessCommand(com_info))
        return DCE2_RET__ERROR;

    status = DCE2_SmbUpdateTransSecondary(ssd, smb_hdr, com_info, nb_ptr, nb_len);
    if (status != DCE2_RET__FULL)
        return status;

    switch (ttracker->subcom)
    {
    case NT_TRANSACT_CREATE:
        status = DCE2_SmbNtTransactCreateReq(ssd, DCE2_BufferData(ttracker->pbuf),
            DCE2_BufferLength(ttracker->pbuf), SmbUnicode(smb_hdr));
        if (status != DCE2_RET__SUCCESS)
            return status;
        break;

    default:
        break;
    }

    return DCE2_RET__SUCCESS;
}

