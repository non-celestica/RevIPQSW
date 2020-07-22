/* Copyright (c) 2014, 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _MHI_HWIO_
#define _MHI_HWIO_
#define MHIREGLEN (0x0)
#define MHIREGLEN_MHIREGLEN_MASK 0xffffffff
#define MHIREGLEN_MHIREGLEN_SHIFT 0x0


#define MHIVER (0x8)
#define MHIVER_MHIVER_MASK 0xffffffff
#define MHIVER_MHIVER_SHIFT 0x0


#define MHICFG (0x10)
#define MHICFG_NHWER_MASK (0xff000000)
#define MHICFG_NHWER_SHIFT (24)
#define MHICFG_NER_MASK (0xff0000)
#define MHICFG_NER_SHIFT (16)
#define MHICFG_NHWCH_MASK (0xff00)
#define MHICFG_NHWCH_SHIFT (8)
#define MHICFG_NCH_MASK (0xff)
#define MHICFG_NCH_SHIFT (0)


#define CHDBOFF (0x18)
#define CHDBOFF_CHDBOFF_MASK 0xffffffff
#define CHDBOFF_CHDBOFF_SHIFT 0x0


#define ERDBOFF (0x20)
#define ERDBOFF_ERDBOFF_MASK 0xffffffff
#define ERDBOFF_ERDBOFF_SHIFT 0x0


#define BHIOFF (0x28)
#define BHIOFF_BHIOFF_MASK 0xffffffff
#define BHIOFF_BHIOFF_SHIFT 0x0


#define DEBUGOFF (0x30)
#define DEBUGOFF_DEBUGOFF_MASK 0xffffffff
#define DEBUGOFF_DEBUGOFF_SHIFT 0x0


#define MHICTRL (0x38)
#define MHICTRL_MHISTATE_MASK 0x0000FF00
#define MHICTRL_MHISTATE_SHIFT 0x8
#define MHICTRL_RESET_MASK 0x2
#define MHICTRL_RESET_SHIFT 0x1


#define MHISTATUS (0x48)
#define MHISTATUS_MHISTATE_MASK 0x0000ff00
#define MHISTATUS_MHISTATE_SHIFT 0x8
#define MHISTATUS_SYSERR_MASK 0x4
#define MHISTATUS_SYSERR_SHIFT 0x2
#define MHISTATUS_READY_MASK 0x1
#define MHISTATUS_READY_SHIFT 0x0


#define CCABAP_LOWER (0x58)
#define CCABAP_LOWER_CCABAP_LOWER_MASK 0xffffffff
#define CCABAP_LOWER_CCABAP_LOWER_SHIFT 0x0


#define CCABAP_HIGHER (0x5c)
#define CCABAP_HIGHER_CCABAP_HIGHER_MASK 0xffffffff
#define CCABAP_HIGHER_CCABAP_HIGHER_SHIFT 0x0


#define ECABAP_LOWER (0x60)
#define ECABAP_LOWER_ECABAP_LOWER_MASK 0xffffffff
#define ECABAP_LOWER_ECABAP_LOWER_SHIFT 0x0


#define ECABAP_HIGHER (0x64)
#define ECABAP_HIGHER_ECABAP_HIGHER_MASK 0xffffffff
#define ECABAP_HIGHER_ECABAP_HIGHER_SHIFT 0x0


#define CRCBAP_LOWER (0x68)
#define CRCBAP_LOWER_CRCBAP_LOWER_MASK 0xffffffff
#define CRCBAP_LOWER_CRCBAP_LOWER_SHIFT 0x0


#define CRCBAP_HIGHER (0x6c)
#define CRCBAP_HIGHER_CRCBAP_HIGHER_MASK 0xffffffff
#define CRCBAP_HIGHER_CRCBAP_HIGHER_SHIFT 0x0


#define CRDB_LOWER (0x70)
#define CRDB_LOWER_CRDB_LOWER_MASK 0xffffffff
#define CRDB_LOWER_CRDB_LOWER_SHIFT 0x0


#define CRDB_HIGHER (0x74)
#define CRDB_HIGHER_CRDB_HIGHER_MASK 0xffffffff
#define CRDB_HIGHER_CRDB_HIGHER_SHIFT 0x0


#define MHICTRLBASE_LOWER (0x80)
#define MHICTRLBASE_LOWER_MHICTRLBASE_LOWER_MASK 0xffffffff
#define MHICTRLBASE_LOWER_MHICTRLBASE_LOWER_SHIFT 0x0


#define MHICTRLBASE_HIGHER (0x84)
#define MHICTRLBASE_HIGHER_MHICTRLBASE_HIGHER_MASK 0xffffffff
#define MHICTRLBASE_HIGHER_MHICTRLBASE_HIGHER_SHIFT 0x0


#define MHICTRLLIMIT_LOWER (0x88)
#define MHICTRLLIMIT_LOWER_MHICTRLLIMIT_LOWER_MASK 0xffffffff
#define MHICTRLLIMIT_LOWER_MHICTRLLIMIT_LOWER_SHIFT 0x0


#define MHICTRLLIMIT_HIGHER (0x8c)
#define MHICTRLLIMIT_HIGHER_MHICTRLLIMIT_HIGHER_MASK 0xffffffff
#define MHICTRLLIMIT_HIGHER_MHICTRLLIMIT_HIGHER_SHIFT 0x0


#define MHIDATABASE_LOWER (0x98)
#define MHIDATABASE_LOWER_MHIDATABASE_LOWER_MASK 0xffffffff
#define MHIDATABASE_LOWER_MHIDATABASE_LOWER_SHIFT 0x0


#define MHIDATABASE_HIGHER (0x9c)
#define MHIDATABASE_HIGHER_MHIDATABASE_HIGHER_MASK 0xffffffff
#define MHIDATABASE_HIGHER_MHIDATABASE_HIGHER_SHIFT 0x0


#define MHIDATALIMIT_LOWER (0xa0)
#define MHIDATALIMIT_LOWER_MHIDATALIMIT_LOWER_MASK 0xffffffff
#define MHIDATALIMIT_LOWER_MHIDATALIMIT_LOWER_SHIFT 0x0


#define MHIDATALIMIT_HIGHER (0xa4)
#define MHIDATALIMIT_HIGHER_MHIDATALIMIT_HIGHER_MASK 0xffffffff
#define MHIDATALIMIT_HIGHER_MHIDATALIMIT_HIGHER_SHIFT 0x0

#define CHDB_LOWER_n(n) (0x0400 + 0x8 * (n))
#define CHDB_LOWER_n_CHDB_LOWER_MASK 0xffffffff
#define CHDB_LOWER_n_CHDB_LOWER_SHIFT 0x0


#define CHDB_HIGHER_n(n) (0x0404 + 0x8 * (n))
#define CHDB_HIGHER_n_CHDB_HIGHER_MASK 0xffffffff
#define CHDB_HIGHER_n_CHDB_HIGHER_SHIFT 0x0


#define ERDB_LOWER_n(n) (0x0800 + 0x8 * (n))
#define ERDB_LOWER_n_ERDB_LOWER_MASK 0xffffffff
#define ERDB_LOWER_n_ERDB_LOWER_SHIFT 0x0


#define ERDB_HIGHER_n(n) (0x0804 + 0x8 * (n))
#define ERDB_HIGHER_n_ERDB_HIGHER_MASK 0xffffffff
#define ERDB_HIGHER_n_ERDB_HIGHER_SHIFT 0x0

#endif