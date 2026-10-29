// Copyright 2026 Lanes Audio
// SPDX-License-Identifier: LicenseRef-Apache-2.0-with-Commons-Clause-1.0
#include "OptiLabCore.h"
#include <algorithm>
#include <cmath>
#include <string_view>
#include <stdexcept>

namespace {
static void applyStreamMatureState(OptiLabCoreEngine::ParameterValues& m) {
    m["p003"] = 1;
    m["p004"] = 4;
    m["p006"] = 1;
    m["p009"] = 20;
    m["p010"] = 0.2;
    m["p016"] = 6.1;
    m["p017"] = 78;
    m["p018"] = -35;
    m["p019"] = 50;
    m["p020"] = 55;
    m["p021"] = 14;
    m["p022"] = 60;
    m["p023"] = 3.6;
    m["p027"] = 76;
    m["p028"] = 42;
    m["p029"] = 50;
    m["p030"] = 74;
    m["p031"] = 12;
    m["p032"] = 40;
    m["p033"] = 40;
    m["p034"] = 6;
    m["p035"] = 3.2;
    m["p036"] = 0;
    m["p037"] = 84;
    m["p038"] = 50;
    m["p039"] = 60;
    m["p040"] = 24;
    m["p041"] = 10;
    m["p042"] = 2;
    m["p043"] = 2;
    m["p044"] = 51;
    m["p048"] = 4.2;
    m["p049"] = 18;
    m["p050"] = 68;
    m["p052"] = 20;
    m["p053"] = -2.6;
    m["p054"] = 150;
    m["p055"] = 3;
    m["p061"] = 0;
    m["p062"] = 0;
    m["p063"] = 46;
    m["p064"] = 26;
    m["p065"] = 100;
    m["p069"] = -1.4;
    m["p079"] = 0;
    m["p080"] = 0;
    m["c17"] = 100;
    m["c04"] = 50;
    m["c29"] = 0;
    m["c08"] = -14;
    m["c33"] = 18;
    m["c12"] = 18;
    m["c01"] = 200;
    m["c26"] = 0;
    m["c15"] = 65;
    m["c35"] = 1.4;
    m["c06"] = 1;
    m["c22"] = 3;
    m["c10"] = 100;
    m["c03"] = 0.73;
    m["c27"] = 0;
    m["c14"] = 1;
    m["c36"] = 92;
    m["c05"] = 200;
    m["c19"] = 50;
    m["c25"] = 0.54;
    m["c02"] = 0.1;
    m["c30"] = 35;
    m["c11"] = 100;
    m["c34"] = 6;
    m["c07"] = 2;
    m["c21"] = 2;
    m["c16"] = 30;
    m["c28"] = 8;
    m["c09"] = -20;
    m["c32"] = -16;
    m["c13"] = 0.97;
    m["c24"] = 0;
    m["c18"] = 0;
    m["c20"] = 0;
    m["c23"] = 2;
    m["c31"] = 0;
}

static void applyStreamLowState(OptiLabCoreEngine::ParameterValues& m) {
    // Released Core v1.4.0 Stream polish at Auto-Adapt 0.  Keep this endpoint
    // independently authored: the bottom of Auto-Adapt should retain the
    // familiar open/released Core character rather than inherit the newer
    // mature Stream architecture and merely back a few controls down.
    m["p003"] = 1;
    m["p004"] = 5;
    m["c17"] = 100;
    m["p006"] = 0;
    m["c04"] = 50;
    m["c29"] = 0;
    m["p009"] = 56;
    m["p010"] = 0;
    m["c08"] = -17;
    m["c33"] = 14;
    m["c12"] = 14;
    m["c01"] = 200;
    m["c26"] = 0;
    m["p016"] = 5;
    m["p017"] = 78;
    m["p018"] = -30;
    m["p019"] = 50;
    m["p020"] = 55;
    m["p021"] = 12;
    m["p022"] = 50;
    m["p023"] = 3.6;
    m["c15"] = 65;
    m["c35"] = 1.4;
    m["c06"] = 0;
    m["p027"] = 76;
    m["p028"] = 42;
    m["p029"] = 66;
    m["p030"] = 74;
    m["p031"] = 12;
    m["p032"] = 65;
    m["p033"] = 32;
    m["p034"] = 6;
    m["p035"] = 3.5;
    m["p036"] = 0;
    m["p037"] = 84;
    m["p038"] = 50;
    m["p039"] = 0;
    m["p040"] = 16;
    m["p041"] = 10;
    m["p042"] = 2.3;
    m["p043"] = 3.2;
    m["p044"] = 50;
    m["c22"] = 2;
    m["c10"] = 100;
    m["c31"] = 0;
    m["p048"] = -0.8;
    m["p049"] = 46;
    m["p050"] = 20;
    m["c03"] = 0.73;
    m["p052"] = 12;
    m["p053"] = 0;
    m["p054"] = 100;
    m["p055"] = 3;
    m["c27"] = 0;
    m["c14"] = 1;
    m["c36"] = 92;
    m["c05"] = 200;
    m["c19"] = 50;
    m["p061"] = 0;
    m["p062"] = 2;
    m["p063"] = 44;
    m["p064"] = 38;
    m["p065"] = 100;
    m["c25"] = 0.54;
    m["c02"] = 0.8;
    m["c30"] = 45;
    m["p069"] = -0.8;
    m["c11"] = 100;
    m["c34"] = 6;
    m["c07"] = 2;
    m["c21"] = 2;
    m["c16"] = 30;
    m["c28"] = 8;
    m["c09"] = -20;
    m["c32"] = -16;
    m["c13"] = 0.97;
    m["p079"] = 0;
    m["p080"] = 0;
    m["c24"] = 0;
    m["c18"] = 0;
    m["c20"] = 0;
    m["c23"] = 2;
}

static void applyStreamExtremeState(OptiLabCoreEngine::ParameterValues& m) {
    m["p003"] = 2;
    m["p004"] = 1;
    m["c17"] = 150;
    m["p006"] = 1;
    m["c04"] = 50;
    m["c29"] = 1;
    m["p009"] = 50;
    m["p010"] = 0.2;
    m["c08"] = -10;
    m["c33"] = 18;
    m["c12"] = 24;
    m["c01"] = 200;
    m["c26"] = 1;
    m["p016"] = 6.1;
    m["p017"] = 78;
    m["p018"] = -30.02;
    m["p019"] = 120;
    m["p020"] = 0;
    m["p021"] = 5;
    m["p022"] = 50;
    m["p023"] = 3;
    m["c15"] = 65;
    m["c35"] = 1.4;
    m["c06"] = 1;
    m["p027"] = 50;
    m["p028"] = 42;
    m["p029"] = 0;
    m["p030"] = 74;
    m["p031"] = 32;
    m["p032"] = 39;
    m["p033"] = 39.5;
    m["p034"] = 0.4;
    m["p035"] = -7.2;
    m["p036"] = 20;
    m["p037"] = 100;
    m["p038"] = 100;
    m["p039"] = 60;
    m["p040"] = 0;
    m["p041"] = 0;
    m["p042"] = 0.96;
    m["p043"] = 3.12;
    m["p044"] = 100;
    m["c22"] = 3;
    m["c10"] = 150;
    m["c31"] = 1;
    m["p048"] = -6;
    m["p049"] = 0;
    m["p050"] = 80;
    m["c03"] = 1.5;
    m["p052"] = 0;
    m["p053"] = 12;
    m["p054"] = 150;
    m["p055"] = 3;
    m["c27"] = 0;
    m["c14"] = 1;
    m["c36"] = 92;
    m["c05"] = 200;
    m["c19"] = 50;
    m["p061"] = 0;
    m["p062"] = 0;
    m["p063"] = 0;
    m["p064"] = 14;
    m["p065"] = 100;
    m["c25"] = 1.5;
    m["c02"] = 5;
    m["c30"] = 70;
    m["p069"] = -3.62;
    m["c11"] = 100;
    m["c34"] = 6;
    m["c07"] = 4;
    m["c21"] = 5;
    m["c16"] = 64;
    m["c28"] = 2;
    m["c09"] = -30;
    m["c32"] = -16.2;
    m["c13"] = 0.8;
    m["p079"] = 0;
    m["p080"] = 0;
    m["c24"] = 0;
    m["c18"] = 0;
    m["c20"] = 0;
    m["c23"] = 2;
}

static void applyPodcastLowerState(OptiLabCoreEngine::ParameterValues& m) {
    m["p003"] = 0;
    m["p004"] = 0;
    m["c17"] = 100;
    m["p006"] = 1;
    m["c04"] = 50;
    m["c29"] = 0;
    m["p009"] = 10;
    m["p010"] = 0.20000000000000001;
    m["c08"] = -14;
    m["c33"] = 14;
    m["c12"] = 14;
    m["c01"] = 200;
    m["c26"] = 0;
    m["p016"] = 1.3599999999999999;
    m["p017"] = 78;
    m["p018"] = -30;
    m["p019"] = 15;
    m["p020"] = 100;
    m["p021"] = 0;
    m["p022"] = 10;
    m["p023"] = 0;
    m["c15"] = 65;
    m["c35"] = 1.3999999999999999;
    m["c06"] = 1;
    m["p027"] = 76;
    m["p028"] = 42;
    m["p029"] = 0;
    m["p030"] = 74;
    m["p031"] = 12;
    m["p032"] = 39.5;
    m["p033"] = 10;
    m["p034"] = 6;
    m["p035"] = 3.2000000000000002;
    m["p036"] = 0;
    m["p037"] = 84;
    m["p038"] = 50;
    m["p039"] = 60;
    m["p040"] = 10;
    m["p041"] = 5.8000000000000052;
    m["p042"] = 1.3399999999999996;
    m["p043"] = 1.5;
    m["p044"] = 25;
    m["c22"] = 2;
    m["c10"] = 100;
    m["c31"] = 0;
    m["p048"] = 0.96000000000000119;
    m["p049"] = 0;
    m["p050"] = 20;
    m["c03"] = 0.17999999999999999;
    m["p052"] = 10;
    m["p053"] = -1.6400000000000001;
    m["p054"] = 100;
    m["p055"] = 3;
    m["c27"] = 0;
    m["c14"] = 1;
    m["c36"] = 92;
    m["c05"] = 200;
    m["c19"] = 50;
    m["p061"] = 0;
    m["p062"] = 2;
    m["p063"] = 46;
    m["p064"] = 100;
    m["p065"] = 100;
    m["c25"] = 0.35999999999999999;
    m["c02"] = 0.10000000000000001;
    m["c30"] = 35;
    m["p069"] = -0.049999999999999961;
    m["c11"] = 100;
    m["c34"] = 6;
    m["c07"] = 2;
    m["c21"] = 2;
    m["c16"] = 30;
    m["c28"] = 8;
    m["c09"] = -20;
    m["c32"] = -16;
    m["c13"] = 0.96999999999999997;
    m["p079"] = 0;
    m["p080"] = 0;
    m["c24"] = 0;
    m["c18"] = 0;
    m["c20"] = 0;
    m["c23"] = 2;
}

static void applyPodcastMiddleState(OptiLabCoreEngine::ParameterValues& m) {
    m["p003"] = 1;
    m["p004"] = 0;
    m["c17"] = 100;
    m["p006"] = 1;
    m["c04"] = 50;
    m["c29"] = 0;
    m["p009"] = 50;
    m["p010"] = 0.20000000000000001;
    m["c08"] = -14;
    m["c33"] = 14;
    m["c12"] = 14;
    m["c01"] = 200;
    m["c26"] = 0;
    m["p016"] = 6.0999999999999996;
    m["p017"] = 78;
    m["p018"] = -30;
    m["p019"] = 50;
    m["p020"] = 55;
    m["p021"] = 14;
    m["p022"] = 30;
    m["p023"] = 3.6000000000000001;
    m["c15"] = 65;
    m["c35"] = 1.3999999999999999;
    m["c06"] = 1;
    m["p027"] = 76;
    m["p028"] = 42;
    m["p029"] = 0;
    m["p030"] = 74;
    m["p031"] = 12;
    m["p032"] = 39.5;
    m["p033"] = 10;
    m["p034"] = 6;
    m["p035"] = 3.2000000000000002;
    m["p036"] = 0;
    m["p037"] = 84;
    m["p038"] = 50;
    m["p039"] = 60;
    m["p040"] = 10;
    m["p041"] = 5.8000000000000052;
    m["p042"] = 1.3399999999999996;
    m["p043"] = 1.5;
    m["p044"] = 25;
    m["c22"] = 2;
    m["c10"] = 100;
    m["c31"] = 0;
    m["p048"] = 0.96000000000000119;
    m["p049"] = 0;
    m["p050"] = 20;
    m["c03"] = 0.72999999999999998;
    m["p052"] = 10;
    m["p053"] = -1.6400000000000001;
    m["p054"] = 100;
    m["p055"] = 3;
    m["c27"] = 0;
    m["c14"] = 1;
    m["c36"] = 92;
    m["c05"] = 200;
    m["c19"] = 50;
    m["p061"] = 0;
    m["p062"] = 2;
    m["p063"] = 46;
    m["p064"] = 100;
    m["p065"] = 100;
    m["c25"] = 0.54000000000000004;
    m["c02"] = 0.10000000000000001;
    m["c30"] = 35;
    m["p069"] = -0.049999999999999961;
    m["c11"] = 100;
    m["c34"] = 6;
    m["c07"] = 2;
    m["c21"] = 2;
    m["c16"] = 30;
    m["c28"] = 8;
    m["c09"] = -20;
    m["c32"] = -16;
    m["c13"] = 0.96999999999999997;
    m["p079"] = 0;
    m["p080"] = 0;
    m["c24"] = 0;
    m["c18"] = 0;
    m["c20"] = 0;
    m["c23"] = 2;
}

static void applyPodcastUpperState(OptiLabCoreEngine::ParameterValues& m) {
    m["p003"] = 1;
    m["p004"] = 0;
    m["c17"] = 100;
    m["p006"] = 1;
    m["c04"] = 50;
    m["c29"] = 0;
    m["p009"] = 50;
    m["p010"] = 0.20000000000000001;
    m["c08"] = -14;
    m["c33"] = 14;
    m["c12"] = 14;
    m["c01"] = 200;
    m["c26"] = 0;
    m["p016"] = 6.0999999999999996;
    m["p017"] = 100;
    m["p018"] = -30;
    m["p019"] = 10;
    m["p020"] = 55;
    m["p021"] = 0;
    m["p022"] = 30;
    m["p023"] = 3.6000000000000001;
    m["c15"] = 65;
    m["c35"] = 1.3999999999999999;
    m["c06"] = 1;
    m["p027"] = 76;
    m["p028"] = 42;
    m["p029"] = 0;
    m["p030"] = 74;
    m["p031"] = 12;
    m["p032"] = 39.5;
    m["p033"] = 10;
    m["p034"] = 6;
    m["p035"] = 3.2000000000000002;
    m["p036"] = 0;
    m["p037"] = 100;
    m["p038"] = 50;
    m["p039"] = 60;
    m["p040"] = 10;
    m["p041"] = 5.8000000000000052;
    m["p042"] = 2.54;
    m["p043"] = 3.0480000000000005;
    m["p044"] = 25;
    m["c22"] = 3;
    m["c10"] = 150;
    m["c31"] = 0;
    m["p048"] = 0.96000000000000119;
    m["p049"] = 0;
    m["p050"] = 20;
    m["c03"] = 0.72999999999999998;
    m["p052"] = 10;
    m["p053"] = 2.6319999999999997;
    m["p054"] = 100;
    m["p055"] = 3;
    m["c27"] = 0;
    m["c14"] = 1;
    m["c36"] = 92;
    m["c05"] = 200;
    m["c19"] = 50;
    m["p061"] = 0;
    m["p062"] = 2;
    m["p063"] = 46;
    m["p064"] = 100;
    m["p065"] = 100;
    m["c25"] = 0.54000000000000004;
    m["c02"] = 0.10000000000000001;
    m["c30"] = 35;
    m["p069"] = -0.07999999999999996;
    m["c11"] = 100;
    m["c34"] = 6;
    m["c07"] = 2;
    m["c21"] = 2;
    m["c16"] = 30;
    m["c28"] = 8;
    m["c09"] = -20;
    m["c32"] = -16;
    m["c13"] = 0.96999999999999997;
    m["p079"] = 0;
    m["p080"] = 1;
    m["c24"] = 0;
    m["c18"] = 0;
    m["c20"] = 0;
    m["c23"] = 2;
}

static void applySmoothLowerState(OptiLabCoreEngine::ParameterValues& m) {
    m["p003"] = 2;
    m["p004"] = 0;
    m["p006"] = 0;
    m["p009"] = 0;
    m["p010"] = 0;
    m["p016"] = 6.5;
    m["p017"] = 0;
    m["p018"] = -70;
    m["p019"] = 50;
    m["p020"] = 0;
    m["p021"] = 0;
    m["p022"] = 0;
    m["p023"] = 0;
    m["p027"] = 0;
    m["p028"] = 0;
    m["p029"] = 0;
    m["p030"] = 0;
    m["p031"] = 0;
    m["p032"] = 0;
    m["p033"] = 25;
    m["p034"] = 0;
    m["p035"] = -0.40000000000000002;
    m["p036"] = 0;
    m["p037"] = 10;
    m["p038"] = 50;
    m["p039"] = 0;
    m["p040"] = 20;
    m["p041"] = 6;
    m["p042"] = 0;
    m["p043"] = 0;
    m["p044"] = 0;
    m["p048"] = 0;
    m["p049"] = 0;
    m["p050"] = 32;
    m["p052"] = 12;
    m["p053"] = -1.6000000000000001;
    m["p054"] = 100;
    m["p055"] = 3;
    m["p061"] = 0;
    m["p062"] = 0;
    m["p063"] = 100;
    m["p064"] = 60;
    m["p065"] = 100;
    m["p069"] = -0.55999999999999983;
    m["p079"] = 0;
    m["p080"] = 0;
    m["c17"] = 100;
    m["c04"] = 50;
    m["c29"] = 0;
    m["c08"] = -17;
    m["c33"] = 9;
    m["c12"] = 14;
    m["c01"] = 200;
    m["c26"] = 0;
    m["c15"] = 65;
    m["c35"] = 1.3999999999999999;
    m["c06"] = 0;
    m["c22"] = 2;
    m["c10"] = 100;
    m["c31"] = 0;
    m["c03"] = 1.5;
    m["c27"] = 0;
    m["c14"] = 1;
    m["c36"] = 92;
    m["c05"] = 200;
    m["c19"] = 50;
    m["c25"] = 1.5;
    m["c02"] = 0.80000000000000004;
    m["c30"] = 45;
    m["c11"] = 100;
    m["c34"] = 6;
    m["c07"] = 2;
    m["c21"] = 2;
    m["c16"] = 30;
    m["c28"] = 8;
    m["c09"] = -20;
    m["c32"] = -16;
    m["c13"] = 0.96999999999999997;
    m["c24"] = 0;
    m["c18"] = 0;
    m["c20"] = 0;
    m["c23"] = 2;
}

static void applySmoothUpperState(OptiLabCoreEngine::ParameterValues& m) {
    m["p003"] = 2;
    m["p004"] = 2;
    m["p006"] = 0;
    m["p009"] = 0;
    m["p010"] = 0;
    m["p016"] = 6.2000000000000002;
    m["p017"] = 35;
    m["p018"] = -70;
    m["p019"] = 50;
    m["p020"] = 0;
    m["p021"] = 0;
    m["p022"] = 0;
    m["p023"] = 0;
    m["p027"] = 0;
    m["p028"] = 8;
    m["p029"] = 35;
    m["p030"] = 55;
    m["p031"] = 0;
    m["p032"] = 0;
    m["p033"] = 8;
    m["p034"] = 0;
    m["p035"] = 2;
    m["p036"] = 0;
    m["p037"] = 48;
    m["p038"] = 14;
    m["p039"] = 100;
    m["p040"] = 14;
    m["p041"] = 10;
    m["p042"] = 0;
    m["p043"] = 0;
    m["p044"] = 0;
    m["p048"] = 0;
    m["p049"] = 60;
    m["p050"] = 92;
    m["p052"] = 34;
    m["p053"] = -0.40000000000000002;
    m["p054"] = 100;
    m["p055"] = 3;
    m["p061"] = 0;
    m["p062"] = 0;
    m["p063"] = 42;
    m["p064"] = 42;
    m["p065"] = 100;
    m["p069"] = -0.039999999999999869;
    m["p079"] = 0;
    m["p080"] = 0;
    m["c17"] = 100;
    m["c04"] = 50;
    m["c29"] = 0;
    m["c08"] = -17;
    m["c33"] = 9;
    m["c12"] = 14;
    m["c01"] = 200;
    m["c26"] = 0;
    m["c15"] = 65;
    m["c35"] = 1.3999999999999999;
    m["c06"] = 0;
    m["c22"] = 2;
    m["c10"] = 100;
    m["c31"] = 0;
    m["c03"] = 1.5;
    m["c27"] = 0;
    m["c14"] = 1;
    m["c36"] = 92;
    m["c05"] = 200;
    m["c19"] = 50;
    m["c25"] = 1.5;
    m["c02"] = 0.80000000000000004;
    m["c30"] = 45;
    m["c11"] = 100;
    m["c34"] = 6;
    m["c07"] = 2;
    m["c21"] = 2;
    m["c16"] = 30;
    m["c28"] = 8;
    m["c09"] = -20;
    m["c32"] = -16;
    m["c13"] = 0.96999999999999997;
    m["c24"] = 0;
    m["c18"] = 0;
    m["c20"] = 0;
    m["c23"] = 2;
}


static void applySmoothMaximumState(OptiLabCoreEngine::ParameterValues& m) {
    applySmoothUpperState(m);
    m["p079"] = -0.10000000000000001;
    m["c02"] = 5;
    m["c07"] = 2.02;
    m["c21"] = 5;
    m["c28"] = 2.3999999999999999;
    m["c09"] = -6;
    m["c32"] = -12;
    m["c13"] = 0.999;
}

bool isNonInterpolatedKey(std::string_view k) {
    if (k == "p001" || k == "p002") return true;
    // Private tuning retains its structural switching policy after leaving the public catalog.
    if (OptiLabCoreEngine::isCoreInternalKey(k)) return true;
    for (const auto& info : OptiLabCoreEngine::parameterInfo())
        if (k == info.key) return info.integer || info.structural();
    return false;
}

}

double OptiLabCore::clamp(double x,double lo,double hi) noexcept { return std::max(lo,std::min(hi,x)); }
double OptiLabCore::smoothstep(double x) noexcept { x=clamp(x,0.0,1.0); return x*x*(3.0-2.0*x); }


OptiLabCore::Mode OptiLabCore::sanitizeMode(int value) noexcept {
    return value >= 0 && value <= 2 ? static_cast<Mode>(value) : Mode::StreamPolish;
}

OptiLabCore::Parameters OptiLabCore::defaultParameters(Mode mode) noexcept {
    mode = sanitizeMode(static_cast<int>(mode));
    Parameters p; p.mode=mode; p.autoAdaptPct=0.0;
    p.inputDriveDb = (mode==Mode::PodcastLeveler) ? 3.5 : 0.0;
    return p;
}

void OptiLabCore::prepare(double sampleRate) {
    if (!OptiLabCoreEngine::isSampleRateSupported(sampleRate))
        throw std::invalid_argument("Unsupported OptiLab Core sample rate");
    sampleRate_=sampleRate;
    producer_.prepare(sampleRate_); scratch_.prepare(sampleRate_); prepared_=true;
    rebuildEndpoints();
    const auto sameKeys = [](const Values& a, const Values& b) {
        if (a.size() != b.size()) return false;
        for (const auto& [key, value] : a) {
            (void)value;
            if (!b.contains(key)) return false;
        }
        return true;
    };
    if (!sameKeys(stream_.low, streamMature_) || !sameKeys(stream_.low, stream_.high)
        || !sameKeys(stream_.low, podcast_.low) || !sameKeys(stream_.low, podcastMiddle_) || !sameKeys(stream_.low, podcast_.high)
        || !sameKeys(stream_.low, smooth_.low) || !sameKeys(stream_.low, smoothMiddle_) || !sameKeys(stream_.low, smooth_.high)) {
        throw std::logic_error("OptiLab Core endpoint parameter sets do not match");
    }
    workingValues_ = stream_.low;
    // Enroll every key before activation; live updates only replace stored numbers.
    OptiLabCorePipeline::Parameters initial;
    initial.preset = OptiLabCorePipeline::Preset::StreamPolish;
    initial.overrideInputTrim = true;
    initial.parameterOverrides = workingValues_;
    producer_.setParameters(initial);
    const auto maxLatency = *std::max_element(modeLatencyTarget_.begin(), modeLatencyTarget_.end());
    latencyDelayL_.assign(maxLatency + 4, 0.0f);
    latencyDelayR_.assign(maxLatency + 4, 0.0f);
    latencyWrite_ = 0;
    applyCurrentParameters();
    reset();
}

OptiLabCore::Values OptiLabCore::makePresetValues(OptiLabCorePipeline::Preset preset,const Values& overrides) {
    Values clean;
    for (const auto& [k,v] : overrides)
        if (OptiLabCoreEngine::isCoreValueSupported(k)) clean[k]=v;
    OptiLabCorePipeline::Parameters p; p.preset=preset; p.overrideInputTrim=true; p.p002=0.0; p.parameterOverrides=std::move(clean);
    scratch_.setParameters(p);
    return scratch_.effectiveCoreValues();
}

void OptiLabCore::rebuildEndpoints() {
    Values o;
    applyStreamLowState(o);
    stream_.low=makePresetValues(OptiLabCorePipeline::Preset::StreamPolish,o);

    o.clear();
    applyStreamMatureState(o);
    streamMature_=makePresetValues(OptiLabCorePipeline::Preset::StreamUpper,o);

    o.clear();
    applyStreamExtremeState(o);
    stream_.high=makePresetValues(OptiLabCorePipeline::Preset::StreamUpper,o);

    o.clear(); applyPodcastLowerState(o);
    podcast_.low=makePresetValues(OptiLabCorePipeline::Preset::PodcastLeveler,o);
    o.clear(); applyPodcastMiddleState(o);
    podcastMiddle_=makePresetValues(OptiLabCorePipeline::Preset::PodcastLeveler,o);

    o.clear(); applyPodcastUpperState(o);
    podcast_.high=makePresetValues(OptiLabCorePipeline::Preset::PodcastLeveler,o);

    o.clear(); applySmoothLowerState(o);
    smooth_.low=makePresetValues(OptiLabCorePipeline::Preset::SmoothBase,o);
    o.clear(); applySmoothUpperState(o);
    smoothMiddle_=makePresetValues(OptiLabCorePipeline::Preset::SmoothUpper,o);
    o.clear(); applySmoothMaximumState(o);
    smooth_.high=makePresetValues(OptiLabCorePipeline::Preset::SmoothUpper,o);

    const auto latencyFor = [&](OptiLabCorePipeline::Preset preset, const Values& v) {
        OptiLabCorePipeline::Parameters p;
        p.preset=preset; p.overrideInputTrim=true; p.p002=0.0; p.parameterOverrides=v;
        scratch_.setParameters(p);
        return scratch_.latencySamples();
    };
    modeLatencyTarget_[static_cast<std::size_t>(Mode::StreamPolish)] =
        std::max({latencyFor(OptiLabCorePipeline::Preset::StreamPolish, stream_.low),
                  latencyFor(OptiLabCorePipeline::Preset::StreamUpper, streamMature_),
                  latencyFor(OptiLabCorePipeline::Preset::StreamUpper, stream_.high)});
    modeLatencyTarget_[static_cast<std::size_t>(Mode::PodcastLeveler)] =
        std::max({latencyFor(OptiLabCorePipeline::Preset::PodcastLeveler, podcast_.low),
                  latencyFor(OptiLabCorePipeline::Preset::PodcastLeveler, podcastMiddle_),
                  latencyFor(OptiLabCorePipeline::Preset::StreamUpper, podcast_.high)});
    modeLatencyTarget_[static_cast<std::size_t>(Mode::SmoothLimiter)] =
        std::max({latencyFor(OptiLabCorePipeline::Preset::SmoothBase, smooth_.low),
                  latencyFor(OptiLabCorePipeline::Preset::SmoothUpper, smoothMiddle_),
                  latencyFor(OptiLabCorePipeline::Preset::SmoothUpper, smooth_.high)});
}

void OptiLabCore::interpolateInto(Values& dst,const Values& lo,const Values& hi,double t,double adapt,Mode mode) {
    if(mode==Mode::SmoothLimiter) {
        const auto& lower=adapt<=90.0?lo:smoothMiddle_;
        const auto& upper=adapt<=90.0?smoothMiddle_:hi;
        const double weight=smoothstep(adapt<=90.0?adapt/90.0:(adapt-90.0)/10.0);
        for(const auto& [key,lv]:lower) {
            if(key=="p001" || key=="p002") continue;
            const double hv=upper.at(key);
            // Preserve the old trajectory below 90; the new segment only changes
            // continuous tuning, with zero slope at both ends of the handoff.
            dst[key]=adapt<=90.0 && isNonInterpolatedKey(key)
                ? (weight<0.72?lv:hv) : lv+(hv-lv)*weight;
        }
        return;
    }
    const double normal = smoothstep(t);

    if(mode==Mode::PodcastLeveler) {
        const auto& lower=adapt<=50.0?lo:podcastMiddle_;
        const auto& upper=adapt<=50.0?podcastMiddle_:hi;
        const double weight=smoothstep(adapt<=50.0?adapt/50.0:(adapt-50.0)/50.0);
        for(const auto& [key,lv]:lower) {
            if(key=="p001" || key=="p002") continue;
            const double hv=upper.at(key);
            const bool continuousTiming=key=="c03" || key=="c25";
            dst[key]=isNonInterpolatedKey(key) && !continuousTiming
                ? (weight<0.72?lv:hv) : lv+(hv-lv)*weight;
        }
        dst["p009"]=adapt<=50.0?10.0+40.0*smoothstep((adapt-20.0)/30.0):50.0;
        dst["c22"]=adapt<70.0?2.0:3.0;
        dst["p080"]=adapt<90.0?0.0:1.0;
        if(adapt>50.0) {
            dst["c10"]=std::floor(100.0+50.0*smoothstep((adapt-50.0)/20.0)+0.5);
            dst["p053"]=podcastMiddle_.at("p053")+(hi.at("p053")-podcastMiddle_.at("p053"))
                *smoothstep((adapt-70.0)/30.0);
        }
        return;
    }

    if(mode==Mode::StreamPolish) {
        const double foundation=smoothstep(clamp(adapt/70.0,0.0,1.0));
        const double prep=smoothstep(clamp((adapt-70.0)/18.0,0.0,1.0));
        const double clean=smoothstep(clamp((adapt-70.0)/14.0,0.0,1.0));
        const double push=smoothstep(clamp((adapt-84.0)/16.0,0.0,1.0));
        // As the late Final-threshold push begins, withdraw the earlier high-band
        // sweetening so added loudness does not stack on top of Presence/Air.
        // Reach neutral tone before the aggressive end of the threshold ramp.
        const double lateToneWithdraw=smoothstep(clamp((adapt-84.0)/6.0,0.0,1.0));

        auto cleanBy84 = [](std::string_view k) noexcept {
            if((k >= "p022" && k <= "p034") || k=="c15" || k=="c35" || k=="c06") return true;
            if(k=="p036" || k=="p037" || k=="p038" || k=="p041"
                || k=="p042" || k=="p043" || k=="p044" || k=="c22"
                || k=="c10" || k=="c31" || k=="p048" || k=="p049"
                || k=="p050" || k=="p052" || k=="p054") return true;
            return false;
        };
        auto pushAbove84 = [](std::string_view k) noexcept {
            if(k=="p035" || k=="p053") return true;
            // Renamed private values keep their original late-Final grouping.
            return (k >= "p062" && k <= "p079")
                || k=="c25" || k=="c02" || k=="c30" || k=="c11"
                || k=="c34" || k=="c07" || k=="c21" || k=="c16"
                || k=="c28" || k=="c09" || k=="c32" || k=="c13";
        };

        for(const auto& [k,lv]:lo) {
            auto mi=streamMature_.find(k);
            const double mv=(mi==streamMature_.end()?lv:mi->second);
            if(k=="p001" || k=="p002" || !OptiLabCoreEngine::isCoreValueSupported(k)) continue;
            double v;
            if(isNonInterpolatedKey(k)) v=(foundation<0.72?lv:mv);
            else v=lv+(mv-lv)*foundation;
            dst[k]=v;
        }
        for(const auto& [k,mv]:streamMature_)
            if(!lo.contains(k) && k!="p001" && k!="p002" && OptiLabCoreEngine::isCoreValueSupported(k))
                dst[k]=mv;

        if(adapt>70.0) {
            for(const auto& [k,hv]:hi) {
                if(k=="p001" || k=="p002" || !OptiLabCoreEngine::isCoreValueSupported(k)) continue;
                const double mv=streamMature_.contains(k)?streamMature_.at(k):(dst.contains(k)?dst[k]:hv);
                const double weight=cleanBy84(k)?clean:(pushAbove84(k)?push:prep);
                if(isNonInterpolatedKey(k)) {
                    // The late Final architecture changes as one controlled regime.
                    // Switch it when the HF sweetening has fully withdrawn rather
                    // than letting the old timing ride into the loudest late points.
                    const bool lateFinalStructural = pushAbove84(k);
                    dst[k]=(lateFinalStructural ? (adapt<90.0?mv:hv) : (weight<0.72?mv:hv));
                } else dst[k]=mv+(hv-mv)*weight;
            }
        }

        if(adapt>84.0) {
            dst["p042"] *= (1.0-lateToneWithdraw);
            dst["p043"] *= (1.0-lateToneWithdraw);
        }


        if(adapt>70.0) {
            const double lateHandoff=adapt<=84.0?0.0:push;
            for(const char* key : {"p023","p029","p037","p038","c10"})
                dst[key]=streamMature_.at(key)+(hi.at(key)-streamMature_.at(key))*lateHandoff;
        }

        const double cleanOutputAnchorDb=2.25;
        const double makeupPrep=smoothstep(clamp((adapt-70.0)/8.0,0.0,1.0));
        const double pushMakeup=std::pow(push,1.60);
        if(adapt>70.0 && adapt<=84.0)
            dst["p053"]=streamMature_.at("p053")+(cleanOutputAnchorDb-streamMature_.at("p053"))*makeupPrep;
        else if(adapt>84.0)
            dst["p053"]=cleanOutputAnchorDb+(hi.at("p053")-cleanOutputAnchorDb)*pushMakeup;

        // 0..70 inherits the independently authored low->mature interpolation.
        // Above 70, preserve the approved late AGC-authority rise.
        if(adapt>70.0)
            dst["p009"] = 20.0 + 30.0*smoothstep(clamp((adapt-70.0)/18.0,0.0,1.0));
        if(adapt>=88.0) dst["p009"]=50.0;
        if(adapt<=10.0) dst["p009"]=20.0+30.0*smoothstep(adapt/10.0);
        else if(adapt<20.0) {
            const double join=56.0+(20.0-56.0)*smoothstep(20.0/70.0);
            dst["p009"]=50.0+(join-50.0)*smoothstep((adapt-10.0)/10.0);
        }
        return;
    }

    for(const auto& [k,lv]:lo) {
        auto it=hi.find(k); const double hv=(it==hi.end()?lv:it->second);
        if(k=="p001" || k=="p002" || !OptiLabCoreEngine::isCoreValueSupported(k)) continue;
        double v;
        if(isNonInterpolatedKey(k)) v=(normal<0.72?lv:hv);
        else v=lv+(hv-lv)*normal;
        dst[k]=v;
    }
    for(const auto& [k,hv]:hi)
        if(!lo.contains(k) && k!="p001" && k!="p002" && OptiLabCoreEngine::isCoreValueSupported(k))
            dst[k]=hv;


}
void OptiLabCore::setParameters(const Parameters& p) {
    const auto previousMode = params_.mode;
    params_=p;
    params_.mode=sanitizeMode(static_cast<int>(p.mode));
    params_.inputDriveDb=std::isfinite(p.inputDriveDb)?clamp(p.inputDriveDb,-18.0,18.0):0.0;
    params_.autoAdaptPct=std::isfinite(p.autoAdaptPct)?clamp(p.autoAdaptPct,0.0,100.0):0.0;
    if(prepared_) {
        applyCurrentParameters();
        if(previousMode != params_.mode || !hasProcessed_) reset();
    }
}

void OptiLabCore::applyCurrentParameters() {
    const double t=params_.autoAdaptPct/100.0; Values& values=workingValues_; OptiLabCorePipeline::Preset base=OptiLabCorePipeline::Preset::StreamPolish;
    if(params_.mode==Mode::StreamPolish) { interpolateInto(values,stream_.low,stream_.high,t,params_.autoAdaptPct,params_.mode); base=OptiLabCorePipeline::Preset::StreamPolish; }
    else if(params_.mode==Mode::PodcastLeveler) { interpolateInto(values,podcast_.low,podcast_.high,t,params_.autoAdaptPct,params_.mode); base=OptiLabCorePipeline::Preset::PodcastLeveler; }
    else { interpolateInto(values,smooth_.low,smooth_.high,t,params_.autoAdaptPct,params_.mode); base=OptiLabCorePipeline::Preset::SmoothBase; }
    producer_.setPreparedParameters(base, values, params_.inputDriveDb);
    updateLatencyCompensation();
}

OptiLabCore::Activity OptiLabCore::activity() const noexcept {
    auto a=producer_.activity(); Activity out; out.agcGain=a.agcGain; out.densityGain=a.densityGain; out.finalGain=a.finalGain; return out;
}
void OptiLabCore::setActivityTracking(bool enabled) noexcept { producer_.setActivityTracking(enabled); }
void OptiLabCore::updateLatencyCompensation() noexcept {
    if(!prepared_) return;
    const auto target = modeLatencyTarget_[static_cast<std::size_t>(params_.mode)];
    const auto internal = producer_.latencySamples();
    const auto needed = target > internal ? target - internal : 0;
    if(needed != compensationDelay_) {
        previousCompensationDelay_ = compensationDelay_;
        compensationDelay_ = needed;
        compensationFadeRemaining_ = compensationFadeLength_;
    }
}

std::pair<float,float> OptiLabCore::applyLatencyCompensation(float left,float right) noexcept {
    if(latencyDelayL_.empty()) return {left,right};
    const auto size = latencyDelayL_.size();
    latencyDelayL_[latencyWrite_] = left;
    latencyDelayR_[latencyWrite_] = right;
    auto readTap = [&](const std::vector<float>& buf, std::size_t delay) {
        const auto d = std::min(delay, size-1);
        const auto idx = (latencyWrite_ + size - d) % size;
        return buf[idx];
    };
    float outL = readTap(latencyDelayL_, compensationDelay_);
    float outR = readTap(latencyDelayR_, compensationDelay_);
    if(compensationFadeRemaining_ > 0) {
        const double x = 1.0 - double(compensationFadeRemaining_) / double(compensationFadeLength_);
        const float oldL = readTap(latencyDelayL_, previousCompensationDelay_);
        const float oldR = readTap(latencyDelayR_, previousCompensationDelay_);
        outL = float(oldL + (outL-oldL)*x);
        outR = float(oldR + (outR-oldR)*x);
        --compensationFadeRemaining_;
    }
    latencyWrite_ = (latencyWrite_ + 1) % size;
    return {outL,outR};
}

std::size_t OptiLabCore::latencySamples() const noexcept {
    return modeLatencyTarget_[static_cast<std::size_t>(params_.mode)];
}
void OptiLabCore::reset(){
    hasProcessed_=false;
    producer_.reset();
    std::fill(latencyDelayL_.begin(), latencyDelayL_.end(), 0.0f);
    std::fill(latencyDelayR_.begin(), latencyDelayR_.end(), 0.0f);
    latencyWrite_=0;
    previousCompensationDelay_=compensationDelay_;
    compensationFadeRemaining_=0;
}
std::pair<float,float> OptiLabCore::processSample(float l,float r){
    hasProcessed_=true;
    auto y=producer_.processSample(l,r);
    return applyLatencyCompensation(y.first,y.second);
}
void OptiLabCore::processPlanar(float* l,float* r,std::size_t n){
    for(std::size_t i=0;i<n;++i){auto o=processSample(l[i],r[i]);l[i]=o.first;r[i]=o.second;}
}
void OptiLabCore::processInterleaved(float* s,std::size_t n,std::size_t c){
    if(c==0) return;
    for(std::size_t i=0;i<n;++i){
        const float l=s[i*c];
        const float r=c>1?s[i*c+1]:l;
        auto o=processSample(l,r);
        s[i*c]=o.first;
        if(c>1) s[i*c+1]=o.second;
    }
}
