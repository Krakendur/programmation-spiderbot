// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_INIT_H
#define UNI_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize Bluepad32.
 * argc / argv are parameters passed to the 'platform'
 */
int uni_init(int argc, const char** argv);

#ifdef __cplusplus
}
#endif

#endif  // UNI_INIT_H
