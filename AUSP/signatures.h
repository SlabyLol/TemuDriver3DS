#ifndef AUSP_SIGNATURES_H
#define AUSP_SIGNATURES_H

#include <3ds.h>

typedef struct {
    const u8 *data;
    u32 length;
    const char *name;
    int is_nimbus;
} AuspSignature;

static const u8 sig_ntr_cfw[]      = {0x4E, 0x54, 0x52, 0x20, 0x43, 0x46, 0x57};
static const u8 sig_ntr_plg[]      = {0x4E, 0x54, 0x52, 0x50, 0x4C, 0x47};
static const u8 sig_ntr_debug[]    = {0x4E, 0x54, 0x52, 0x20, 0x44, 0x65, 0x62, 0x75, 0x67};
static const u8 sig_ctrpf[]        = {0x43, 0x54, 0x52, 0x50, 0x46};
static const u8 sig_ctrplugin[]    = {0x43, 0x54, 0x52, 0x50, 0x6C, 0x75, 0x67, 0x69, 0x6E};
static const u8 sig_3gx[]          = {0x33, 0x47, 0x58};
static const u8 sig_plg_hdr[]      = {0x50, 0x4C, 0x47, 0x00};
static const u8 sig_bootntr[]      = {0x42, 0x6F, 0x6F, 0x74, 0x4E, 0x54, 0x52};
static const u8 sig_luma_plg[]     = {0x6C, 0x75, 0x6D, 0x61, 0x2F, 0x70, 0x6C, 0x75, 0x67, 0x69, 0x6E, 0x73};
static const u8 sig_actionreplay[] = {0x41, 0x63, 0x74, 0x69, 0x6F, 0x6E, 0x52, 0x65, 0x70, 0x6C, 0x61, 0x79};
static const u8 sig_gateshark[]    = {0x47, 0x61, 0x74, 0x65, 0x73, 0x68, 0x61, 0x72, 0x6B};
static const u8 sig_pluginloader[] = {0x50, 0x6C, 0x75, 0x67, 0x69, 0x6E, 0x4C, 0x6F, 0x61, 0x64, 0x65, 0x72};

static const u8 sig_nimbus[]       = {0x4E, 0x69, 0x6D, 0x62, 0x75, 0x73};
static const u8 sig_pretendo[]     = {0x50, 0x72, 0x65, 0x74, 0x65, 0x6E, 0x64, 0x6F};
static const u8 sig_nimbus_plg[]   = {0x6E, 0x69, 0x6D, 0x62, 0x75, 0x73, 0x2E, 0x33, 0x67, 0x78};

static const AuspSignature ausp_signatures[] = {
    {sig_ntr_cfw,      sizeof(sig_ntr_cfw),      "NTR CFW",        0},
    {sig_ntr_plg,      sizeof(sig_ntr_plg),      "NTR PLG",        0},
    {sig_ntr_debug,    sizeof(sig_ntr_debug),    "NTR Debug",      0},
    {sig_ctrpf,        sizeof(sig_ctrpf),        "CTRPF",          0},
    {sig_ctrplugin,    sizeof(sig_ctrplugin),    "CTRPlugin",      0},
    {sig_3gx,          sizeof(sig_3gx),          "3GX",            0},
    {sig_plg_hdr,      sizeof(sig_plg_hdr),      "PLG Header",     0},
    {sig_bootntr,      sizeof(sig_bootntr),      "BootNTR",        0},
    {sig_luma_plg,     sizeof(sig_luma_plg),     "Luma Plugins",   0},
    {sig_actionreplay, sizeof(sig_actionreplay), "ActionReplay",   0},
    {sig_gateshark,    sizeof(sig_gateshark),    "Gateshark",      0},
    {sig_pluginloader, sizeof(sig_pluginloader), "PluginLoader",   0},

    {sig_nimbus,       sizeof(sig_nimbus),       "Nimbus",         1},
    {sig_pretendo,     sizeof(sig_pretendo),     "Pretendo",       1},
    {sig_nimbus_plg,   sizeof(sig_nimbus_plg),   "nimbus.3gx",     1},
};

#define AUSP_SIGNATURE_COUNT (sizeof(ausp_signatures) / sizeof(ausp_signatures[0]))

#endif
